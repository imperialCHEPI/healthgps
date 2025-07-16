#pragma once

#include "population.h"
#include "runtime_context.h"
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/parallel_for_each.h>
#include <oneapi/tbb/blocked_range.h>
#include <vector>
#include <functional>

namespace hgps {

/// @brief Configuration for performance optimizations
struct OptimizationConfig {
    /// @brief Minimum population size for parallel processing
    size_t parallel_threshold = 1000;
    
    /// @brief Chunk size for blocked range processing
    size_t chunk_size = 100;
    
    /// @brief Enable cache-friendly memory access patterns
    bool enable_cache_optimization = true;
    
    /// @brief Enable SIMD optimizations where possible
    bool enable_simd = true;
};

/// @brief Performance optimizer for population processing
class PerformanceOptimizer {
public:
    /// @brief Initialize with optimization configuration
    /// @param config Optimization settings
    explicit PerformanceOptimizer(const OptimizationConfig& config = {});

    /// @brief Optimized parallel processing of active population
    /// @param population The population to process
    /// @param func Function to apply to each active person
    template<typename Function>
    void parallel_process_active(Population& population, Function&& func);

    /// @brief Optimized parallel processing with blocked ranges
    /// @param population The population to process
    /// @param func Function to apply to range of people
    template<typename Function>
    void parallel_process_blocked(Population& population, Function&& func);

    /// @brief Cache-optimized iteration over population
    /// @param population The population to process
    /// @param func Function to apply to each person
    template<typename Function>
    void cache_optimized_process(Population& population, Function&& func);

    /// @brief Get active people as contiguous vector for better cache performance
    /// @param population The source population
    /// @return Vector of references to active people
    std::vector<std::reference_wrapper<Person>> get_active_people(Population& population);

    /// @brief Batch process people by age groups for cache efficiency
    /// @param population The population to process
    /// @param func Function to apply to each person
    template<typename Function>
    void age_grouped_process(Population& population, Function&& func);

private:
    OptimizationConfig config_;
    
    /// @brief Check if population size warrants parallel processing
    /// @param size Population size
    /// @return true if should use parallel processing
    bool should_parallelize(size_t size) const;
};

// Template implementations

template<typename Function>
void PerformanceOptimizer::parallel_process_active(Population& population, Function&& func) {
    if (!should_parallelize(population.size())) {
        // Use simple loop for small populations
        for (auto& person : population) {
            if (person.is_active()) {
                func(person);
            }
        }
        return;
    }

    if (config_.enable_cache_optimization) {
        // Get active people as contiguous vector for better cache performance
        auto active_people = get_active_people(population);
        
        tbb::parallel_for_each(active_people.begin(), active_people.end(),
            [&func](std::reference_wrapper<Person>& person_ref) {
                func(person_ref.get());
            });
    } else {
        // Standard parallel_for_each
        tbb::parallel_for_each(population.begin(), population.end(),
            [&func](Person& person) {
                if (person.is_active()) {
                    func(person);
                }
            });
    }
}

template<typename Function>
void PerformanceOptimizer::parallel_process_blocked(Population& population, Function&& func) {
    if (!should_parallelize(population.size())) {
        func(population.begin(), population.end());
        return;
    }

    tbb::parallel_for(
        tbb::blocked_range<Population::iterator>(population.begin(), population.end(), config_.chunk_size),
        [&func](const tbb::blocked_range<Population::iterator>& range) {
            func(range.begin(), range.end());
        });
}

template<typename Function>
void PerformanceOptimizer::cache_optimized_process(Population& population, Function&& func) {
    if (config_.enable_cache_optimization) {
        age_grouped_process(population, std::forward<Function>(func));
    } else {
        parallel_process_active(population, std::forward<Function>(func));
    }
}

template<typename Function>
void PerformanceOptimizer::age_grouped_process(Population& population, Function&& func) {
    // Group people by age for better cache locality
    std::unordered_map<int, std::vector<std::reference_wrapper<Person>>> age_groups;
    
    for (auto& person : population) {
        if (person.is_active()) {
            age_groups[person.age].emplace_back(person);
        }
    }
    
    // Process each age group in parallel
    tbb::parallel_for_each(age_groups.begin(), age_groups.end(),
        [&func](auto& age_group) {
            for (auto& person_ref : age_group.second) {
                func(person_ref.get());
            }
        });
}

/// @brief Optimized population statistics calculator
class OptimizedStatsCalculator {
public:
    /// @brief Calculate population statistics with optimization
    /// @param population The population to analyze
    /// @param optimizer Performance optimizer instance
    /// @return Statistics summary
    static std::map<std::string, double> calculate_stats(
        Population& population, 
        PerformanceOptimizer& optimizer);

    /// @brief Calculate risk factor statistics efficiently
    /// @param population The population to analyze
    /// @param risk_factors List of risk factors to calculate
    /// @param optimizer Performance optimizer instance
    /// @return Risk factor statistics
    static std::map<core::Identifier, std::map<std::string, double>> calculate_risk_factor_stats(
        Population& population,
        const std::vector<core::Identifier>& risk_factors,
        PerformanceOptimizer& optimizer);
};

} // namespace hgps 