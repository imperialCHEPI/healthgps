#pragma once
#include <utility>

namespace hgps {
	template<typename TYPE>
	struct TwoStepValue {
        TwoStepValue() = default;
		TwoStepValue(TYPE value) 
			: value_{ value }, old_value_{}{}

        TYPE value() const { return value_; }
        TYPE old_value() const { return old_value_; }

        void set_both_values(TYPE new_value) {
            value_ = new_value;
            old_value_ = new_value;
        }

        void set_value(TYPE new_value) {
            old_value_ = std::exchange(value_, new_value);
        }

        TYPE operator()() const { return value_; }
        void operator=(TYPE new_value) { set_value(new_value); }

        TwoStepValue<TYPE> clone() {
            auto clone = TwoStepValue<TYPE>{ old_value() };
            clone = value();
            return clone;
        }
        
	private:
		TYPE value_{};
		TYPE old_value_{};
	};
}
