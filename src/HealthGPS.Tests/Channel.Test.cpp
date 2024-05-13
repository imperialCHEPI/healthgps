#include "pch.h"

#include "HealthGPS/channel.h"
#include <oneapi/tbb/parallel_for_each.h>
#include <thread>

TEST(ChannelTest, DefaultConstruction) {
    using namespace hgps;

    auto channel = Channel<int>{};
    ASSERT_EQ(0, channel.size());
    ASSERT_TRUE(channel.empty());
    ASSERT_FALSE(channel.closed());
}

// NOLINTBEGIN(bugprone-unchecked-optional-access)
TEST(ChannelTest, SendAndReceive) {
    using namespace hgps;

    auto channel = Channel<int>{};
    channel.send(3);
    channel.send(7);

    ASSERT_EQ(2, channel.size());
    ASSERT_FALSE(channel.empty());
    ASSERT_FALSE(channel.closed());

    auto out = channel.try_receive();
    ASSERT_TRUE(out.has_value());
    ASSERT_EQ(3, out.value());

    out = channel.try_receive();
    ASSERT_TRUE(out.has_value());
    ASSERT_EQ(7, out.value());
}

TEST(ChannelTest, ReceiveTimeout) {
    using namespace hgps;

    auto timeout = 10;
    auto channel = Channel<int>{};
    channel.send(3);

    ASSERT_EQ(1, channel.size());
    ASSERT_FALSE(channel.empty());
    ASSERT_FALSE(channel.closed());

    auto out = channel.try_receive(timeout);
    ASSERT_TRUE(out.has_value());
    ASSERT_EQ(3, out.value());

    out = channel.try_receive(timeout);
    ASSERT_FALSE(out.has_value());
}

TEST(ChannelTest, SendByMoveAndReceive) {
    using namespace hgps;

    auto channel = Channel<std::string>{};

    auto fox = std::string{"fox"};
    channel.send(std::move(fox));
    channel.send(std::string{"dog"});

    auto out = channel.try_receive();
    ASSERT_TRUE(out.has_value());
    ASSERT_EQ("fox", out.value());

    out = channel.try_receive();
    ASSERT_TRUE(out.has_value());
    ASSERT_EQ("dog", out.value());
}

TEST(ChannelTest, ClosedWontSend) {
    using namespace hgps;

    auto channel = Channel<int>{};
    channel.close();

    ASSERT_TRUE(channel.empty());
    ASSERT_TRUE(channel.closed());
    ASSERT_FALSE(channel.send(5));
}

TEST(ChannelTest, ClosedWillReceive) {
    using namespace hgps;

    auto channel = Channel<int>{};
    channel.send(5);
    channel.send(9);
    channel.close();

    ASSERT_FALSE(channel.empty());
    ASSERT_TRUE(channel.closed());
    ASSERT_FALSE(channel.send(7));
    ASSERT_EQ(2, channel.size());
    ASSERT_EQ(5, channel.try_receive().value());
    ASSERT_EQ(9, channel.try_receive().value());
    ASSERT_TRUE(channel.empty());
}
// NOLINTEND(bugprone-unchecked-optional-access)

TEST(ChannelTest, ProducerConsummer) {
    using namespace hgps;

    auto channel = Channel<int>{10};
    std::vector<int> result;

    auto producer = std::thread{[&channel]() {
        for (auto i = 0; i < 10; ++i) {
            channel.send({i});
        }

        channel.close();
    }};

    auto consumer = std::thread{[&channel, &result]() {
        for (;;) {
            while (channel.empty() && !channel.closed()) {
                // busy wait
            }

            auto element = channel.try_receive();
            if (element.has_value()) {
                result.push_back(*element);
            } else {
                break;
            }
        }
    }};

    producer.join();
    consumer.join();

    ASSERT_EQ(std::vector<int>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}), result);
    ASSERT_TRUE(channel.closed());
}

TEST(ChannelTest, ConsummerProducer) {
    using namespace hgps;

    auto channel = Channel<int>{10};
    std::vector<int> result;

    auto consumer = std::thread{[&channel, &result]() {
        for (;;) {
            while (channel.empty() && !channel.closed()) {
                // busy wait
            }

            auto element = channel.try_receive();
            if (element.has_value()) {
                result.push_back(*element);
            } else {
                break;
            }
        }
    }};

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto producer = std::thread{[&channel]() {
        for (auto i = 0; i < 10; ++i) {
            channel.send({i});
        }

        channel.close();
    }};

    consumer.join();
    producer.join();

    ASSERT_EQ(std::vector<int>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}), result);
    ASSERT_TRUE(channel.closed());
}

TEST(ChannelTest, MultipleConsumers) {
    using namespace hgps;

    const int numbers = 100;
    const int expected = 5050;
    const int number_of_consumers = 10;

    auto channel = Channel<int>{5};

    std::mutex mtx_read;
    std::condition_variable cond_read;
    bool wait_to_read{true};
    std::atomic<int> count_reads{};
    std::atomic<int> sum_of_reads{};

    std::mutex mtx_wait;
    std::condition_variable cond_wait;
    std::atomic<int> wait_counter{};
    wait_counter.store(number_of_consumers);

    // Consume
    auto consumer = [&] {
        std::unique_lock<std::mutex> lock{mtx_read};
        cond_read.wait(lock, [&wait_to_read] { return !wait_to_read; });

        while (count_reads <= numbers) {
            auto out = channel.try_receive();
            sum_of_reads += out.value_or(0);
            ++count_reads;
        }

        --wait_counter;
        cond_wait.notify_one();
    };

    // Create multiple consumers
    std::vector<std::jthread> threads;
    threads.reserve(number_of_consumers);

    for (auto i = 0; i < number_of_consumers; ++i) {
        threads.emplace_back(consumer);
    }

    // Producer
    auto producer = std::jthread{[&]() {
        wait_to_read = false;
        cond_read.notify_all();
        for (auto i = 0; i <= numbers; ++i) {
            channel.send({i});
        }

        channel.close();
    }};

    // Wait until all items have been read
    std::unique_lock<std::mutex> lock{mtx_wait};
    cond_wait.wait(lock, [&wait_counter]() {
        auto items = wait_counter.load();
        return items == 0;
    });

    producer.join();
    for (auto &thread : threads) {
        thread.join();
    }

    ASSERT_EQ(expected, sum_of_reads);
}
