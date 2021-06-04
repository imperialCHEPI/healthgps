#pragma once

#include <adevs/adevs.h>
#include "HealthGPS.Core\datastore.h"
#include "scenario.h"

namespace hgps {

	class ModelRunner
	{
	public:
		explicit ModelRunner(adevs::Model<int>& model, hgps::core::Datastore& manager);

		ModelRunner() = delete;

		double run() const;

	private:
		adevs::Model<int>& simulation_;
		hgps::core::Datastore& datastore_;
	};
}