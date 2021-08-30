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

		Person& operator[](std::size_t index);

		const Person& operator[](std::size_t index) const;

		void add_newborn_babies(const int number, core::Gender gender) noexcept;

		IteratorType begin() noexcept { return people_.begin(); }
		IteratorType end() noexcept { return people_.end(); }

		ConstIteratorType cbegin() const noexcept { return people_.cbegin(); }
		ConstIteratorType cend() const noexcept { return people_.cend(); }

	private:
		std::vector<Person> people_;
	};
}