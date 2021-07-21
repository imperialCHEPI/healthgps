#pragma once
#include <map>
#include <string>

#include "HealthGPS.Core/forward_type.h"

namespace hgps {

	class DiseaseMeasure
	{
	public:
		DiseaseMeasure() = default;
		DiseaseMeasure(std::map<int, double> measures);

		std::size_t size() const noexcept;

		double at(int measure_id) const;

		const double operator[](int measure_id) const;

	private:
		std::map<int, double> measures_;
	};

	class DiseaseTable
	{
	public:
		DiseaseTable() = delete;
		DiseaseTable(std::string name, std::map<std::string, int>&& measures, 
			std::map<int, std::map<core::Gender, DiseaseMeasure>>&& data);

		std::string name() const noexcept;

		std::size_t size() const noexcept;

		std::size_t rows() const noexcept;

		std::size_t cols() const noexcept;

		std::map<std::string, int> measures() const noexcept;

		int at(std::string measure) const;

		const int operator[](std::string measure) const;

		DiseaseMeasure& operator()(int age, core::Gender gender);

		const DiseaseMeasure& operator()(int age, core::Gender gender) const;

	private:
		std::string name_;
		std::map<std::string, int> measures_;
		std::map<int, std::map<core::Gender, DiseaseMeasure>> data_;
	};
}