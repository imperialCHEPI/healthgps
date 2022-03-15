#include "runtime_metric.h"
#include <algorithm>

namespace hgps {
	std::size_t RuntimeMetric::size() const noexcept {
		return metrics_.size();
	}

	double& RuntimeMetric::operator[](const std::string metric_key) {
		return metrics_[metric_key];
	}

	double& RuntimeMetric::at(const std::string metric_key) {
		return metrics_.at(metric_key);
	}

	const double& RuntimeMetric::at(const std::string metric_key) const {
		return metrics_.at(metric_key);
	}

	bool RuntimeMetric::contains(const std::string metric_key) const noexcept {
		return metrics_.contains(metric_key);
	}

	bool RuntimeMetric::emplace(const std::string metric_key, const double value) {
		auto sucess = metrics_.emplace(metric_key, value);
		return sucess.second;
	}

	bool RuntimeMetric::erase(const std::string metric_key) {
		auto sucess = metrics_.erase(metric_key);
		return sucess > 0;
	}

	void RuntimeMetric::clear() noexcept {
		metrics_.clear();
	}

	void RuntimeMetric::reset() noexcept {
		for (auto& m : metrics_) {
			m.second = 0.0;
		}
	}

	bool RuntimeMetric::empty() const noexcept {
		return metrics_.empty();
	}
}
