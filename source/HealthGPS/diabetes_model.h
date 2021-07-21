#pragma once
#include "interfaces.h"

namespace hgps {
	class DiabetesModel final : public DiseaseModel {
	public:
		DiabetesModel() = delete;

		std::string name() const noexcept override;

		void generate(RuntimeContext& context) override;

		void adjust_relative_risk(RuntimeContext& context) override;

		void update(RuntimeContext& context) override;

	private:

	};
}

