#include "HealthGPS/sha256.h"

#include <fmt/format.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace hgps {
std::string compute_sha256_for_file(const std::filesystem::path &file_path, size_t buffer_size) {
    std::ifstream ifs{file_path};
    if (!ifs) {
        throw std::runtime_error(fmt::format("Could not read file: {}", file_path.string()));
    }

    std::vector<unsigned char> buffer;
    buffer.resize(buffer_size);
    SHA256Context ctx;

    do {
        ifs.read(reinterpret_cast<char *>(buffer.data()), buffer_size);
        if (!ifs) {
            buffer.resize(ifs.gcount());
        }

        ctx.update(buffer);
    } while (ifs);

    return ctx.finalise();
}

SHA256Context::SHA256Context() : ctx_{EVP_MD_CTX_new()} {
    EVP_DigestInit_ex(ctx_, EVP_sha256(), nullptr);
}

SHA256Context::~SHA256Context() { EVP_MD_CTX_free(ctx_); }

std::string SHA256Context::finalise() {
    std::array<unsigned char, 32> hash;
    unsigned int len;
    EVP_DigestFinal_ex(ctx_, hash.data(), &len);

    std::stringstream ss;
    for (auto byte : hash) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }
    return ss.str();
}
} // namespace hgps
