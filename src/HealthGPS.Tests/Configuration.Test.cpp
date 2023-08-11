#include "pch.h"

#include "HealthGPS.Console/configuration_parsing.h"
#include "HealthGPS.Console/configuration_parsing_helpers.h"
#include "HealthGPS.Console/jsonparser.h"

#include <fstream>
#include <random>
#include <type_traits>

using json = nlohmann::json;
using namespace host;
using namespace poco;

#define TYPE_OF(x) std::remove_cvref_t<decltype(x)>

namespace {
const std::string TEST_KEY = "my_key";
const std::string TEST_KEY2 = "other_key";

#ifdef _WIN32
const std::filesystem::path TEST_PATH_ABSOLUTE = R"(C:\Users\hgps_nonexistent\file.txt)";
#else
const std::filesystem::path TEST_PATH_ABSOLUTE = "/home/hgps_nonexistent/file.txt";
#endif

class TempDir {
  public:
    TempDir() : rnd_{std::random_device()()} {
        path_ = std::filesystem::path{::testing::TempDir()} / "hgps" / random_string();
        if (!std::filesystem::create_directories(path_)) {
            throw std::runtime_error{"Could not create temp dir"};
        }

        path_ = std::filesystem::absolute(path_);
    }

    ~TempDir() {
        if (std::filesystem::exists(path_)) {
            std::filesystem::remove_all(path_);
        }
    }

    std::string random_string() const { return std::to_string(rnd_()); }

    const auto &path() const { return path_; }

  private:
    mutable std::mt19937 rnd_;
    std::filesystem::path path_;

    std::filesystem::path createTempDir() {
        const auto rnd = std::random_device()();
        const auto path =
            std::filesystem::path{::testing::TempDir()} / "hgps" / std::to_string(rnd);
        if (!std::filesystem::create_directories(path)) {
            throw std::runtime_error{"Could not create temp dir"};
        }

        return std::filesystem::absolute(path);
    }
};

class ConfigParsingFixture : public ::testing::Test {
  public:
    const auto &tmp_path() const { return dir_.path(); }

    std::filesystem::path random_filename() const { return dir_.random_string(); }

    std::filesystem::path create_file_relative() const {
        auto file_path = random_filename();
        std::ofstream ofs{dir_.path() / file_path};
        return file_path;
    }

    std::filesystem::path create_file_absolute() const {
        auto file_path = tmp_path() / random_filename();
        std::ofstream ofs{file_path};
        return file_path;
    }

  private:
    TempDir dir_;
};

} // anonymous namespace

TEST(ConfigParsing, Get) {
    json j;

    // Null object
    EXPECT_THROW(get(j, TEST_KEY), ConfigurationError);

    // Key missing
    j[TEST_KEY2] = 1;
    EXPECT_THROW(get(j, TEST_KEY), ConfigurationError);

    // Key present
    j[TEST_KEY] = 2;
    EXPECT_NO_THROW(get(j, TEST_KEY));
}

template <class Func> void testGetTo(const Func &f) {
    f(1);
    f(std::string{"hello"});
}

TEST(ConfigParsing, GetToGood) {
    testGetTo([](const auto &exp) {
        json j;
        j[TEST_KEY] = exp;

        TYPE_OF(exp) out;
        EXPECT_TRUE(get_to(j, TEST_KEY, out));
        EXPECT_EQ(out, exp);
    });
}

TEST(ConfigParsing, GetToGoodSetFlag) {
    testGetTo([](const auto &exp) {
        json j;
        j[TEST_KEY] = exp;

        const auto check = [&exp, &j](bool initial) {
            bool success = initial;
            TYPE_OF(exp) out;
            EXPECT_TRUE(get_to(j, TEST_KEY, out, success));
            EXPECT_EQ(success, initial); // flag shouldn't have been modified
            EXPECT_EQ(out, exp);
        };

        check(true);
        check(false);
    });
}

TEST(ConfigParsing, GetToBadKey) {
    testGetTo([](const auto &exp) {
        json j;
        j[TEST_KEY] = exp;

        // Try reading a different, non-existent key
        TYPE_OF(exp) out;
        EXPECT_FALSE(get_to(j, TEST_KEY2, out));
    });
}

TEST(ConfigParsing, GetToBadKeySetFlag) {
    testGetTo([](const auto &exp) {
        json j;
        j[TEST_KEY] = exp;

        // Try reading a different, non-existent key
        bool success = true;
        TYPE_OF(exp) out;
        EXPECT_FALSE(get_to(j, TEST_KEY2, out, success));
        EXPECT_FALSE(success);
    });
}

TEST(ConfigParsing, GetToWrongType) {
    testGetTo([](const auto &exp) {
        json j;
        j[TEST_KEY] = exp;

        // Deliberately choose a different type from the inputs so we get a mismatch
        std::vector<int> out;
        EXPECT_FALSE(get_to(j, TEST_KEY, out));
    });
}

TEST(ConfigParsing, GetToWrongTypeSetFlag) {
    testGetTo([](const auto &exp) {
        json j;
        j[TEST_KEY] = exp;

        // Deliberately choose a different type from the inputs so we get a mismatch
        std::vector<int> out;
        bool success = true;
        EXPECT_FALSE(get_to(j, TEST_KEY, out, success));
        EXPECT_FALSE(success);
    });
}

TEST_F(ConfigParsingFixture, RebaseValidPathGood) {
    {
        const auto absPath = create_file_absolute();
        auto path = absPath;
        ASSERT_TRUE(path.is_absolute());
        EXPECT_NO_THROW(rebase_valid_path(path, tmp_path()));

        // As path is absolute, it shouldn't be modified
        EXPECT_EQ(path, absPath);
    }

    {
        const auto relPath = create_file_relative();
        auto path = relPath;
        ASSERT_TRUE(path.is_relative());
        EXPECT_NO_THROW(rebase_valid_path(path, tmp_path()));

        // Path should have been rebased
        EXPECT_EQ(path, tmp_path() / relPath);
    }
}

TEST_F(ConfigParsingFixture, RebaseValidPathBad) {
    {
        auto path = TEST_PATH_ABSOLUTE;
        ASSERT_TRUE(path.is_absolute());
        EXPECT_THROW(rebase_valid_path(path, tmp_path()), ConfigurationError);
    }

    {
        auto path = random_filename();
        ASSERT_TRUE(path.is_relative());
        EXPECT_THROW(rebase_valid_path(path, tmp_path()), ConfigurationError);
    }
}

TEST_F(ConfigParsingFixture, RebaseValidPathTo) {
    {
        // Should fail because key doesn't exist
        std::filesystem::path out;
        EXPECT_FALSE(rebase_valid_path_to(json{}, TEST_KEY, out, tmp_path()));
    }

    {
        const auto relPath = create_file_relative();
        ASSERT_TRUE(relPath.is_relative());

        json j;
        j[TEST_KEY] = relPath;
        std::filesystem::path out;
        EXPECT_TRUE(rebase_valid_path_to(j, TEST_KEY, out, tmp_path()));

        // Path should have been rebased
        EXPECT_EQ(out, tmp_path() / relPath);
    }

    {
        json j;
        j[TEST_KEY] = TEST_PATH_ABSOLUTE;

        // Should fail because path is invalid
        std::filesystem::path out;
        EXPECT_FALSE(rebase_valid_path_to(j, TEST_KEY, out, tmp_path()));
    }
}

TEST_F(ConfigParsingFixture, RebaseValidPathToSetFlag) {
    {
        // Should fail because key doesn't exist
        bool success = true;
        std::filesystem::path out;
        rebase_valid_path_to(json{}, TEST_KEY, out, tmp_path(), success);
        EXPECT_FALSE(success);
    }

    {
        const auto relPath = create_file_relative();
        ASSERT_TRUE(relPath.is_relative());

        json j;
        j[TEST_KEY] = relPath;
        bool success = true;
        std::filesystem::path out;
        rebase_valid_path_to(j, TEST_KEY, out, tmp_path());
        EXPECT_TRUE(success);

        // Path should have been rebased
        EXPECT_EQ(out, tmp_path() / relPath);
    }

    {
        json j;
        j[TEST_KEY] = TEST_PATH_ABSOLUTE;
        auto s = j.dump();
        fmt::print(fmt::fg(fmt::color::red), "JSON: {}\n", s);

        // Should fail because path is invalid
        bool success = true;
        std::filesystem::path out;
        rebase_valid_path_to(j, TEST_KEY, out, tmp_path(), success);
        EXPECT_FALSE(success);
    }
}

TEST_F(ConfigParsingFixture, GetFileInfo) {

    const FileInfo info1{.name = create_file_absolute(),
                         .format = "csv",
                         .delimiter = ",",
                         .columns = {{"a", "string"}, {"b", "other string"}}};
    json j = info1;

    /*
     * Converting to JSON and back again should work. NB: We just assume that the path
     * rebasing code works because it's already been tested.
     */
    const auto info2 = get_file_info(j, tmp_path());
    EXPECT_EQ(info1, info2);

    // Removing a required field should cause an error
    j.erase("format");
    EXPECT_THROW(get_file_info(j, tmp_path()), ConfigurationError);
}

TEST_F(ConfigParsingFixture, GetBaseLineInfo) {
    const BaselineInfo info1{
        .format = "csv",
        .delimiter = ",",
        .encoding = "UTF8",
        .file_names = {{"a", create_file_absolute()}, {"b", create_file_absolute()}}};

    json j;
    j["baseline_adjustments"] = info1;

    // Convert to JSON and back again
    const auto info2 = get_baseline_info(j, tmp_path());
    EXPECT_EQ(info1, info2);

    // Using an invalid path should cause an error
    j["baseline_adjustments"]["file_names"]["a"] = random_filename();
    EXPECT_THROW(get_baseline_info(j, tmp_path()), ConfigurationError);
}
