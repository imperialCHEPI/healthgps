// NOLINTBEGIN(modernize-avoid-c-arrays)
#include "pch.h"

#include "HealthGPS/program_dirs.h"
#include "HealthGPS/sha256.h"

TEST(SHA256, SHA256Context) {
    // Empty hash
    {
        hgps::SHA256Context ctx;
        EXPECT_EQ(ctx.finalise(),
                  "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    }

    // Example string
    {
        hgps::SHA256Context ctx;
        const char data[]{'h', 'e', 'l', 'l', 'o'};
        ctx.update(data);
        EXPECT_EQ(ctx.finalise(),
                  "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
    }

    // The same string, in two chunks
    {
        hgps::SHA256Context ctx;
        const char data1[]{'h', 'e', 'l'};
        ctx.update(data1);
        const char data2[]{'l', 'o'};
        ctx.update(data2);
        EXPECT_EQ(ctx.finalise(),
                  "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
    }
}

TEST(SHA256, compute_sha256_for_file) {
    const auto file_path = hgps::get_program_directory() / "example_file.txt";

    // Default buffer size (read whole file in one go)
    EXPECT_EQ(hgps::compute_sha256_for_file(file_path),
              "5891b5b522d5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03");
}

TEST(SHA256, compute_sha256_for_file_chunked) {
    const auto file_path = hgps::get_program_directory() / "example_file.txt";

    // Buffer size of two bytes (check that it works when reading in chunks)
    EXPECT_EQ(hgps::compute_sha256_for_file(file_path, 2),
              "5891b5b522d5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03");
}
