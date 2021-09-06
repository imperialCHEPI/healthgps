#pragma once
#include <queue>
#include <mutex>

namespace hgps {
	template<typename T>
	class ThreadsafeQueue {
	public:
		ThreadsafeQueue() = default;
		ThreadsafeQueue(const ThreadsafeQueue<T>&) = delete;
		ThreadsafeQueue& operator=(const ThreadsafeQueue<T>&) = delete;

		ThreadsafeQueue(ThreadsafeQueue<T>&& other) {
			std::unique_lock<std::mutex> lck(mutex_);
			queue_ = std::move(other.queue_);
		}

		virtual ~ThreadsafeQueue() { }

		std::size_t size() const {
			std::unique_lock<std::mutex> lck(mutex_);
			return queue_.size();
		}

		std::optional<T> pop() {
			std::unique_lock<std::mutex> lck(mutex_);
			if (queue_.empty()) {
				return {};
			}

			T tmp = queue_.front();
			queue_.pop();
			return tmp;
		}

		void push(const T& item) {
			std::unique_lock<std::mutex> lck(mutex_);
			queue_.push(item);
		}

	private:
		std::queue<T> queue_;
		mutable std::mutex mutex_;
	};
}
