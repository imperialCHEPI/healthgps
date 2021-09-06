#pragma once
#include <vector>

#include "person.h"
namespace hgps {

	class Population
	{
	public:
		using IteratorType = std::vector<Person>::iterator;
		using ConstIteratorType = std::vector<Person>::const_iterator;

		Population() = delete;
		explicit Population(const std::size_t size);

		std::size_t size() const noexcept;

		std::size_t initial_size() const noexcept;

		std::size_t current_active_size() const noexcept;

		Person& operator[](std::size_t index);

		const Person& operator[](std::size_t index) const;

		void add(Person&& person) noexcept;

		void add_newborn_babies(const int number, core::Gender gender) noexcept;

		IteratorType begin() noexcept { return people_.begin(); }
		IteratorType end() noexcept { return people_.end(); }

		ConstIteratorType cbegin() const noexcept { return people_.cbegin(); }
		ConstIteratorType cend() const noexcept { return people_.cend(); }

	private:
		std::size_t initial_size_;
		std::vector<Person> people_;
	};
}