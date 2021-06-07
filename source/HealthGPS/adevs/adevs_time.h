#ifndef _adevs_time_h_
#define _adevs_time_h_
#include <limits>

/**
 * \file adevs_time.h
 * Essential template functions for working with time.
 *
 * A TimeType must offer support for +, -, <, <=, adevs_inf(), adevs_zero(),
 * and adevs_epsilon().
 */

namespace adevs
{

/**
  * \brief This is the default super dense simulation time.
  * 
  * See the main page for a description of how the comparison, 
  * addition, and  subtraction (length) operations work. This
  * demonstrates the minumum requires for a type that is
  * used as time. The type Time is an alias for SuperDenseTime<int>.
  */
template <typename RealType>
class SuperDenseTime
{
	public:
		/// The real, physically meaningful part of time
		RealType real;
		/// The logical part of time for order at a real instant
		int logical;
		/// Default constructor
		SuperDenseTime(){}
		/// Constructor assigns initial values to real and logical
		SuperDenseTime(RealType real, int logical):real(real),logical(logical){}
		/** \brief Advance the time by h.
		  *
		  * Result is (real,logical)+(hreal,hlogical) = 
		  * if hreal > 0 then (real+hreal,hlogical) else
		  * (real,logical+hlogical). The pair (real,logical)
		  * is this object and (hreal,hlogical) is the object
		  * h. */
		SuperDenseTime<RealType> operator+(const SuperDenseTime<RealType>& h) const
		{
			return SuperDenseTime<RealType>(
				real+h.real,logical*(h.real==RealType(0))+h.logical);
		}
		/**
		 * \brief Get the length of the interval from preceding to this.
		 * The format for the call is big number - small number.
		 */
		SuperDenseTime<RealType> operator-(const SuperDenseTime<RealType>& preceding) const
		{
			if (real == preceding.real)
				return SuperDenseTime<RealType>(RealType(0),logical-preceding.logical);
			return SuperDenseTime<RealType>(real-preceding.real,logical);
		}
		/// Order is lexicographical favoring real then logical.
		bool operator<(const SuperDenseTime<RealType>& other) const
		{
			return (real < other.real) ||
				((real == other.real) && (logical < other.logical));
		}
		/// Order is lexicographical favoring real then logical.
		bool operator<=(const SuperDenseTime<RealType>& other) const
		{
			return (real < other.real) ||
				((real == other.real) && (logical <= other.logical));
		}
		/// Times are identical
		bool operator==(const SuperDenseTime<RealType>& other) const
		{
			return (real == other.real && logical == other.logical);
		}
};

/// Default type to use for time
typedef SuperDenseTime<int> Time;

};

/**
 * \brief Returns the maximum value for a time type
 *
 * For all t not inf(), t < inf()
 */
template <class T> inline T adevs_inf();
template <> inline adevs::SuperDenseTime<int> adevs_inf() {
	return adevs::SuperDenseTime<int>(
			std::numeric_limits<int>::max(),
			std::numeric_limits<int>::max());
}
template <class T> inline T adevs_inf();
template <> inline adevs::SuperDenseTime<long> adevs_inf() {
	return adevs::SuperDenseTime<long>(
			std::numeric_limits<long>::max(),
			std::numeric_limits<int>::max());
}
template <> inline adevs::SuperDenseTime<double> adevs_inf() {
	return adevs::SuperDenseTime<double>(
			std::numeric_limits<double>::infinity(),
			std::numeric_limits<int>::max());
}
template <> inline int adevs_inf() {
	return std::numeric_limits<int>::max();
}

/**
 * \brief Returns the zero value for a time type
 *
 * t + 0 = 0 + t = t
 */
template <typename T> inline T adevs_zero(); 
template <> inline adevs::SuperDenseTime<int> adevs_zero() {
	return adevs::SuperDenseTime<int>(0,0);
}
template <> inline adevs::SuperDenseTime<long> adevs_zero() {
	return adevs::SuperDenseTime<long>(0,0);
}
template <> inline adevs::SuperDenseTime<double> adevs_zero() {
	return adevs::SuperDenseTime<double>(0.0,0);
}
template <> inline int adevs_zero() { return 0; }
/**
 * \brief Returns the smallest increment of time
 *
 * For all h not zero and t, h < inf, t < t + h
 */
template <class T> inline T adevs_epsilon();
template <> inline adevs::SuperDenseTime<int> adevs_epsilon() {
	return adevs::SuperDenseTime<int>(0,1);
}
template <> inline adevs::SuperDenseTime<long> adevs_epsilon() {
	return adevs::SuperDenseTime<long>(0,1);
}
template <> inline adevs::SuperDenseTime<double> adevs_epsilon() {
	return adevs::SuperDenseTime<double>(0.0,1);
}
template <> inline int adevs_epsilon() { return 1; }

#endif
