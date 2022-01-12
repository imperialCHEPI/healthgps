#include "pch.h"

#include "HealthGPS/event_bus.h"
#include "HealthGPS/info_message.h"
#include "HealthGPS/runner_message.h"
#include "HealthGPS/result_message.h"
#include "HealthGPS/error_message.h"

struct TestHandler {
	void handler_event(std::shared_ptr<hgps::EventMessage> message) {
		counter++;
	}

	int counter{};
};

static int global_counter = 0;
static void free_handler_event(std::shared_ptr<hgps::EventMessage> message) {
	global_counter++;
}

TEST(TestHealthGPS_EventBus, CreateHandlerIdentifier)
{
	using namespace hgps;

	auto identifier = EventHandlerIdentifier{ "ded69078-723b-4aa4-8c4d-df646be2f75b" };

	ASSERT_FALSE(identifier.str().empty());
	ASSERT_GT(identifier.str().length(), 0);
}

TEST(TestHealthGPS_EventBus, EmptyHandlerIdentifierThrow)
{
	using namespace hgps;

	ASSERT_THROW(EventHandlerIdentifier{ "" }, std::invalid_argument);
	ASSERT_THROW(EventHandlerIdentifier{ std::string{} }, std::invalid_argument);
}

TEST(TestHealthGPS_EventBus, CreateDefaultEmpty)
{
	using namespace hgps;

	auto hub = DefaultEventBus{};
	ASSERT_EQ(0, hub.count());
}

TEST(TestHealthGPS_EventBus, CreateEventSubscriberHandler)
{
	using namespace hgps;

	auto identifier = EventHandlerIdentifier{ "ded69078-723b-4aa4-8c4d-df646be2f75b" };
	auto hub = DefaultEventBus{};
	auto handler = EventSubscriberHandler(identifier, &hub);

	ASSERT_EQ(identifier.str() , handler.id());
}

TEST(TestHealthGPS_EventBus, CreateEventSubscriberWithNullBusThrow)
{
	using namespace hgps;

	auto identifier = EventHandlerIdentifier{ "ded69078-723b-4aa4-8c4d-df646be2f75b" };
	ASSERT_THROW(auto handler = EventSubscriberHandler(identifier, nullptr), std::invalid_argument);
}

TEST(TestHealthGPS_EventBus, AddEventSubscribers)
{
	using namespace hgps;
	using namespace std::placeholders;

	auto expected = 2;
	auto message = InfoEventMessage{ "UnitTest", ModelAction::start, 1, 2010 };

	auto counter = 0;
	auto handler = TestHandler{};
	auto member_callback = std::bind(&TestHandler::handler_event, &handler, _1);

	auto hub = DefaultEventBus{};
	auto free_sub = hub.subscribe(EventType::info, free_handler_event);
	auto fun_sub = hub.subscribe(EventType::info, member_callback);
	// GCC lambda optimization bug, optimized out.
	//auto lam_sub = hub.subscribe(EventType::info,
	//	[&counter](std::shared_ptr<hgps::EventMessage> msg) { counter++; });

	ASSERT_EQ(expected, hub.count());

	hub.unsubscribe(*free_sub);
	fun_sub->unregister();
	//lam_sub->~EventSubscriber();
	ASSERT_EQ(0, hub.count());
}

TEST(TestHealthGPS_EventBus, HandlerAutoUnsubscribe)
{
	using namespace hgps;
	using namespace std::placeholders;

	auto expected = 3;
	auto message = InfoEventMessage{ "UnitTest", ModelAction::start, 1, 2010 };

	auto counter = 0;
	auto handler = TestHandler{};
	auto member_callback = std::bind(&TestHandler::handler_event, &handler, _1);

	auto hub = DefaultEventBus{};
	{
		auto free_sub = hub.subscribe(EventType::info, free_handler_event);
		auto fun_sub = hub.subscribe(EventType::info, member_callback);
		auto lam_sub = hub.subscribe(EventType::info,
			[&counter](std::shared_ptr<hgps::EventMessage> msg) { counter++; });

		ASSERT_EQ(expected, hub.count());
	}

	ASSERT_EQ(0, hub.count());
}

TEST(TestHealthGPS_EventBus, ContainerAutoUnsubscribe)
{
	using namespace hgps;
	using namespace std::placeholders;

	auto expected = 3;
	auto message = InfoEventMessage{ "UnitTest", ModelAction::start, 1, 2010 };

	auto counter = 0;
	auto handler = TestHandler{};
	auto member_callback = std::bind(&TestHandler::handler_event, &handler, _1);

	std::vector<std::unique_ptr<EventSubscriber>> subscribers;

	auto hub = DefaultEventBus{};
	subscribers.push_back(hub.subscribe(EventType::info, free_handler_event));
	subscribers.push_back(hub.subscribe(EventType::info, member_callback));
	subscribers.push_back(hub.subscribe(EventType::info,
		[&counter](std::shared_ptr<hgps::EventMessage> msg) {counter++; }));
	
	ASSERT_EQ(expected, hub.count());
	ASSERT_EQ(expected, subscribers.size());

	subscribers.clear();
	ASSERT_EQ(0, hub.count());
	ASSERT_EQ(0, subscribers.size());
}

TEST(TestHealthGPS_EventBus, ClearUnsubscribes)
{
	using namespace hgps;
	using namespace std::placeholders;

	auto expected = 3;
	auto message = InfoEventMessage{ "UnitTest", ModelAction::start, 1, 2010 };

	auto counter = 0;
	auto handler = TestHandler{};
	auto member_callback = std::bind(&TestHandler::handler_event, &handler, _1);

	auto hub = DefaultEventBus{};
	auto free_sub = hub.subscribe(EventType::info, free_handler_event);
	auto fun_sub = hub.subscribe(EventType::info, member_callback);
	auto lam_sub = hub.subscribe(EventType::info,
		[&counter](std::shared_ptr<hgps::EventMessage> msg) {counter++; });

	ASSERT_EQ(expected, hub.count());
	hub.clear();
	ASSERT_EQ(0, hub.count());
}

TEST(TestHealthGPS_EventBus, PublishToSubscribers)
{
	using namespace hgps;
	using namespace std::placeholders;

	auto counter = 0;
	auto expected = 2;
	auto handler = TestHandler{};
	auto callback = std::bind(&TestHandler::handler_event, &handler, _1);

	auto hub = DefaultEventBus{};
	auto fun_sub = hub.subscribe(EventType::info, callback);
	auto lam_sub = hub.subscribe(EventType::info, [&counter](std::shared_ptr<hgps::EventMessage> msg) {counter++; });
	hub.publish(std::make_unique<InfoEventMessage>("UnitTest", ModelAction::start, 1, 2010));
	hub.publish_async(std::make_unique<InfoEventMessage>("UnitTest", ModelAction::start, 2, 2010));

	ASSERT_EQ(expected, hub.count());
	ASSERT_EQ(expected, handler.counter);
	ASSERT_EQ(expected, counter);
}

TEST(TestHealthGPS_EventBus, PublishToFilteredSubscribers)
{
	using namespace hgps;
	using namespace std::placeholders;

	auto runner_count = 0;
	auto error_count = 0;
	auto result_count = 0;
	auto hub_expected = 4;
	auto count_expected = 2;

	auto info_handler = TestHandler{};
	auto info_callback = std::bind(&TestHandler::handler_event, &info_handler, _1);

	auto hub = DefaultEventBus{};
	auto info_sub = hub.subscribe(EventType::info, info_callback);
	auto run_sub = hub.subscribe(EventType::runner, 
		[&runner_count](std::shared_ptr<hgps::EventMessage> msg) {runner_count++; });
	auto err_sub = hub.subscribe(EventType::error,
		[&error_count](std::shared_ptr<hgps::EventMessage> msg) {error_count++; });
	auto result_sub = hub.subscribe(EventType::result,
		[&result_count](std::shared_ptr<hgps::EventMessage> msg) {result_count++; });

	hub.publish(std::make_unique<RunnerEventMessage>("UnitTest", RunnerAction::start));
	hub.publish_async(std::make_unique<InfoEventMessage>("UnitTest", ModelAction::start, 1, 0));
	hub.publish_async(std::make_unique<ResultEventMessage>("UnitTest", 1, 2010, ModelResult{}));
	hub.publish_async(std::make_unique<ErrorEventMessage>("UnitTest", 1, 2010, "fell from world's edge"));

	hub.publish_async(std::make_unique<InfoEventMessage>("UnitTest", ModelAction::start, 2, 0));
	hub.publish_async(std::make_unique<ResultEventMessage>("UnitTest", 2, 2030, ModelResult{}));
	hub.publish_async(std::make_unique<ErrorEventMessage>("UnitTest", 2, 2030, "fell from world's edge"));
	hub.publish(std::make_unique<RunnerEventMessage>("UnitTest", RunnerAction::finish));

	ASSERT_EQ(hub_expected, hub.count());
	ASSERT_EQ(count_expected, info_handler.counter);
	ASSERT_EQ(count_expected, runner_count);
	ASSERT_EQ(count_expected, error_count);
	ASSERT_EQ(count_expected, result_count);
}