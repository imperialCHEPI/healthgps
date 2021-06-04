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
		virtual unsigned int get_id() = 0;
	};

	/// @brief Simulation modules interface
	class Module
	{
	public:
		virtual ~Module() = default;

		/// @brief Gets the kind of module unique identifier
		/// @return The module kind identifier
		virtual std::string get_kind() = 0;

		virtual void execute(std::string_view command, std::vector<Entity>& entities) = 0;

		virtual void execute(std::string_view command, Entity& entity) = 0;
	};

	/// @brief Simulation module builders interface
	class ModuleBuilder
	{
	public:
		virtual ~ModuleBuilder() = default;

		/// @brief Create a new instance of the module
		/// @param manager The data API to retrieve the modulo data 
		/// @return The module instance
		virtual std::unique_ptr<Module> Build(hgps::core::Datastore& manager) = 0;
	};
}
