#pragma once

#include <string>

namespace hgps {
	namespace core {

		struct Country
		{
			int code{};
			std::string name{};
			std::string alpha2{};
			std::string alpha3{};

			bool operator > (const Country& other) const { return (name > other.name); }
			bool operator < (const Country& other) const { return (name < other.name); }
		};

		struct PopulationItem
		{
			int code{};
			int year{};
			int age{};
			float males{};
			float females{};
			float total{};
		};

		inline bool operator< (PopulationItem const& lhs, PopulationItem const& rhs) {
			return std::tie(lhs.year, lhs.year) < std::tie(rhs.age, rhs.age);
		}

		inline bool operator> (PopulationItem const& lhs, PopulationItem const& rhs) {
			return std::tie(lhs.year, lhs.year) > std::tie(rhs.age, rhs.age);
		}
	}
}