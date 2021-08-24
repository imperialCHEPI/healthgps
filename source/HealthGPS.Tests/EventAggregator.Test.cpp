#include "pch.h"

#include "HealthGPS\event_bus.h"

struct InfoMessage final : public hgps::EventMessage
{
	InfoMessage(std::string sender)
		: source{ sender } {}

	const std::string source;

	int id() const noexcept override {
		return static_cast<int>(hgps::EventType::info);
	}

	std::string to_string() const override {
		return std::format("{},{}", id(), source);
	}
};

struct TestHandler {
	void handler_event(const hgps::EventMessage& message) {
		counter++;
	}

	int counter{};
};

static int global_counter = 0;
static void free_handler_event(const hgps::EventMessage& message) {
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

	auto expected = 3;
	auto message = InfoMessage{ "UnitTest" };

	auto counter = 0;
	auto handler = TestHandler{};
	auto member_callback = std::bind(&TestHandler::handler_event, &handler, _1);

	auto hub = DefaultEventBus{};
	auto free_sub = hub.subscribe(EventType::info, free_handler_event);
	auto fun_sub = hub.subscribe(EventType::info, member_callback);
	auto lam_sub = hub.subscribe(EventType::info, [&counter](const EventMessage& msg) {counter++; });

	ASSERT_EQ(expected, hub.count());

	hub.unsubscribe(*free_sub);
	fun_sub->unregister();
	lam_sub->~EventSubscriber();
	ASSERT_EQ(0, hub.count());
}

TEST(TestHealthGPS_EventBus, HandlerAutoUnsubscribe)
{
	using namespace hgps;
	using namespace std::placeholders;

	auto expected = 3;
	auto message = InfoMessage{ "UnitTest" };

	auto counter = 0;
	auto handler = TestHandler{};
	auto member_callback = std::bind(&TestHandler::handler_event, &handler, _1);

	auto hub = DefaultEventBus{};
	{
		auto free_sub = hub.subscribe(EventType::info, free_handler_event);
		auto fun_sub = hub.subscribe(EventType::info, member_callback);
		auto lam_sub = hub.subscribe(EventType::info, [&counter](const EventMessage& msg) {counter++; });

		ASSERT_EQ(expected, hub.count());
	}

	ASSERT_EQ(0, hub.count());
}

TEST(TestHealthGPS_EventBus, ContainerAutoUnsubscribe)
{
	using namespace hgps;
	using namespace std::placeholders;

	auto expected = 3;
	auto message = InfoMessage{ "UnitTest" };

	auto counter = 0;
	auto handler = TestHandler{};
	auto member_callback = std::bind(&TestHandler::handler_event, &handler, _1);

	std::vector<std::unique_ptr<EventSubscriber>> subscribers;

	auto hub = DefaultEventBus{};
	subscribers.push_back(hub.subscribe(EventType::info, free_handler_event));
	subscribers.push_back(hub.subscribe(EventType::info, member_callback));
	subscribers.push_back(hub.subscribe(EventType::info,
		[&counter](const EventMessage& msg) {counter++; }));
	
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
	auto message = InfoMessage{ "UnitTest" };

	auto counter = 0;
	auto handler = TestHandler{};
	auto member_callback = std::bind(&TestHandler::handler_event, &handler, _1);

	auto hub = DefaultEventBus{};
	auto free_sub = hub.subscribe(EventType::info, free_handler_event);
	auto fun_sub = hub.subscribe(EventType::info, member_callback);
	auto lam_sub = hub.subscribe(EventType::info, [&counter](const EventMessage& msg) {counter++; });

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
	auto message = InfoMessage{ "UnitTest" };

	auto handler = TestHandler{};
	auto callback = std::bind(&TestHandler::handler_event, &handler, _1);

	auto hub = DefaultEventBus{};
	auto fun_sub = hub.subscribe(EventType::info, callback);
	auto lam_sub = hub.subscribe(EventType::info, [&counter](const EventMessage& msg) {counter++; });
	hub.publish(message);
	hub.publish_async(message);

	ASSERT_EQ(expected, hub.count());
	ASSERT_EQ(expected, handler.counter);
	ASSERT_EQ(expected, counter);
}