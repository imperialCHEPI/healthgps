#pragma once

#include "event_message.h"

#include <map>
#include <string>
#include <vector>

namespace hgps {

/// MAHIMA: One row for the IndividualIDTracking CSV; carries filtered per-person data so the same
/// person (by id) can be tracked across baseline and intervention.
struct IndividualTrackingRow {
    std::size_t id{};
    unsigned int age{};
    std::string gender{};
    std::string region{};
    std::string ethnicity{};
    std::string income_category{};
    std::map<std::string, double> risk_factors{};
};

/// MAHIMA: Event carrying filtered individual rows for the IndividualIDTracking CSV. Published by
/// AnalysisModule when individual_id_tracking is enabled; consumed by IndividualIDTrackingWriter.
struct IndividualTrackingEventMessage final : public EventMessage {
    IndividualTrackingEventMessage() = delete;

    IndividualTrackingEventMessage(std::string sender, unsigned int run, int time,
                                   std::string scenario_name,
                                   std::vector<IndividualTrackingRow> rows);

    int model_time{};
    std::string scenario_name{};
    std::vector<IndividualTrackingRow> rows{};

    int id() const noexcept override;
    std::string to_string() const override;
    void accept(EventMessageVisitor &visitor) const override;
};

} // namespace hgps
