#include "pch.h"
#include <thread>

#include "HealthGPS\threadsafe_queue.h"

TEST(TestHealthGPS_Queue, CreateDefaultQueue)
{
	using namespace hgps;

	auto q = ThreadsafeQueue<int>{};

	ASSERT_EQ(0, q.size());
}

TEST(TestHealthGPS_Queue, ProducerConsumerQueue)
{
	using namespace hgps;
	using namespace::std::literals;

	auto q = ThreadsafeQueue<int>{};
	auto result = std::vector<int>{};

	auto producer = std::jthread([&q](std::stop_token token) {
		for (auto i : { 3, 5, 9, 11, 13, 15 }) {
			q.push(i);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			if (token.stop_requested()) { break; }
		}
	});

	auto consumer = std::jthread([&q, &result](std::stop_token token) {
		while (true) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			auto v = q.pop();
			if (!v.has_value()) {
				break;
			}

			result.push_back(v.value());
			if (token.stop_requested()) { break; }
		}});

	std::this_thread::sleep_for(std::chrono::seconds(1));
	producer.request_stop();
	consumer.request_stop();

	ASSERT_EQ(6, result.size());
	ASSERT_EQ(0, q.size());
}