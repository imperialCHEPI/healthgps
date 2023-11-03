#pragma once

#include "gender_table.h"
#include "sync_message.h"

namespace hgps {

/// @brief Defines the residual mortality synchronisation message
using ResidualMortalityMessage = SyncDataMessage<GenderTable<int, double>>;

/// @brief Defines the net immigration synchronisation message
using NetImmigrationMessage = SyncDataMessage<IntegerAgeGenderTable>;

} // namespace hgps
