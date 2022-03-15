#pragma once

namespace hgps {

	class SyncMessage {
	public:
		explicit SyncMessage(const int run, const int time)
			: run_{ run }, time_{ time } {}

		SyncMessage(const SyncMessage&) = delete;
		SyncMessage& operator=(const SyncMessage&) = delete;
		virtual ~SyncMessage() = default;

		int run() const noexcept { return run_; }
		int time() const noexcept { return time_; }

	protected:
		SyncMessage(SyncMessage&&) = default;
		SyncMessage& operator=(SyncMessage&&) = default;

	private:
		int run_;
		int time_;
	};


	template <typename PayloadType>
	class SyncDataMessage final : public SyncMessage
	{
	public:
		explicit SyncDataMessage(const int run, const int time, PayloadType&& data)
			: SyncMessage(run, time), data_{ std::move(data) } {}

		const PayloadType& data() const noexcept { return data_; }

	private:
		PayloadType data_;
	};
}
