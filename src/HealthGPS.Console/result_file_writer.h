#pragma once
#include <mutex>
#include <atomic>
#include <fstream>
#include <filesystem>

#include "result_writer.h"
#include "model_info.h"

class ResultFileWriter final : public ResultWriter {
public:
	ResultFileWriter() = delete;
	ResultFileWriter(const std::filesystem::path file_name, const ExperimentInfo info);
	ResultFileWriter(const ResultFileWriter&) = delete;
	ResultFileWriter& operator= (const ResultFileWriter&) = delete;
	ResultFileWriter(ResultFileWriter&& other) noexcept;
	ResultFileWriter& operator=(ResultFileWriter&& other) noexcept;

	~ResultFileWriter();
	void write(const hgps::ResultEventMessage& message) override;

private:
	std::ofstream stream_;
	std::ofstream csvstream_;
	std::mutex lock_mutex_;
	std::atomic<bool> first_row_{ true };
	ExperimentInfo info_;

	void write_json_begin(const std::filesystem::path output);
	void write_json_end();

	std::string to_json_string(const hgps::ResultEventMessage& message) const;
	void write_csv_channels(const hgps::ResultEventMessage& message);
	void write_csv_header(const hgps::ResultEventMessage& message);
};

