#pragma once

#include <adevs/adevs.h>
#include "interfaces.h"
#include "modelinput.h"

namespace hgps {

	class Simulation : public adevs::Model<int>
	{
	public:
		Simulation() = delete;
		explicit Simulation(ModelInput& config, RandomBitGenerator&& generator)
			: config_{ config }, rnd_{ generator }
		{}

		virtual ~Simulation() = default;

		virtual void initialize() = 0;

		virtual void terminate() = 0;

	protected:
		ModelInput config_;
		RandomBitGenerator& rnd_;
		adevs::Time end_time_;
	};
}