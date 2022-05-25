#pragma once
#include "sync_message.h"
#include "gender_table.h"
#include "riskfactor_adjustment_types.h"

namespace hgps {

	using ResidualMortalityMessage = SyncDataMessage<GenderTable<int, double>>;

	using NetImmigrationMessage = SyncDataMessage<IntegerAgeGenderTable>;

	using BaselineAdjustmentMessage = SyncDataMessage<FactorAdjustmentTable>;
}
