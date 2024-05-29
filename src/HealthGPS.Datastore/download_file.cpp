#include "download_file.h"

#include <curlpp/Easy.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <fmt/format.h>

#include <fstream>

namespace hgps::data {
std::filesystem::path download_file(const std::string &url,
                                    const std::filesystem::path &output_directory) {
    curlpp::Cleanup cleanup;

    auto output_path = output_directory / "data.zip";
    std::ofstream ofs{output_path};
    if (!ofs) {
        throw std::runtime_error(fmt::format("Failed to create file {}", output_path.string()));
    }

    // Our request to be sent
    curlpp::Easy request;
    request.setOpt<curlpp::options::Url>(url);
    request.setOpt<curlpp::options::WriteStream>(&ofs);

    // Make request
    request.perform();

    return output_path;
}
} // namespace hgps::data
