#pragma once
#include "data_series.h"

#include <string>
#include <map>
#include <vector>

namespace hgps {

    struct ResultByGender {
        double male{};
        double female{};
    };

    struct DALYsIndicator {
        double years_of_life_lost{};
        double years_lived_with_disability{};
        double disablity_adjusted_life_years{};
    };

	struct ModelResult {
        ModelResult() = delete;
        ModelResult(const unsigned int sample_size);

        int population_size{};

        int number_alive{};

        int number_dead{};

        int number_emigrated{};

        ResultByGender average_age{};

        DALYsIndicator indicators{};

        std::map<std::string, ResultByGender> risk_ractor_average{};

        std::map<std::string, ResultByGender> disease_prevalence{};

        std::map<std::string, double> metrics{};

        DataSeries time_series;

		std::string to_string() const noexcept;

        int number_of_recyclable() const noexcept;

    private:
        std::size_t caluclate_min_padding() const noexcept;
	};
}


