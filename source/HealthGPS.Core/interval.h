#pragma once
#include <format>
#include "forward_type.h"
#include "string_util.h"

namespace hgps {
	namespace core {

		template <Numerical TYPE>
		class Interval {
		public:
			Interval() = default;

			explicit Interval(TYPE lower_value, TYPE upper_value) 
				: lower_{ lower_value }, upper_{ upper_value }{}

			TYPE lower() const { return lower_; }

			TYPE upper() const { return upper_; }

			TYPE length() const { return upper_ - lower_; }

			bool contains(TYPE value) { lower_ <= value && value <= upper_; }

			bool contains(Interval<TYPE>& other) { is_inside(other.lower_) && is_inside(other.upper_); }

			std::string to_string() const noexcept { return std::format("{}-{}", lower_, upper_); }

			auto operator<=>(const Interval<TYPE>&) const = default;
		private:
			TYPE lower_;
			TYPE upper_;
		};

		// Pre-define intervals
		using IntegerInterval = Interval<int>;
		using FloatInterval = Interval<float>;
		using DoubleInterval = Interval<double>;

		IntegerInterval parse_integer_interval(const std::string_view& value, const std::string_view delims = "-");
	
		FloatInterval parse_float_interval(const std::string_view& value, const std::string_view delims = "-");

		DoubleInterval parse_double_interval(const std::string_view& value, const std::string_view delims = "-");
	}
}