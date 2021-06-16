#pragma once

#include <limits>
#include <memory>

#include "HealthGPS.Core\api.h"

namespace hgps {

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

	/// @brief Simulation entity interface
	struct Entity
	{
		virtual ~Entity() = default;

		/// @brief Gets the entity unique identifier
		/// @return The entity identifier
		virtual int get_id() const = 0;

		virtual std::string to_string() const = 0;
	};

	/// @brief Health GPS simulation modules types enumeration
	enum class ModuleType : uint8_t
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

	/// @brief Simulation modules interface
	class Module
	{
	public:
		virtual ~Module() = default;

		/// @brief Gets the module type identifier
		/// @return The module type identifier
		virtual ModuleType type() const = 0;

		/// @brief Gets the module name
		/// @return The human-readable module name
		virtual std::string name() const = 0;

		virtual void execute(std::string_view command, std::vector<Entity>& entities) = 0;

		virtual void execute(std::string_view command, Entity& entity) = 0;
	};
}
