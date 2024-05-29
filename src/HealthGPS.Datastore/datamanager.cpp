#include "datamanager.h"
#include "HealthGPS.Core/math_util.h"
#include "HealthGPS.Core/string_util.h"
#include "HealthGPS/program_path.h"

#include <fmt/color.h>
#include <fstream>
#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonschema/jsonschema.hpp>
#include <rapidcsv.h>

#include <utility>

namespace {
using namespace jsoncons;

json resolve_uri(const jsoncons::uri &uri, const std::filesystem::path &schema_directory) {
    constexpr const char *url_prefix =
        "https://raw.githubusercontent.com/imperialCHEPI/healthgps/main/schemas/v1/";

    const auto &uri_str = uri.string();
    if (!uri_str.starts_with(url_prefix)) {
        throw std::runtime_error(fmt::format("Unable to load URL: {}", uri_str));
    }

    // Strip URL prefix and load file from local filesystem
    const auto filename = std::filesystem::path{uri.path()}.filename();
    const auto schema_path = schema_directory / filename;
    auto ifs = std::ifstream{schema_path};
    if (!ifs) {
        throw std::runtime_error("Failed to read schema file");
    }

    return json::parse(ifs);
}

void validate_index(const std::filesystem::path &schema_directory, std::ifstream &ifs) {
    // **YUCK**: We have to read in the data with jsoncons here rather than reusing
    // the nlohmann-json representation :-(
    const auto index = json::parse(ifs);

    // Load schema
    auto ifs_schema = std::ifstream{schema_directory / "data_index.json"};
    if (!ifs_schema) {
        throw std::runtime_error("Failed to load schema");
    }

    const auto resolver = [&schema_directory](const auto &uri) {
        return resolve_uri(uri, schema_directory);
    };
    const auto schema = jsonschema::make_json_schema(json::parse(ifs_schema), resolver);

    // Perform validation
    schema.validate(index);
}
} // namespace

namespace hgps::data {
DataManager::DataManager(std::filesystem::path root_directory, VerboseMode verbosity)
    : root_{std::move(root_directory)}, verbosity_{verbosity} {
    auto full_filename = root_ / "index.json";
    auto ifs = std::ifstream{full_filename};
    if (!ifs) {
        throw std::invalid_argument(
            fmt::format("File-based store, index file: '{}' not found.", full_filename.string()));
    }

    index_ = nlohmann::json::parse(ifs);

    if (!index_.contains("version")) {
        throw std::runtime_error("File-based store, invalid definition missing schema version");
    }

    auto version = index_["version"].get<int>();
    if (version != 2) {
        throw std::runtime_error(fmt::format(
            "File-based store, index schema version: {} mismatch, supported: 2", version));
    }

    // Validate against schema
    ifs.seekg(0);
    const auto schema_directory = get_program_directory() / "schemas" / "v1";
    validate_index(schema_directory, ifs);
}

std::vector<Country> DataManager::get_countries() const {
    auto results = std::vector<Country>();
    if (index_.contains("country")) {
        auto filepath = index_["country"]["path"].get<std::string>();
        auto filename = index_["country"]["file_name"].get<std::string>();
        filename = (root_ / filepath / filename).string();

        if (std::filesystem::exists(filename)) {
            rapidcsv::Document doc(filename);
            auto mapping = create_fields_index_mapping(doc.GetColumnNames(),
                                                       {"Code", "Name", "Alpha2", "Alpha3"});
            for (size_t i = 0; i < doc.GetRowCount(); i++) {
                auto row = doc.GetRow<std::string>(i);
                results.push_back(Country{.code = std::stoi(row[mapping["Code"]]),
                                          .name = row[mapping["Name"]],
                                          .alpha2 = row[mapping["Alpha2"]],
                                          .alpha3 = row[mapping["Alpha3"]]});
            }

            std::sort(results.begin(), results.end());
        } else {
            notify_warning(fmt::format("countries file: '{}' not found.", filename));
        }
    } else {
        notify_warning("index has no 'country' entry.");
    }

    return results;
}

Country DataManager::get_country(const std::string &alpha) const {
    auto c = get_countries();
    auto is_target = [&alpha](const hgps::core::Country &c) {
        return core::case_insensitive::equals(c.alpha2, alpha) ||
               core::case_insensitive::equals(c.alpha3, alpha);
    };

    auto country = std::find_if(c.begin(), c.end(), is_target);
    if (country != c.end()) {
        return *country;
    }

    throw std::invalid_argument(fmt::format("Target country: '{}' not found.", alpha));
}

std::vector<PopulationItem> DataManager::get_population(const Country &country) const {
    return DataManager::get_population(country, [](unsigned int) { return true; });
}

std::vector<PopulationItem>
DataManager::get_population(const Country &country,
                            std::function<bool(unsigned int)> time_filter) const {
    auto results = std::vector<PopulationItem>();

    if (index_.contains("demographic")) {
        auto nodepath = index_["demographic"]["path"].get<std::string>();
        auto filepath = index_["demographic"]["population"]["path"].get<std::string>();
        auto filename = index_["demographic"]["population"]["file_name"].get<std::string>();

        // Tokenized file names X{country.code}X.xxx
        filename = replace_string_tokens(filename, {std::to_string(country.code)});
        filename = (root_ / nodepath / filepath / filename).string();

        // LocID,Location,VarID,Variant,Time,MidPeriod,AgeGrp,AgeGrpStart,AgeGrpSpan,PopMale,PopFemale,PopTotal
        if (std::filesystem::exists(filename)) {
            rapidcsv::Document doc(filename);
            auto mapping = create_fields_index_mapping(
                doc.GetColumnNames(), {"LocID", "Time", "Age", "PopMale", "PopFemale", "PopTotal"});

            for (size_t i = 0; i < doc.GetRowCount(); i++) {
                auto row = doc.GetRow<std::string>(i);
                auto row_time = std::stoi(row[mapping["Time"]]);
                if (!time_filter(row_time)) {
                    continue;
                }

                results.push_back(PopulationItem{.location_id = std::stoi(row[mapping["LocID"]]),
                                                 .at_time = row_time,
                                                 .with_age = std::stoi(row[mapping["Age"]]),
                                                 .males = std::stof(row[mapping["PopMale"]]),
                                                 .females = std::stof(row[mapping["PopFemale"]]),
                                                 .total = std::stof(row[mapping["PopTotal"]])});
            }

            std::sort(results.begin(), results.end());
        } else {
            notify_warning(
                fmt::format("{} population file: '{}' not found.", country.name, filename));
        }
    } else {
        notify_warning("index has no 'demographic' entry.");
    }

    return results;
}

std::vector<MortalityItem> DataManager::get_mortality(const Country &country) const {
    return DataManager::get_mortality(country, [](unsigned int) { return true; });
}

std::vector<MortalityItem>
DataManager::get_mortality(const Country &country,
                           std::function<bool(unsigned int)> time_filter) const {
    auto results = std::vector<MortalityItem>();
    if (index_.contains("demographic")) {
        auto nodepath = index_["demographic"]["path"].get<std::string>();
        auto filepath = index_["demographic"]["mortality"]["path"].get<std::string>();
        auto filename = index_["demographic"]["mortality"]["file_name"].get<std::string>();

        // Tokenized file names X{country.code}X.xxx
        filename = replace_string_tokens(filename, {std::to_string(country.code)});
        filename = (root_ / nodepath / filepath / filename).string();

        if (std::filesystem::exists(filename)) {
            rapidcsv::Document doc(filename);
            auto mapping = create_fields_index_mapping(
                doc.GetColumnNames(),
                {"LocID", "Time", "Age", "DeathMale", "DeathFemale", "DeathTotal"});
            for (size_t i = 0; i < doc.GetRowCount(); i++) {
                auto row = doc.GetRow<std::string>(i);
                auto row_time = std::stoi(row[mapping["Time"]]);
                if (!time_filter(row_time)) {
                    continue;
                }

                results.push_back(MortalityItem{.location_id = std::stoi(row[mapping["LocID"]]),
                                                .at_time = row_time,
                                                .with_age = std::stoi(row[mapping["Age"]]),
                                                .males = std::stof(row[mapping["DeathMale"]]),
                                                .females = std::stof(row[mapping["DeathFemale"]]),
                                                .total = std::stof(row[mapping["DeathTotal"]])});
            }

            std::sort(results.begin(), results.end());
        } else {
            notify_warning(
                fmt::format("{} mortality file: '{}' not found.", country.name, filename));
        }
    } else {
        notify_warning("index has no 'demographic' entry.");
    }

    return results;
}

std::vector<DiseaseInfo> DataManager::get_diseases() const {
    auto result = std::vector<DiseaseInfo>();
    if (index_.contains("diseases")) {
        const auto &registry = index_["diseases"]["registry"];
        for (const auto &item : registry) {
            auto info = DiseaseInfo{};
            auto group_str = std::string{};
            auto code_srt = std::string{};
            item["group"].get_to(group_str);
            item["id"].get_to(code_srt);
            item["name"].get_to(info.name);
            info.group = DiseaseGroup::other;
            info.code = Identifier{code_srt};
            if (core::case_insensitive::equals(group_str, "cancer")) {
                info.group = DiseaseGroup::cancer;
            }

            result.emplace_back(info);
        }

        std::sort(result.begin(), result.end());
    } else {
        notify_warning("index has no 'diseases' entry.");
    }

    return result;
}

DiseaseInfo DataManager::get_disease_info(const core::Identifier &code) const {
    if (index_.contains("diseases")) {
        const auto &registry = index_["diseases"]["registry"];
        const auto &disease_code_str = code.to_string();
        auto info = DiseaseInfo{};
        for (const auto &item : registry) {
            auto item_code_str = std::string{};
            item["id"].get_to(item_code_str);
            if (item_code_str == disease_code_str) {
                auto group_str = std::string{};
                item["group"].get_to(group_str);
                item["name"].get_to(info.name);
                info.code = code;
                info.group = DiseaseGroup::other;
                if (core::case_insensitive::equals(group_str, "cancer")) {
                    info.group = DiseaseGroup::cancer;
                }

                return info;
            }
        }

        throw std::invalid_argument(fmt::format("Disease code: '{}' not found.", code.to_string()));
    }

    throw std::runtime_error("Index has no 'diseases' entry.");
}

DiseaseEntity DataManager::get_disease(const DiseaseInfo &info, const Country &country) const {
    DiseaseEntity result;
    result.info = info;
    result.country = country;

    if (index_.contains("diseases")) {
        auto diseases_path = index_["diseases"]["path"].get<std::string>();
        auto disease_folder = index_["diseases"]["disease"]["path"].get<std::string>();
        auto filename = index_["diseases"]["disease"]["file_name"].get<std::string>();

        // Tokenized folder name X{info.code}X
        disease_folder = replace_string_tokens(disease_folder, {info.code.to_string()});

        // Tokenized file names X{country.code}X.xxx
        filename = replace_string_tokens(filename, {std::to_string(country.code)});

        filename = (root_ / diseases_path / disease_folder / filename).string();
        if (std::filesystem::exists(filename)) {
            rapidcsv::Document doc(filename);

            auto table =
                std::unordered_map<int, std::unordered_map<core::Gender, std::map<int, double>>>();

            auto mapping = create_fields_index_mapping(
                doc.GetColumnNames(), {"age", "gender_id", "measure_id", "measure", "mean"});
            for (size_t i = 0; i < doc.GetRowCount(); i++) {
                auto row = doc.GetRow<std::string>(i);
                auto age = std::stoi(row[mapping["age"]]);
                auto gender = static_cast<core::Gender>(std::stoi(row[mapping["gender_id"]]));
                auto measure_id = std::stoi(row[mapping["measure_id"]]);
                auto measure_name = core::to_lower(row[mapping["measure"]]);
                auto measure_value = std::stod(row[mapping["mean"]]);

                if (!result.measures.contains(measure_name)) {
                    result.measures.emplace(measure_name, measure_id);
                }

                if (!table[age].contains(gender)) {
                    table[age].emplace(gender, std::map<int, double>{});
                }

                table[age][gender][measure_id] = measure_value;
            }

            for (auto &pair : table) {
                for (auto &child : pair.second) {
                    result.items.emplace_back(DiseaseItem{
                        .with_age = pair.first, .gender = child.first, .measures = child.second});
                }
            }

            result.items.shrink_to_fit();
        } else {
            notify_warning(
                fmt::format("{}, {} file: '{}' not found.", country.name, info.name, filename));
        }
    } else {
        notify_warning("index has no 'diseases' entry.");
    }

    return result;
}

RelativeRiskEntity DataManager::get_relative_risk_to_disease(const DiseaseInfo &source,
                                                             const DiseaseInfo &target) const {
    if (index_.contains("diseases")) {
        auto diseases_path = index_["diseases"]["path"].get<std::string>();
        auto disease_folder = index_["diseases"]["disease"]["path"].get<std::string>();
        const auto &risk_node = index_["diseases"]["disease"]["relative_risk"];
        auto default_value = risk_node["to_disease"]["default_value"].get<float>();

        auto risk_folder = risk_node["path"].get<std::string>();
        auto file_folder = risk_node["to_disease"]["path"].get<std::string>();
        auto filename = risk_node["to_disease"]["file_name"].get<std::string>();

        // Tokenized folder name X{info.code}X
        auto source_code_str = source.code.to_string();
        disease_folder =
            replace_string_tokens(disease_folder, std::vector<std::string>{source_code_str});

        // Tokenized file name{ DISEASE_TYPE }_{ DISEASE_TYPE }.xxx
        auto target_code_str = target.code.to_string();
        auto tokens = {source_code_str, target_code_str};
        filename = replace_string_tokens(filename, tokens);

        filename = (root_ / diseases_path / disease_folder / risk_folder / file_folder / filename)
                       .string();
        if (std::filesystem::exists(filename)) {
            rapidcsv::Document doc(filename);

            auto table = RelativeRiskEntity();
            table.is_default_value = false;
            table.columns = doc.GetColumnNames();

            auto row_size = table.columns.size();
            auto non_default_count = std::size_t{0};
            for (size_t i = 0; i < doc.GetRowCount(); i++) {
                auto row = doc.GetRow<std::string>(i);

                auto row_data = std::vector<float>(row_size);
                for (size_t col = 0; col < row_size; col++) {
                    row_data[col] = std::stof(row[col]);
                }

                non_default_count += std::count_if(
                    std::next(row_data.cbegin()), row_data.cend(),
                    [&default_value](auto &v) { return !MathHelper::equal(v, default_value); });

                table.rows.emplace_back(row_data);
            }

            if (non_default_count < 1) {
                table.is_default_value = true;
                notify_warning(fmt::format("{} to {} relative risk file has only default values.",
                                           source_code_str, target_code_str));
            }

            return table;
        }
        notify_warning(fmt::format("{} to {} relative risk file not found, using default.",
                                   source_code_str, target_code_str));

        return generate_default_relative_risk_to_disease();
    }

    notify_warning("index has no 'diseases' entry.");

    return {};
}

RelativeRiskEntity
DataManager::get_relative_risk_to_risk_factor(const DiseaseInfo &source, Gender gender,
                                              const core::Identifier &risk_factor_key) const {
    auto table = RelativeRiskEntity();
    if (!index_.contains("diseases")) {
        notify_warning("index has no 'diseases' entry.");
        return table;
    }

    // Creates full file name from store configuration
    auto diseases_path = index_["diseases"]["path"].get<std::string>();
    auto disease_folder = index_["diseases"]["disease"]["path"].get<std::string>();
    const auto &risk_node = index_["diseases"]["disease"]["relative_risk"];

    auto risk_folder = risk_node["path"].get<std::string>();
    auto file_folder = risk_node["to_risk_factor"]["path"].get<std::string>();
    auto filename = risk_node["to_risk_factor"]["file_name"].get<std::string>();

    // Tokenized folder name X{info.code}X
    auto source_code_str = source.code.to_string();
    disease_folder = replace_string_tokens(disease_folder, {source_code_str});

    // Tokenized file name {GENDER}_{DISEASE_TYPE}_{RISK_FACTOR}.xxx
    auto factor_key_str = risk_factor_key.to_string();
    std::string gender_name = gender == Gender::male ? "male" : "female";
    auto tokens = {gender_name, source_code_str, factor_key_str};
    filename = replace_string_tokens(filename, tokens);
    filename =
        (root_ / diseases_path / disease_folder / risk_folder / file_folder / filename).string();
    if (!std::filesystem::exists(filename)) {
        notify_warning(fmt::format("{} to {} relative risk file not found, disabled.",
                                   source_code_str, factor_key_str));
        return table;
    }

    rapidcsv::Document doc(filename);
    table.is_default_value = false;
    table.columns = doc.GetColumnNames();
    auto row_size = table.columns.size();
    for (size_t i = 0; i < doc.GetRowCount(); i++) {
        auto row = doc.GetRow<std::string>(i);

        auto row_data = std::vector<float>(row_size);
        for (size_t col = 0; col < row_size; col++) {
            row_data[col] = std::stof(row[col]);
        }

        table.rows.emplace_back(row_data);
    }

    table.rows.shrink_to_fit();
    table.columns.shrink_to_fit();
    return table;
}

CancerParameterEntity DataManager::get_disease_parameter(const DiseaseInfo &info,
                                                         const Country &country) const {
    auto table = CancerParameterEntity();
    if (!index_.contains("diseases")) {
        notify_warning("index has no 'diseases' entry.");
        return table;
    }

    // Creates full file name from store configuration
    auto disease_path = index_["diseases"]["path"].get<std::string>();
    auto disease_folder = index_["diseases"]["disease"]["path"].get<std::string>();

    const auto &params_node = index_["diseases"]["disease"]["parameters"];
    auto params_folder = params_node["path"].get<std::string>();
    const auto &params_files = params_node["files"];

    // Tokenized folder name X{info.code}X
    auto info_code_str = info.code.to_string();
    disease_folder = replace_string_tokens(disease_folder, {info_code_str});

    // Tokenized path name P{COUNTRY_CODE}
    auto tokens = std::vector<std::string>{std::to_string(country.code)};
    params_folder = replace_string_tokens(params_folder, tokens);
    auto files_folder = (root_ / disease_path / disease_folder / params_folder);
    if (!std::filesystem::exists(files_folder)) {
        notify_warning(fmt::format("{}, {} parameters folder: '{}' not found.", info_code_str,
                                   country.name, files_folder.string()));
        return table;
    }

    for (const auto &file : params_files.items()) {
        auto file_name = (files_folder / file.value().get<std::string>());
        if (!std::filesystem::exists(file_name)) {
            notify_warning(fmt::format("{}, {} parameters file: '{}' not found.", info_code_str,
                                       country.name, file_name.string()));
            continue;
        }

        auto lookup = std::vector<LookupGenderValue>{};
        rapidcsv::Document doc(file_name.string());
        auto mapping =
            create_fields_index_mapping(doc.GetColumnNames(), {"Time", "Male", "Female"});
        for (size_t i = 0; i < doc.GetRowCount(); i++) {
            auto row = doc.GetRow<std::string>(i);
            lookup.emplace_back(LookupGenderValue{
                .value = std::stoi(row[mapping["Time"]]),
                .male = std::stod(row[mapping["Male"]]),
                .female = std::stod(row[mapping["Female"]]),
            });
        }

        if (file.key() == "distribution") {
            table.prevalence_distribution = lookup;
        } else if (file.key() == "survival_rate") {
            table.survival_rate = lookup;
        } else if (file.key() == "death_weight") {
            table.death_weight = lookup;
        } else {
            throw std::out_of_range(
                fmt::format("Unknown disease parameter file type: {}", file.key()));
        }
    }

    index_["diseases"]["time_year"].get_to(table.at_time);
    return table;
}

std::vector<BirthItem> DataManager::get_birth_indicators(const Country &country) const {
    return DataManager::get_birth_indicators(country, [](unsigned int) { return true; });
}

std::vector<BirthItem>
DataManager::get_birth_indicators(const Country &country,
                                  std::function<bool(unsigned int)> time_filter) const {
    std::vector<BirthItem> result;
    if (index_.contains("demographic")) {
        auto nodepath = index_["demographic"]["path"].get<std::string>();
        auto filefolder = index_["demographic"]["indicators"]["path"].get<std::string>();
        auto filename = index_["demographic"]["indicators"]["file_name"].get<std::string>();

        // Tokenized file name Pi{COUNTRY_CODE}.xxx
        auto tokens = std::vector<std::string>{std::to_string(country.code)};
        filename = replace_string_tokens(filename, tokens);
        filename = (root_ / nodepath / filefolder / filename).string();
        if (!std::filesystem::exists(filename)) {
            notify_warning(fmt::format("{}, demographic indicators file: '{}' not found.",
                                       country.name, filename));
            return result;
        }

        rapidcsv::Document doc(filename);
        auto mapping = create_fields_index_mapping(doc.GetColumnNames(), {"Time", "Births", "SRB"});
        for (size_t i = 0; i < doc.GetRowCount(); i++) {
            auto row = doc.GetRow<std::string>(i);
            auto row_time = std::stoi(row[mapping["Time"]]);
            if (!time_filter(row_time)) {
                continue;
            }

            result.push_back(BirthItem{.at_time = row_time,
                                       .number = std::stof(row[mapping["Births"]]),
                                       .sex_ratio = std::stof(row[mapping["SRB"]])});
        }
    } else {
        notify_warning("index has no 'demographic' entry.");
    }

    return result;
}

std::vector<LmsDataRow> DataManager::get_lms_parameters() const {
    auto parameters = std::vector<LmsDataRow>{};
    if (!index_.contains("analysis")) {
        notify_warning("index has no 'analysis' entry.");
        return parameters;
    }

    auto analysis_folder = index_["analysis"]["path"].get<std::string>();
    auto lms_filename = index_["analysis"]["lms_file_name"].get<std::string>();
    auto full_filename = (root_ / analysis_folder / lms_filename);
    if (!std::filesystem::exists(full_filename)) {
        notify_warning(fmt::format("LMS parameters file: '{}' not found.", full_filename.string()));
        return parameters;
    }

    rapidcsv::Document doc(full_filename.string());
    auto mapping = create_fields_index_mapping(doc.GetColumnNames(),
                                               {"age", "gender_id", "lambda", "mu", "sigma"});
    for (size_t i = 0; i < doc.GetRowCount(); i++) {
        auto row = doc.GetRow<std::string>(i);
        if (row.size() < 6) {
            continue;
        }

        parameters.emplace_back(
            LmsDataRow{.age = std::stoi(row[mapping["age"]]),
                       .gender = static_cast<core::Gender>(std::stoi(row[mapping["gender_id"]])),
                       .lambda = std::stod(row[mapping["lambda"]]),
                       .mu = std::stod(row[mapping["mu"]]),
                       .sigma = std::stod(row[mapping["sigma"]])});
    }

    return parameters;
}

DiseaseAnalysisEntity DataManager::get_disease_analysis(const Country &country) const {
    DiseaseAnalysisEntity entity;
    if (!index_.contains("analysis")) {
        notify_warning("index has no 'analysis' entry.");
        return entity;
    }

    auto analysis_folder = index_["analysis"]["path"].get<std::string>();
    auto disability_filename = index_["analysis"]["disability_file_name"].get<std::string>();
    const auto &cost_node = index_["analysis"]["cost_of_disease"];

    auto local_root_path = (root_ / analysis_folder);
    disability_filename = (local_root_path / disability_filename).string();
    if (!std::filesystem::exists(disability_filename)) {
        notify_warning(
            fmt::format("disease disability weights file: '{}' not found.", disability_filename));
        return entity;
    }

    rapidcsv::Document doc(disability_filename);
    for (size_t i = 0; i < doc.GetRowCount(); i++) {
        auto row = doc.GetRow<std::string>(i);
        if (row.size() < 2) {
            continue;
        }

        entity.disability_weights.emplace(row[0], std::stof(row[1]));
    }

    entity.cost_of_diseases = load_cost_of_diseases(country, cost_node, local_root_path);
    entity.life_expectancy = load_life_expectancy(country);
    return entity;
}

RelativeRiskEntity DataManager::generate_default_relative_risk_to_disease() const {
    if (index_.contains("diseases")) {
        auto age_limits = index_["diseases"]["age_limits"].get<std::vector<int>>();
        const auto &to_disease = index_["diseases"]["disease"]["relative_risk"]["to_disease"];
        auto default_value = to_disease["default_value"].get<float>();

        auto table = RelativeRiskEntity();
        table.is_default_value = true;
        table.columns = {"age", "male", "female"};
        for (int age = age_limits[0]; age <= age_limits[1]; age++) {
            table.rows.emplace_back(
                std::vector<float>{static_cast<float>(age), default_value, default_value});
        }

        return table;
    }
    notify_warning("index has no 'diseases' entry.");

    return {};
}

std::map<int, std::map<Gender, double>>
DataManager::load_cost_of_diseases(const Country &country, const nlohmann::json &node,
                                   const std::filesystem::path &parent_path) const {
    std::map<int, std::map<Gender, double>> result;
    auto cost_path = node["path"].get<std::string>();
    auto filename = node["file_name"].get<std::string>();

    // Tokenized file name BoD{COUNTRY_CODE}.xxx
    auto tokens = std::vector<std::string>{std::to_string(country.code)};
    filename = replace_string_tokens(filename, tokens);
    filename = (parent_path / cost_path / filename).string();
    if (!std::filesystem::exists(filename)) {
        notify_warning(
            fmt::format("{} cost of disease file: '{}' not found.", country.name, filename));
        return result;
    }

    rapidcsv::Document doc(filename);
    for (size_t i = 0; i < doc.GetRowCount(); i++) {
        auto row = doc.GetRow<std::string>(i);
        if (row.size() < 13) {
            continue;
        }

        auto age = std::stoi(row[6]);
        auto gender = static_cast<core::Gender>(std::stoi(row[8]));
        result[age][gender] = std::stod(row[12]);
    }

    return result;
}

std::vector<LifeExpectancyItem> DataManager::load_life_expectancy(const Country &country) const {
    std::vector<LifeExpectancyItem> result;
    if (index_.contains("demographic")) {
        auto nodepath = index_["demographic"]["path"].get<std::string>();
        auto filefolder = index_["demographic"]["indicators"]["path"].get<std::string>();
        auto filename = index_["demographic"]["indicators"]["file_name"].get<std::string>();

        // Tokenized file name Pi{COUNTRY_CODE}.xxx
        auto tokens = std::vector<std::string>{std::to_string(country.code)};
        filename = replace_string_tokens(filename, tokens);
        filename = (root_ / nodepath / filefolder / filename).string();
        if (!std::filesystem::exists(filename)) {
            notify_warning(fmt::format("{}, demographic indicators file: '{}' not found.",
                                       country.name, filename));
            return result;
        }

        rapidcsv::Document doc(filename);
        auto mapping = create_fields_index_mapping(doc.GetColumnNames(),
                                                   {"Time", "LEx", "LExMale", "LExFemale"});
        for (size_t i = 0; i < doc.GetRowCount(); i++) {
            auto row = doc.GetRow<std::string>(i);
            if (row.size() < 4) {
                continue;
            }

            result.emplace_back(LifeExpectancyItem{.at_time = std::stoi(row[mapping["Time"]]),
                                                   .both = std::stof(row[mapping["LEx"]]),
                                                   .male = std::stof(row[mapping["LExMale"]]),
                                                   .female = std::stof(row[mapping["LExFemale"]])});
        }
    }

    return result;
}

std::string DataManager::replace_string_tokens(const std::string &source,
                                               const std::vector<std::string> &tokens) {
    std::string output = source;
    std::size_t tk_end = 0;
    for (const auto &tk : tokens) {
        auto tk_start = output.find_first_of('{', tk_end);
        if (tk_start != std::string::npos) {
            tk_end = output.find_first_of('}', tk_start + 1);
            if (tk_end != std::string::npos) {
                output = output.replace(tk_start, tk_end - tk_start + 1, tk);
                tk_end = tk_start + tk.length();
            }
        }
    }

    return output;
}

std::map<std::string, std::size_t>
DataManager::create_fields_index_mapping(const std::vector<std::string> &column_names,
                                         const std::vector<std::string> &fields) {
    auto mapping = std::map<std::string, std::size_t>();
    for (const auto &field : fields) {
        auto field_index = core::case_insensitive::index_of(column_names, field);
        if (field_index < 0) {
            throw std::out_of_range(
                fmt::format("File-based store, required field {} not found", field));
        }

        mapping.emplace(field, field_index);
    }

    return mapping;
}

void DataManager::notify_warning(std::string_view message) const {
    if (verbosity_ == VerboseMode::none) {
        return;
    }

    fmt::print(fg(fmt::color::dark_salmon), "File-based store, {}\n", message);
}
} // namespace hgps::data
