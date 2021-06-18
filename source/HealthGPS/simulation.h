#pragma once

#include <adevs/adevs.h>
#include "interfaces.h"
#include "modelcontext.h"

namespace hgps {

	class Simulation : public adevs::Model<int>
	{
	public:
		Simulation() = delete;
		explicit Simulation(ModelContext& context, RandomBitGenerator&& generator)
			: context_{ context }, rnd_{ generator }
		{}

		virtual ~Simulation() = default;

		virtual void initialize() const = 0;

		virtual void terminate() const = 0;

	protected:
		ModelContext context_;
		RandomBitGenerator& rnd_;
		adevs::Time end_time_;
	};
}