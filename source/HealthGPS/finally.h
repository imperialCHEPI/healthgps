#pragma once
#include <functional>

namespace hgps {

    template <typename F>
    class Finally {
    public:
        template <typename Func>
        Finally(Func&& func) 
            : functor_(std::forward<Func>(func)) 
        {}
        Finally(const Finally&) = delete;
        Finally(Finally&&) = delete;
        Finally& operator =(const Finally&) = delete;
        Finally& operator =(Finally&&) = delete;

        ~Finally() {
            try {
                functor_(); 
            }
            catch (...){}
        }

    private:
        F functor_;
    };

    template <typename F>
    Finally<F> make_finally(F&& f)
    {
        return { std::forward<F>(f) };
    }
}
