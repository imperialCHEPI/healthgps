#pragma once

// Forward declarations for PIF data structures
// The actual implementations are in HealthGPS.Input namespace

namespace hgps::input {
struct PIFDataItem;
class PIFTable;
class PIFData;
} // namespace hgps::input

// For convenience, bring PIF types into the main hgps namespace
namespace hgps {
using PIFDataItem = input::PIFDataItem;
using PIFTable = input::PIFTable;
using PIFData = input::PIFData;
} // namespace hgps
