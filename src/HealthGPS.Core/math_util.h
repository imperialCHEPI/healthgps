#pragma once

namespace hgps {
    namespace core {
		/// @brief Additional mathematical functions and determines the parameters of
		///        the floating point representation.
		/// 
		/// References:
		/// - William Cody, Algorithm 665: MACHAR, a subroutine to dynamically determine
		///   machine parameters, ACM Transactions on Mathematical Software, Volume 14,
		///   Number 4, December 1988, pages 303-311.
		/// 
		/// - Didier H. Besset, Object-Oriented Implementation of Numerical Methods An
		///   Introduction with Smalltalk, Morgan Kaufmann, November 2000.
		class MathHelper {
		public:
			MathHelper() = delete;

			/// @brief Gets the machine radix used by floating-point numbers.
			/// @return The machine radix value
			static int radix() noexcept;

			/// @brief Gets the largest positive value which, when added to 1.0, yields 0.
			/// @return The machine precision value
			static double machine_precision() noexcept;

			/// @brief Gets the typical meaningful precision for numerical calculations.
			/// @return The default precision for numerical calculations
			static double default_numerical_precision() noexcept;

			/// @brief Compares two floating-point numbers for relative equality using
			///        the default numerical precision.
			/// @param left The left double to compare.
			/// @param right The right double to compare.
			/// @return <b>true</b> if the number are equal, otherwise. <b>false</b>
			static bool equal(double left, double right) noexcept;

			/// @brief Compares two floating-point numbers for relative equality.
			/// 
			/// Since the floating-point representation is keeping a constant relative
			/// precision, comparison must be made using relative error.
			/// 
			/// Let <c>a</c> and <c>b</c> be the two numbers to be compared, we build 
			/// the following expression:
			/// 
			/// <c>x = |a - b| / max(|a|, |b|)</c>
			/// 
			/// The two numbers can be considered equal if <c>x</c> is smaller 
			/// than a given number <c>x<sub>max</sub></c>. If the denominator of
			/// the fraction on the above equation is less than <c>x<sub>max</sub></c>,
			/// than the two numbers can be considered as being equal. For lack of information
			/// on how the numbers <c>a</c> and <c>b</c> have been obtained, one uses for
			/// <c>x<sub>max</sub></c> the <see cref="DefaultNumericalPrecision"/>.
			/// If one can determine the precision of each number, then this relative equality
			/// method can be used.
			///  
			/// @param left The left double to compare.
			/// @param right The right double to compare.
			/// @param precision The comparison precision.
			/// @return <b>true</b> if the number are equal, otherwise. <b>false</b>
			static bool equal(double left, double right, double precision) noexcept;

		private:
			/// @brief Radix used by floating-point numbers.
			static int radix_;

			/// @brief Largest positive value which, when added to 1.0, yields 0 - Epsilon.
			static double machine_precision_;

			/// @brief Typical meaningful precision for numerical calculations.
			static double numerical_precision_;

			/// @brief Calculates the machine radix.
			static void compute_radix() noexcept;

			/// @brief Calculates the machine precision.
			static void compute_machine_precision() noexcept;
		};
    }
}