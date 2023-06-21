#pragma once
#include <functional>

namespace hgps {

/// @brief Define a scoped reset state data type
/// @tparam F The function to reset
template <typename F> class Finally {
  public:
    /// @brief Initialises a new instance of the Finally class
    /// @tparam Func Functor type
    /// @param func The functor instance
    template <typename Func> Finally(Func &&func) : functor_(std::forward<Func>(func)) {}

    Finally(const Finally &) = delete;
    Finally(Finally &&) = delete;
    Finally &operator=(const Finally &) = delete;
    Finally &operator=(Finally &&) = delete;

    /// @brief Destroys a Finally instance
    ~Finally() {
        try {
            functor_();
        } catch (...) {
        }
    }

  private:
    F functor_;
};

/// @brief Makes a finally instance
/// @tparam F Functor type
/// @param f The functor instance
/// @return The new Finally instance
template <typename F> Finally<F> make_finally(F &&f) { return {std::forward<F>(f)}; }
} // namespace hgps
