#pragma once

#include <map>
#include <limits>
#include <memory>

#include "HealthGPS.Core\api.h"

namespace hgps {

	/// @brief Defines a map template with case insensitive string keys and type.
	/// @tparam T The map value data type
	template <typename T>
	using case_insensitive_map = std::map<std::string, T, core::case_insensitive::comparator>;

	/// @brief Random number generator algorithms interface
	class RandomBitGenerator {
	public:
		using result_type = unsigned int;

		virtual ~RandomBitGenerator() = default;

		virtual unsigned int operator()() = 0;

		virtual double next_double() = 0;

		virtual void seed(unsigned int) = 0;

		virtual void discard(unsigned long long skip) = 0;

		static constexpr result_type min() { return std::numeric_limits<result_type>::min(); }
		static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }
	};

	/// @brief Health GPS simulation modules types enumeration
	enum class SimulationModuleType : uint8_t
	{
		/// @brief Health GPS microsimulation algorithm
		Simulator,
		
		/// @brief Risk factor module
		RiskFactor,

		/// @brief Socio-economic status module
		SES,

		/// @brief Intervention module (non-baseline).
		Intervention,

		/// @brief Demographic module
		Demographic,

		/// @brief Disease module coordinator
		Disease,

		/// @brief Statistical analysis module, e.g. BoD module
		Analysis,
	};

	/// @brief Health GPS risk factor module types enumeration
	enum class HierarchicalModelType : uint8_t
	{
		/// @brief Static hierarchical model
		Static,

		/// @brief Dynamic hierarchical model
		Dynamic,
	};

	/// @brief Simulation modules interface
	class SimulationModule
	{
	public:
		virtual ~SimulationModule() = default;

		/// @brief Gets the module type identifier
		/// @return The module type identifier
		virtual SimulationModuleType type() const = 0;

		/// @brief Gets the module name
		/// @return The human-readable module name
		virtual std::string name() const = 0;
	};
}
