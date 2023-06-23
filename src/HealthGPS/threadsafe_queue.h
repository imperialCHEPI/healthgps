#pragma once
#include <mutex>
#include <optional>
#include <queue>

namespace hgps {
/// @brief Defines a thread-safe queue data structure
/// @tparam T Type of the queue data
template <typename T> class ThreadsafeQueue {
  public:
    /// @brief Initialise a new instance of the ThreadsafeQueue class.
    ThreadsafeQueue() = default;

    ThreadsafeQueue(const ThreadsafeQueue<T> &) = delete;
    ThreadsafeQueue &operator=(const ThreadsafeQueue<T> &) = delete;

    /// @brief Move-constructs the underlying container using move semantics
    /// @param other The other instance to move
    ThreadsafeQueue(ThreadsafeQueue<T> &&other) {
        std::unique_lock<std::mutex> lck(mutex_);
        queue_ = std::move(other.queue_);
    }

    /// @brief Destroys a ThreadsafeQueue instance
    virtual ~ThreadsafeQueue() {}

    /// @brief Gets the number of elements in the underlying container
    /// @return The number of elements in the queue.
    std::size_t size() const {
        std::unique_lock<std::mutex> lck(mutex_);
        return queue_.size();
    }

    /// @brief Removes an element from the front of the queue
    /// @return The removed element, if found; otherwise, empty.
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lck(mutex_);
        if (queue_.empty()) {
            return {};
        }

        T tmp = std::move(queue_.front());
        queue_.pop();
        return tmp;
    }

    /// @brief Pushes the given element value to the end of the queue.
    /// @param item the value of the element to push
    void push(T &&item) { do_push(std::move(item)); }

    /// @brief Pushes the given element value to the end of the queue.
    /// @param item the value of the element to push
    void push(const T &item) { do_push(item); }

  private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;

    void do_push(auto &&item) {
        std::unique_lock<std::mutex> lck(mutex_);
        queue_.push(std::forward<decltype(item)>(item));
    }
};
} // namespace hgps
