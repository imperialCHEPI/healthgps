#pragma once
#include <utility>

namespace hgps {

	/// @brief Defines a two step value data type 
    /// @details Holds the current and previous value for algorithms requiring both values.
	/// @tparam TYPE The value type
	template<typename TYPE>
	struct TwoStepValue {
        /// @brief Initialise a new instance of the TwoStepValue type
        TwoStepValue() = default;

		/// @brief Initialise a new instance of the TwoStepValue type
		/// @param value Current value
		TwoStepValue(TYPE value) 
			: value_{ value }, old_value_{}{}

        /// @brief Gets the current value
        /// @return Current value
        TYPE value() const { return value_; }

        /// @brief Gets the previous value
        /// @return Previous value
        TYPE old_value() const { return old_value_; }

        /// @brief Sets both current and previous values to the same value
        /// @param new_value The new value
        void set_both_values(TYPE new_value) {
            value_ = new_value;
            old_value_ = new_value;
        }

        /// @brief Sets the current value
        /// @param new_value The new value
        void set_value(TYPE new_value) {
            old_value_ = std::exchange(value_, new_value);
        }

        /// @brief Gets the current value
        /// @return Current value
        TYPE operator()() const { return value_; }

        /// @brief Sets the current value
        /// @param new_value The new value
        void operator=(TYPE new_value) { set_value(new_value); }

        /// @brief Creates a new instance from this contents
        /// @return The cloned instance
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
