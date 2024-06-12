#pragma once

#include <openssl/evp.h>

#include <algorithm>
#include <filesystem>
#include <string>

namespace hgps {

/// @brief Compute the SHA256 hash for a file
/// @param file_path Path to the file
/// @param buffer_size Size of the buffer for reading the file in chunks
/// @return The SHA256 hash
std::string compute_sha256_for_file(const std::filesystem::path &file_path,
                                    size_t buffer_size = static_cast<size_t>(1024 * 1024));

//! An object for computing a SHA256 hash
class SHA256Context {
  public:
    SHA256Context();
    ~SHA256Context();

    /// @brief Feed in more data to the hash algorithm
    /// @tparam R Range type
    /// @param range A container-like range of char data
    template <class R> void update(const R &range) {
        EVP_DigestUpdate(ctx_, &*std::begin(range),
                         std::distance(std::begin(range), std::end(range)));
    }

    //! Finish calculating the hash and return
    std::string finalise();

  private:
    EVP_MD_CTX *ctx_;
};
} // namespace hgps
