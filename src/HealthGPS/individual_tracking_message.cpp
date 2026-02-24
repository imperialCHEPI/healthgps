#include "individual_tracking_message.h"

#include <fmt/format.h>
#include <utility>

namespace hgps {

IndividualTrackingEventMessage::IndividualTrackingEventMessage(
    std::string sender, unsigned int run, int time, std::string scenario_name,
    std::vector<IndividualTrackingRow> rows)
    : EventMessage{std::move(sender), run}, model_time{time},
      scenario_name{std::move(scenario_name)}, rows{std::move(rows)} {}

int IndividualTrackingEventMessage::id() const noexcept {
    return static_cast<int>(EventType::individual_tracking);
}

std::string IndividualTrackingEventMessage::to_string() const {
    return fmt::format("Source: {}, run # {}, time: {}, scenario: {}, rows: {}", source, run_number,
                       model_time, scenario_name, rows.size());
}

void IndividualTrackingEventMessage::accept(EventMessageVisitor &visitor) const {
    visitor.visit(*this);
}

} // namespace hgps
