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

constexpr auto *POLICY_A = R"(
        {
            "active_period": {
                    "start_time": 2022,
                    "finish_time": 2022
                },
            "impact_type": "absolute",
            "impacts": [
                {
                    "risk_factor": "BMI",
                    "impact_value": -1.0,
                    "from_age": 0,
                    "to_age": null
                }
            ]
        })";

constexpr auto *POLICY_B = R"(
        {
            "active_period": {
                "start_time": 2022,
                "finish_time": 2050
            },
            "impacts": [
                {
                    "risk_factor": "BMI",
                    "impact_value": -0.12,
                    "from_age": 5,
                    "to_age": 12
                },
                {
                    "risk_factor": "BMI",
                    "impact_value": -0.31,
                    "from_age": 13,
                    "to_age": 18
                },
                {
                    "risk_factor": "BMI",
                    "impact_value": -0.16,
                    "from_age": 19,
                    "to_age": null
                }
            ]
        })";

const auto POLICIES = []() {
    json p;
    p["a"] = json::parse(POLICY_A);
    p["b"] = json::parse(POLICY_B);
    return p;
}();

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

    auto create_config() const {
        host::Configuration config;
        config.root_path = dir_.path();
        return config;
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

TEST_F(ConfigParsingFixture, LoadInterventions) {
    {
        // No intervention
        auto config = create_config();
        json j;
        j["interventions"]["active_type_id"] = nullptr;
        j["interventions"]["types"] = json::object();
        EXPECT_NO_THROW(load_interventions(j, config));
        EXPECT_FALSE(config.active_intervention.has_value());
    }

    {
        // active_type_id key missing
        auto config = create_config();
        json j;
        j["interventions"]["other_key"] = 1;
        j["interventions"]["types"] = json::object();
        EXPECT_THROW(load_interventions(j, config), ConfigurationError);
    }

    {
        // A valid intervention
        auto config = create_config();
        json j;
        j["interventions"]["active_type_id"] = "A";
        j["interventions"]["types"] = POLICIES;
        EXPECT_NO_THROW(load_interventions(j, config));
        EXPECT_TRUE(config.active_intervention.has_value());
        auto intervention = POLICIES["a"].get<PolicyScenarioInfo>();
        intervention.identifier = "a";
        EXPECT_EQ(config.active_intervention, intervention);
    }

    {
        // An invalid intervention
        auto config = create_config();
        json j;
        j["interventions"]["active_type_id"] = "c";
        j["interventions"]["types"] = POLICIES;
        EXPECT_THROW(load_interventions(j, config), ConfigurationError);
    }
}

TEST(ConfigParsing, CheckVersion) {
    // Correct version present
    {
        json j;
        j["version"] = 2;
        EXPECT_NO_THROW(check_version(j));
    }

    // Wrong version
    {
        json j;
        j["version"] = 1;
        EXPECT_THROW(check_version(j), ConfigurationError);
    }

    // Version key missing
    {
        json j;
        j["other_key"] = 2;
        EXPECT_THROW(check_version(j), ConfigurationError);
    }
}

TEST_F(ConfigParsingFixture, LoadInputInfo) {
    const FileInfo file_info{.name = create_file_absolute(),
                             .format = "csv",
                             .delimiter = ",",
                             .columns = {{"a", "string"}, {"b", "other string"}}};
    const SettingsInfo settings_info{.country = "FRA",
                                     .age_range = hgps::core::IntegerInterval{0, 100},
                                     .size_fraction = 0.0001f};

    // Valid inputs
    {
        auto config = create_config();
        json j;
        j["inputs"]["dataset"] = file_info;
        j["inputs"]["settings"] = settings_info;
        EXPECT_NO_THROW(load_input_info(j, config));
        EXPECT_EQ(config.file, file_info);
        EXPECT_EQ(config.settings, settings_info);
    }

    // inputs key missing
    {
        json j;
        j["other_key"] = nullptr;
        auto config = create_config();
        EXPECT_THROW(load_input_info(j, config), ConfigurationError);
    }

    // Missing dataset
    {
        auto config = create_config();
        json j;
        j["inputs"]["settings"] = settings_info;
        EXPECT_THROW(load_input_info(j, config), ConfigurationError);
    }

    // Missing settings
    {
        auto config = create_config();
        json j;
        j["inputs"]["dataset"] = file_info;
        EXPECT_THROW(load_input_info(j, config), ConfigurationError);
    }
}

TEST_F(ConfigParsingFixture, LoadModellingInfo) {
    const std::vector<RiskFactorInfo> risk_factors{
        RiskFactorInfo{.name = "Gender", .level = 0, .range = std::nullopt},
        RiskFactorInfo{.name = "Age", .level = 1, .range = hgps::core::DoubleInterval(0.0, 1.0)}};
    const std::unordered_map<std::string, std::filesystem::path> risk_factor_models{
        {"a", create_file_absolute()}, {"b", create_file_absolute()}};
    const BaselineInfo baseline_info{
        .format = "csv",
        .delimiter = ",",
        .encoding = "UTF8",
        .file_names = {{"a", create_file_absolute()}, {"b", create_file_absolute()}}};
    const poco::SESInfo ses_info{.function = "normal", .parameters = {0.0, 1.0}};

    const json valid_modelling_info = [&]() {
        json j;
        j["modelling"]["risk_factors"] = risk_factors;
        j["modelling"]["risk_factor_models"] = risk_factor_models;
        j["modelling"]["baseline_adjustments"] = baseline_info;
        j["modelling"]["ses_model"] = ses_info;
        return j;
    }();

    // Valid modelling info
    {
        auto config = create_config();

        EXPECT_NO_THROW(load_modelling_info(valid_modelling_info, config));
        EXPECT_EQ(config.modelling.risk_factors, risk_factors);
        EXPECT_EQ(config.modelling.risk_factor_models, risk_factor_models);
        EXPECT_EQ(config.modelling.baseline_adjustment, baseline_info);
        EXPECT_EQ(config.ses, ses_info);
    }

    // No modelling key
    {
        auto config = create_config();
        json j;
        j["other_key"] = nullptr;
        EXPECT_THROW(load_modelling_info(j, config), ConfigurationError);
    }

    // Invalid risk factor
    {
        auto config = create_config();
        auto j = valid_modelling_info;
        j["modelling"]["risk_factors"][0].erase("level");
        EXPECT_THROW(load_modelling_info(j, config), ConfigurationError);
    }

    // Invalid risk factor model
    {
        auto config = create_config();
        auto j = valid_modelling_info;

        // Change to non-existent filepath
        j["modelling"]["risk_factor_models"]["a"] = random_filename();

        EXPECT_THROW(load_modelling_info(j, config), ConfigurationError);
    }

    // Invalid baseline adjustment
    {
        auto config = create_config();
        auto j = valid_modelling_info;
        j["modelling"]["baseline_adjustments"].erase("format");
        EXPECT_THROW(load_modelling_info(j, config), ConfigurationError);
    }

    // Invalid SES model
    {
        auto config = create_config();
        auto j = valid_modelling_info;
        j["modelling"]["ses_model"].erase("function_name");
        EXPECT_THROW(load_modelling_info(j, config), ConfigurationError);
    }
}

TEST_F(ConfigParsingFixture, LoadRunningInfo) {
    constexpr auto start_time = 2010u;
    constexpr auto stop_time = 2050u;
    constexpr auto trial_runs = 1u;
    constexpr auto sync_timeout_ms = 15'000u;
    constexpr auto seed = 42u;
    const std::vector<std::string> diseases{"alzheimer", "asthma",      "colorectalcancer",
                                            "diabetes",  "lowbackpain", "osteoarthritisknee"};

    const auto valid_running_info = [&]() {
        json j;
        auto &running = j["running"];
        running["start_time"] = start_time;
        running["stop_time"] = stop_time;
        running["trial_runs"] = trial_runs;
        running["sync_timeout_ms"] = sync_timeout_ms;
        running["diseases"] = diseases;
        running["seed"][0] = seed; // for some reason it has to be an array
        running["interventions"]["active_type_id"] = "a";
        running["interventions"]["types"] = POLICIES;
        return j;
    }();

    // Valid running info
    {
        auto config = create_config();
        EXPECT_NO_THROW(load_running_info(valid_running_info, config));
        EXPECT_EQ(config.start_time, start_time);
        EXPECT_EQ(config.stop_time, stop_time);
        EXPECT_EQ(config.trial_runs, trial_runs);
        EXPECT_EQ(config.sync_timeout_ms, sync_timeout_ms);
        EXPECT_EQ(config.custom_seed, seed);
        EXPECT_EQ(config.diseases, diseases);
    }

    // Should still work with empty seed array
    {
        auto config = create_config();
        auto j = valid_running_info;
        j["running"]["seed"].clear();

        std::string s = j.dump();
        EXPECT_NO_THROW(load_running_info(j, config));
        EXPECT_EQ(config.start_time, start_time);
        EXPECT_EQ(config.stop_time, stop_time);
        EXPECT_EQ(config.trial_runs, trial_runs);
        EXPECT_EQ(config.sync_timeout_ms, sync_timeout_ms);
        EXPECT_EQ(config.diseases, diseases);
        EXPECT_FALSE(config.custom_seed.has_value());
    }

    // If any of the required keys are invalid then an error should be thrown
    for (const auto key : {"start_time", "stop_time", "trial_runs", "sync_timeout_ms", "diseases",
                           "seed", "interventions"}) {
        auto config = create_config();
        auto j = valid_running_info;
        j["running"][key] = nullptr; // None of the values should be null
        EXPECT_THROW(load_running_info(j, config), ConfigurationError);
    }
}

TEST_F(ConfigParsingFixture, LoadOutputInfo) {
    const OutputInfo output_info{
        .comorbidities = 3, .folder = "/home/test", .file_name = "filename.txt"};
    const auto valid_output_info = [&]() {
        json j;
        j["output"] = output_info;
        return j;
    }();

    // Valid info
    {
        auto config = create_config();
        EXPECT_NO_THROW(load_output_info(valid_output_info, config));
        EXPECT_EQ(config.output, output_info);
    }

    // If any of the required keys are invalid then an error should be thrown
    for (const auto key : {"folder", "file_name", "comorbidities"}) {
        auto config = create_config();
        auto j = valid_output_info;
        j["output"][key] = nullptr; // None of the values should be null
        EXPECT_THROW(load_output_info(j, config), ConfigurationError);
    }
}