#include <thread>
#include <future>

namespace hgps {
	namespace core {

		template<typename F, typename... Ts>
		auto run_async(F&& action, Ts&&... params)
		{
			return std::async(std::launch::async,
				std::forward<F>(action),
				std::forward<Ts>(params)...);
		};
	}
}