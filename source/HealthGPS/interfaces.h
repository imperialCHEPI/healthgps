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

	/// @brief Simulation entity data structure
	struct Person;

	/// @brief Simulation run-time context for shared data and state.
	class RuntimeContext;

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
		virtual SimulationModuleType type() const noexcept = 0;

		/// @brief Gets the module name
		/// @return The human-readable module name
		virtual std::string name() const noexcept  = 0;

		/// @brief Initialises the virtual population
		/// @param context The simulation shared runtime context instance
		virtual void initialise_population(RuntimeContext& context) = 0;
	};

	/// @brief Generic disease module interface to host multiple diseases model
	class DiseaseHostModule : public SimulationModule
	{
	public:
		/// @brief Gets the number of diseases model hosted
		/// @return Number of hosted diseases models 
		virtual std::size_t size() const noexcept = 0;

		/// @brief Indicates whether the host contains an disease identified by code.
		/// @param disease_code The disease unique identification code 
		/// @return true if the disease is found, otherwise false.
		virtual bool contains(const std::string disease_code) const noexcept = 0;

		/// @brief Gets the excess mortality associated with a disease for an individual
		/// @param disease_code The disease unique identification code 
		/// @param age The reference age associated with the mortality
		/// @param gender The gender associated with the mortality
		/// @return the excess mortality value, if found, otherwise zero.
		virtual double get_excess_mortality(const std::string disease_code,
			const int& age, const core::Gender& gender) const noexcept = 0;

		/// @brief Updates the population diseases status: remission and incidence
		/// @param context The simulation run-time context
		virtual void update_population(RuntimeContext& context) = 0;
	};

	/// @brief Hierarchical linear model interface
	class HierarchicalLinearModel {
	public:
		virtual ~HierarchicalLinearModel() = default;

		/// @brief Gets the model type identifier
		/// @return The module type identifier
		virtual HierarchicalModelType type() const noexcept = 0;

		/// @brief Gets the model name
		/// @return The human-readable model name
		virtual std::string name() const noexcept = 0;

		/// @brief Generates the initial risk factors for a population and newborns
		/// @param context The simulation run-time context
		virtual void generate_risk_factors(RuntimeContext& context) = 0;

		/// @brief Update risk factors for population
		/// @param context The simulation run-time context
		virtual void update_risk_factors(RuntimeContext& context) = 0;

		/// @brief Adjust the risk factors using the baseline scenario
		/// @param context The simulation run-time context
		virtual void adjust_risk_factors_with_baseline(RuntimeContext& context) = 0;
	};

	/// @brief Diseases model interface
	class DiseaseModel
	{
	public:
		virtual ~DiseaseModel() = default;

		/// @brief Gets the model disease type unique identifier
		/// @return The disease type identifier
		virtual std::string disease_type() const noexcept = 0;

		/// @brief Initialises the population disease status.
		/// @param The simulation run-time context
		virtual void initialise_disease_status(RuntimeContext& context) = 0;

		/// @brief Initialises the average relative risks once all diseases status were initialised.
		/// @param The simulation run-time context
		virtual void initialise_average_relative_risk(RuntimeContext& context) = 0;

		/// @brief Updates the disease cases remission and incidence in the population
		/// @param The simulation run-time context
		virtual void update_disease_status(RuntimeContext& context) = 0;

		/// @brief Gets the excess mortality associated with a disease for an individual
		/// @param age The reference age associated with the mortality
		/// @param gender The gender associated with the mortality
		/// @return the excess mortality value, if found, otherwise zero.
		virtual double get_excess_mortality(const int& age, const core::Gender& gender) const noexcept = 0;
	};
}
