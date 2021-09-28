#pragma once

#include <mutex>
#include <queue>
#include <atomic>
#include <optional>
#include <condition_variable>

namespace hgps {

	template <typename T>
	class Channel {
	public:
		using value_type = T;
		using size_type = std::size_t;

		explicit Channel(const size_type capacity = 0) 
			: capacity_{ capacity }, is_closed_{ false } {}

		Channel(const Channel&) = delete;
		Channel& operator=(const Channel&) = delete;
		Channel(Channel&&) = delete;
		Channel& operator=(Channel&&) = delete;
		virtual ~Channel() = default;

		bool send(const value_type& t) { return do_send(t); }
		bool send(value_type&& t) { return do_send(std::move(t)); }

		std::optional<value_type> try_receive(const int timeout_millis = 0) {
			std::unique_lock<std::mutex> lock{ mtx_ };

			if (timeout_millis <= 0) {
				cond_var_.wait(lock, [this] { return buffer_.size() > 0 || closed(); });
			}
			else {
				cond_var_.wait_for(lock, std::chrono::milliseconds(timeout_millis),
					[this] { return buffer_.size() > 0 || closed(); });
			}

			if (buffer_.empty()) {
				return {};
			}

			auto entry = std::move(buffer_.front());
			buffer_.pop();

			cond_var_.notify_one();
			return entry;
		}

		[[nodiscard]]
		size_type constexpr size() const noexcept {
			return buffer_.size();
		}

		[[nodiscard]]
		bool constexpr empty() const noexcept {
			return buffer_.empty();
		}

		void close() noexcept {
			cond_var_.notify_one();
			is_closed_.store(true);
		}

		[[nodiscard]]
		bool closed() const noexcept {
			return is_closed_.load();
		}

	private:
		const size_type capacity_;
		std::queue<value_type> buffer_;
		std::atomic<bool> is_closed_;
		std::condition_variable cond_var_;
		std::mutex mtx_;

		bool do_send(auto&& payload) {
			if (is_closed_.load()) {
				return false;
			}

			std::unique_lock<std::mutex> lock(mtx_);
			if (capacity_ > 0 && buffer_.size() >= capacity_) {
				cond_var_.wait(lock, [this]() { return buffer_.size() < capacity_; });
			}

			buffer_.push(std::forward<decltype(payload)>(payload));
			cond_var_.notify_one();
			return true;
		}
	};
}
