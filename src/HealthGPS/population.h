#pragma once
#include "person.h"

#include <vector>

namespace hgps {

/// @brief Defines the virtual population data type
///
/// @details The virtual population size is dynamic and can change due
/// to births, deaths and emigration. When possible, the Population class
/// recycles the dead and migrated individuals slots with newborn babies
/// to minimise memory allocation and clear-up the inactive population.
class Population {
  public:
    /// @brief Population iterator
    using IteratorType = std::vector<Person>::iterator;
    /// @brief Read-only population iterator
    using ConstIteratorType = std::vector<Person>::const_iterator;

    Population() = delete;
    /// @brief Initialises a new instance of the Population class
    /// @param size Initial population size
    explicit Population(const std::size_t size);

    /// @brief Gets the current size of the population
    /// @return Current population size
    std::size_t size() const noexcept;

    /// @brief Gets the initial size of the population
    /// @return Initial population size
    std::size_t initial_size() const noexcept;

    /// @brief Gets the current active size of the population
    /// @return Current active population size
    std::size_t current_active_size() const noexcept;

    /// @brief Gets a Person by index without bounds checking
    /// @param index The population index
    /// @return Reference to Person, out of bound access is undefined behaviour.
    Person &operator[](std::size_t index);

    /// @brief Gets a Person by index without bounds checking
    /// @param index The population index
    /// @return Reference to Person, out of bound access is undefined behaviour.
    const Person &operator[](std::size_t index) const;

    /// @brief Gets a Person by index with bounds checking
    /// @param index The population index
    /// @return Reference to Person
    /// @throws std::out_of_range for accessing outside of the bounds
    Person &at(std::size_t index);

    /// @brief Gets a Person by index with bounds checking
    /// @param index The population index
    /// @return Reference to Person
    /// @throws std::out_of_range for accessing outside of the bounds
    const Person &at(std::size_t index) const;

    /// @brief Adds a Person to the virtual population
    /// @param person The new Person instance
    /// @param time Current simulation time
    void add(Person person, unsigned int time) noexcept;

    /// @brief Adds newborn babies of gender to the virtual population, age = 0
    /// @param number The number of newborn babies to add
    /// @param gender The gender of the newborn babies
    /// @param time Current simulation time
    void add_newborn_babies(std::size_t number, core::Gender gender, unsigned int time) noexcept;

    /// @brief Gets an iterator to the beginning of the virtual population
    /// @return Iterator to the first Person
    IteratorType begin() noexcept { return people_.begin(); }

    /// @brief Gets an iterator to the element following the last Person of the population
    /// @return Iterator to the element following the last Person.
    IteratorType end() noexcept { return people_.end(); }

    /// @brief Gets an read-only iterator to the beginning of the virtual population
    /// @return Iterator to the first Person
    ConstIteratorType begin() const noexcept { return people_.cbegin(); }

    /// @brief Gets a read-only iterator to the element following the last Person of the population
    /// @return Iterator to the element following the last Person.
    ConstIteratorType end() const noexcept { return people_.cend(); }

    /// @brief Gets an read-only iterator to the beginning of the virtual population
    /// @return Iterator to the first Person
    ConstIteratorType cbegin() const noexcept { return people_.cbegin(); }

    /// @brief Gets a read-only iterator to the element following the last Person of the population
    /// @return Iterator to the element following the last Person.
    ConstIteratorType cend() const noexcept { return people_.cend(); }

    /// @brief Checks if the population is empty
    /// @return True if the population is empty, false otherwise
    bool empty() const noexcept { return people_.empty(); }

  private:
    std::size_t initial_size_;
    std::vector<Person> people_;

    std::vector<int> find_index_of_recyclables(unsigned int time,
                                               std::size_t top = 0) const noexcept;
};
} // namespace hgps
