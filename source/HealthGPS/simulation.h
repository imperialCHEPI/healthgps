#pragma once

#include <adevs/adevs.h>
#include "interfaces.h"

namespace hgps {

	class Simulation : public adevs::Model<int>
	{
	public:
		Simulation() = delete;
		explicit Simulation(RandomBitGenerator&& generator)
			: rnd_{generator}
		{}

		virtual ~Simulation() = default;

		int trial_number() const noexcept { return trial_number_; }

		virtual void initialize() const = 0;

		virtual void terminate() const = 0;

	protected:
		int trial_number_{};
		RandomBitGenerator& rnd_;
		adevs::Time end_time_;
	};
}