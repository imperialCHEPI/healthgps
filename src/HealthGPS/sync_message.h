#pragma once

namespace hgps {

/// @brief Scenario data synchronisation message data type
class SyncMessage {
  public:
    /// @brief Initialises a new instance of the SyncMessage class
    /// @param run Simulation run number
    /// @param time Simulation time
    explicit SyncMessage(const int run, const int time) : run_{run}, time_{time} {}

    SyncMessage(const SyncMessage &) = delete;
    SyncMessage &operator=(const SyncMessage &) = delete;
    /// @brief Destroys a SyncMessage instance
    virtual ~SyncMessage() = default;

    /// @brief Gets the Simulation run number
    /// @return Run number
    int run() const noexcept { return run_; }

    /// @brief Gets the Simulation time
    /// @return Simulation time
    int time() const noexcept { return time_; }

  protected:
    /// @brief Moves the target of other to the target of *this
    /// @param other The other instance to move
    SyncMessage(SyncMessage &&other) = default;

    /// @brief Moves the target of other to *this
    /// @param other The other instance to move
    /// @return This instance
    SyncMessage &operator=(SyncMessage &&other) = default;

  private:
    int run_;
    int time_;
};

/// @brief Implements a synchronisation message with payload
/// @tparam PayloadType The payload type
template <typename PayloadType> class SyncDataMessage final : public SyncMessage {
  public:
    /// @brief Initialises a new instance of the SyncMessage class
    /// @param run Simulation run number
    /// @param time Simulation time
    /// @param data The payload data instance
    explicit SyncDataMessage(const int run, const int time, PayloadType &&data)
        : SyncMessage(run, time), data_{std::move(data)} {}

    /// @brief Creates a copy of a SyncDataMessage using a deep copy of the data
    /// @param run Simulation run number
    /// @param time Simulation time
    /// @param data The payload data instance to copy
    explicit SyncDataMessage(const int run, const int time, const PayloadType &data)
        : SyncMessage(run, time), data_{data} {} // Makes a copy

    /// @brief Gets the message payload data
    /// @return The payload data
    const PayloadType &data() const noexcept { return data_; }

  private:
    PayloadType data_;
};
} // namespace hgps
