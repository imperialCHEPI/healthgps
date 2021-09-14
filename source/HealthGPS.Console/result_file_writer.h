#pragma once
#include <mutex>
#include <fstream>
#include <filesystem>

#include "result_writer.h"
#include "model_info.h"

class ResultFileWriter final : public ResultWriter {
public:
	ResultFileWriter() = delete;
	ResultFileWriter(const std::filesystem::path file_name, const ModelInfo info);
	ResultFileWriter(const ResultFileWriter&) = delete;
	ResultFileWriter& operator= (const ResultFileWriter&) = delete;
	ResultFileWriter(ResultFileWriter&& other) noexcept;
	ResultFileWriter& operator=(ResultFileWriter&& other) noexcept;

	~ResultFileWriter();

	void write(const hgps::ResultEventMessage& message);

private:
	std::ofstream stream_;
	std::mutex lock_mutex_;
	std::atomic<bool> first_row_{ true };
	ModelInfo info_;

	void write_json_begin();
	void write_json_end();

	std::string to_json_string(const hgps::ResultEventMessage& message) const;
};

