#pragma once

#include <mutex>
#include <queue>
#include <atomic>
#include <optional>
#include <condition_variable>

namespace hgps {

	/// @brief Thread-safe communication channel data type
	/// 
	/// @details This channel has been designed specific to enable
	/// in-memory communication from the baseline to the intervention
	/// scenarios simulations. This is a hard modelling requirement,
	/// non-memory communication such as data streaming might offer
	/// more scalable alternative, including the option of running a
	/// single baseline and multiple interventions in parallel.
	/// 
	/// @note Messages in the channel are kept in a FIFO queue to 
	/// guarantee receive order for the intervention algorithm.
	/// 
	/// @tparam T The channel data type
	template <typename T>
	class Channel {
	public:
		using value_type = T;
		using size_type = std::size_t;

		/// @brief Initialises a new instance of the Channel class
		/// @param capacity The channel capacity, if fixed; otherwise, unlimited
		explicit Channel(size_type capacity = 0) 
			: capacity_{ capacity }, is_closed_{ false } {}

		Channel(const Channel&) = delete;
		Channel& operator=(const Channel&) = delete;
		Channel(Channel&&) = delete;
		Channel& operator=(Channel&&) = delete;

		/// @brief Destroys a Channel instance
		virtual ~Channel() = default;

		/// @brief Sends a new message through the channel by reference
		/// @param message The message instance to send 
		/// @return true, the operation succeeded; otherwise false
		bool send(const value_type& message) { return do_send(message); }

		/// @brief Sends a new message through the channel
		/// @param message The message instance to send
		/// @return true, the operation succeeded; otherwise false
		bool send(value_type&& message) { return do_send(std::move(message)); }

		/// @brief Try to receive a message from the channel
		/// @param timeout_millis Max wait timeout, or zero for infinity
		/// @return The received message, if arrived; otherwise empty.
		std::optional<value_type> try_receive(int timeout_millis = 0) {
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

		/// @brief Gets the current channel size, number of messages.
		/// @return The channel size
		[[nodiscard]]
		size_type constexpr size() const noexcept {
			return buffer_.size();
		}

		/// @brief Determine whether the channel is empty
		/// @return true, if there are no messages; otherwise, false.
		[[nodiscard]]
		bool constexpr empty() const noexcept {
			return buffer_.empty();
		}

		/// @brief Close the channel, no new messages are accepted.
		void close() noexcept {
			cond_var_.notify_one();
			is_closed_.store(true);
		}

		/// @brief Determine whether the channel is closed
		/// @return true, if the channel is closed; otherwise, false.
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
