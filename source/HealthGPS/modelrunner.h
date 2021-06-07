#pragma once

#include <adevs/adevs.h>
#include "modulefactory.h"
#include "scenario.h"

namespace hgps {

	class ModelRunner
	{
	public:
		explicit ModelRunner(adevs::Model<int>& model, ModuleFactory& factory);

		ModelRunner() = delete;

		double run() const;

	private:
		adevs::Model<int>& simulation_;
		ModuleFactory& factory_;
	};
}