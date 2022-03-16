#pragma once
#include <map>
#include <string>

namespace hgps {

	class RuntimeMetric
	{
	public:
		using IteratorType = std::map<std::string, double>::iterator;
		using ConstIteratorType = std::map<std::string, double>::const_iterator;

		std::size_t size() const noexcept;

		double& operator[](const std::string metric_key);

		double& at(const std::string metric_key);

		const double& at(const std::string metric_key) const;

		bool contains(const std::string metric_key) const noexcept;

		bool emplace(const std::string metric_key, const double value);

		bool erase(const std::string metric_key);

		void clear() noexcept;

		void reset() noexcept;

		bool empty() const noexcept;

		IteratorType begin() noexcept { return metrics_.begin(); }
		IteratorType end() noexcept { return metrics_.end(); }

		ConstIteratorType cbegin() const noexcept { return metrics_.cbegin(); }
		ConstIteratorType cend() const noexcept { return metrics_.cend(); }

	private:
		std::map<std::string, double> metrics_;
	};

}