#pragma once
#include "sync_message.h"
#include "gender_table.h"

namespace hgps {

	using ResidualMortalityMessage = SyncDataMessage<GenderTable<int, double>>;

	using NetImmigrationMessage = SyncDataMessage<IntegerAgeGenderTable>;

	using BaselineAdjustmentMessage = SyncDataMessage<std::map<std::string, DoubleGenderValue>>;
}
