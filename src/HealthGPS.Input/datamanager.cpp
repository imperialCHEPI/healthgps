#include "datamanager.h"
#include "download_file.h"
#include "schema.h"
#include "zip_file.h"

#include "HealthGPS.Core/math_util.h"
#include "HealthGPS.Core/string_util.h"

#include <fmt/color.h>
#include <rapidcsv.h>

#include <cstdlib>
#include <fstream>
#include <regex>
#include <utility>

namespace {
//! The name of the index file
constexpr const char *IndexFileName = "index.json";

//! The name of the index.json schema file
constexpr const char *DataIndexSchemaFileName = "data_index.json";

//! The version of the index.json schema file
constexpr int DataIndexSchemaVersion = 1;

nlohmann::json read_input_files_from_directory(const std::filesystem::path &data_path) {
    return hgps::input::load_and_validate_json(data_path / IndexFileName, DataIndexSchemaFileName,
                                               DataIndexSchemaVersion);
}
} // anonymous namespace

namespace hgps::input {
DataManager::DataManager(std::filesystem::path data_path, VerboseMode verbosity)
    : root_(std::move(data_path)), verbosity_(verbosity),
      index_(read_input_files_from_directory(root_)) {}

std::vector<Country> DataManager::get_countries() const {
    auto results = std::vector<Country>();
    auto filepath = index_["country"]["path"].get<std::string>();
    auto filename = index_["country"]["file_name"].get<std::string>();
    filename = (root_ / filepath / filename).string();

    if (!std::filesystem::exists(filename)) {
        throw std::runtime_error(fmt::format("countries file: '{}' not found.", filename));
    }

    rapidcsv::Document doc(filename);
    auto mapping =
        create_fields_index_mapping(doc.GetColumnNames(), {"Code", "Name", "Alpha2", "Alpha3"});
    for (size_t i = 0; i < doc.GetRowCount(); i++) {
        auto row = doc.GetRow<std::string>(i);
        results.push_back(Country{.code = std::stoi(row[mapping["Code"]]),
                                  .name = row[mapping["Name"]],
                                  .alpha2 = row[mapping["Alpha2"]],
                                  .alpha3 = row[mapping["Alpha3"]]});
    }

    std::sort(results.begin(), results.end());

    return results;
}

Country DataManager::get_country(const std::string &alpha) const {
    auto c = get_countries();
    auto is_target = [&alpha](const hgps::core::Country &c) {
        return core::case_insensitive::equals(c.alpha2, alpha) ||
               core::case_insensitive::equals(c.alpha3, alpha);
    };

    auto country = std::find_if(c.begin(), c.end(), is_target);
    if (country == c.end()) {
        throw std::runtime_error(fmt::format("Target country: '{}' not found.", alpha));
    }

    return *country;
}

std::vector<PopulationItem> DataManager::get_population(const Country &country) const {
    return DataManager::get_population(country, [](unsigned int) { return true; });
}

std::vector<PopulationItem>
DataManager::get_population(const Country &country,
                            std::function<bool(unsigned int)> time_filter) const {
    auto results = std::vector<PopulationItem>();

    auto nodepath = index_["demographic"]["path"].get<std::string>();
    auto filepath = index_["demographic"]["population"]["path"].get<std::string>();
    auto filename = index_["demographic"]["population"]["file_name"].get<std::string>();

    // Tokenized file names X{country.code}X.xxx
    filename = replace_string_tokens(filename, {std::to_string(country.code)});
    filename = (root_ / nodepath / filepath / filename).string();

    // LocID,Location,VarID,Variant,Time,MidPeriod,AgeGrp,AgeGrpStart,AgeGrpSpan,PopMale,PopFemale,PopTotal
    if (!std::filesystem::exists(filename)) {
        throw std::runtime_error(
            fmt::format("{} population file: '{}' not found.", country.name, filename));
    }

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

    return results;
}

std::vector<MortalityItem> DataManager::get_mortality(const Country &country) const {
    return DataManager::get_mortality(country, [](unsigned int) { return true; });
}

std::vector<MortalityItem>
DataManager::get_mortality(const Country &country,
                           std::function<bool(unsigned int)> time_filter) const {
    auto results = std::vector<MortalityItem>();
    auto nodepath = index_["demographic"]["path"].get<std::string>();
    auto filepath = index_["demographic"]["mortality"]["path"].get<std::string>();
    auto filename = index_["demographic"]["mortality"]["file_name"].get<std::string>();

    // Tokenized file names X{country.code}X.xxx
    filename = replace_string_tokens(filename, {std::to_string(country.code)});
    filename = (root_ / nodepath / filepath / filename).string();

    if (!std::filesystem::exists(filename)) {
        throw std::runtime_error(
            fmt::format("{} mortality file: '{}' not found.", country.name, filename));
    }

    rapidcsv::Document doc(filename);
    auto mapping = create_fields_index_mapping(
        doc.GetColumnNames(), {"LocID", "Time", "Age", "DeathMale", "DeathFemale", "DeathTotal"});
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

    return results;
}

std::vector<DiseaseInfo> DataManager::get_diseases() const {
    auto result = std::vector<DiseaseInfo>();

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

    return result;
}

DiseaseInfo DataManager::get_disease_info(const core::Identifier &code) const {
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

    throw std::runtime_error(fmt::format("Disease code: '{}' not found.", code.to_string()));
}

DiseaseEntity DataManager::get_disease(const DiseaseInfo &info, const Country &country) const {
    DiseaseEntity result;
    result.info = info;
    result.country = country;

    auto diseases_path = index_["diseases"]["path"].get<std::string>();
    auto disease_folder = index_["diseases"]["disease"]["path"].get<std::string>();
    auto filename = index_["diseases"]["disease"]["file_name"].get<std::string>();

    // Tokenized folder name X{info.code}X
    disease_folder = replace_string_tokens(disease_folder, {info.code.to_string()});

    // Tokenized file names X{country.code}X.xxx
    filename = replace_string_tokens(filename, {std::to_string(country.code)});

    filename = (root_ / diseases_path / disease_folder / filename).string();
    if (!std::filesystem::exists(filename)) {
        throw std::runtime_error(
            fmt::format("{}, {} file: '{}' not found.", country.name, info.name, filename));
    }

    rapidcsv::Document doc(filename);

    auto table = std::unordered_map<int, std::unordered_map<core::Gender, std::map<int, double>>>();

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

    return result;
}

std::optional<RelativeRiskEntity>
DataManager::get_relative_risk_to_disease(const DiseaseInfo &source,
                                          const DiseaseInfo &target) const {
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

    filename =
        (root_ / diseases_path / disease_folder / risk_folder / file_folder / filename).string();
    if (!std::filesystem::exists(filename)) {
        notify_warning(
            fmt::format("{} to {} relative risk file not found", source_code_str, target_code_str));
        return std::nullopt;
    }

    rapidcsv::Document doc(filename);

    auto table = RelativeRiskEntity();
    table.columns = doc.GetColumnNames();

    auto row_size = table.columns.size();
    auto non_default_count = std::size_t{0};
    for (size_t i = 0; i < doc.GetRowCount(); i++) {
        auto row = doc.GetRow<std::string>(i);

        auto row_data = std::vector<float>(row_size);
        for (size_t col = 0; col < row_size; col++) {
            row_data[col] = std::stof(row[col]);
        }

        non_default_count +=
            std::count_if(std::next(row_data.cbegin()), row_data.cend(), [&default_value](auto &v) {
                return !MathHelper::equal(v, default_value);
            });

        table.rows.emplace_back(row_data);
    }

    if (non_default_count < 1) {
        notify_warning(fmt::format("{} to {} relative risk file has only default values.",
                                   source_code_str, target_code_str));
        return std::nullopt;
    }

    return table;
}

std::optional<RelativeRiskEntity>
DataManager::get_relative_risk_to_risk_factor(const DiseaseInfo &source, Gender gender,
                                              const core::Identifier &risk_factor_key) const {
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
        return std::nullopt;
    }

    rapidcsv::Document doc(filename);
    auto table = RelativeRiskEntity{.columns = doc.GetColumnNames(), .rows = {}};
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
        throw std::runtime_error(fmt::format("{}, {} parameters folder: '{}' not found.",
                                             info_code_str, country.name, files_folder.string()));
    }

    auto table = CancerParameterEntity();
    for (const auto &file : params_files.items()) {
        auto file_name = (files_folder / file.value().get<std::string>());
        if (!std::filesystem::exists(file_name)) {
            throw std::runtime_error(fmt::format("{}, {} parameters file: '{}' not found.",
                                                 info_code_str, country.name, file_name.string()));
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
    auto nodepath = index_["demographic"]["path"].get<std::string>();
    auto filefolder = index_["demographic"]["indicators"]["path"].get<std::string>();
    auto filename = index_["demographic"]["indicators"]["file_name"].get<std::string>();

    // Tokenized file name Pi{COUNTRY_CODE}.xxx
    auto tokens = std::vector<std::string>{std::to_string(country.code)};
    filename = replace_string_tokens(filename, tokens);
    filename = (root_ / nodepath / filefolder / filename).string();
    if (!std::filesystem::exists(filename)) {
        throw std::runtime_error(fmt::format("{}, demographic indicators file: '{}' not found.",
                                             country.name, filename));
    }

    rapidcsv::Document doc(filename);
    auto mapping = create_fields_index_mapping(doc.GetColumnNames(), {"Time", "Births", "SRB"});
    std::vector<BirthItem> result;
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

    return result;
}

std::vector<LmsDataRow> DataManager::get_lms_parameters() const {
    auto analysis_folder = index_["analysis"]["path"].get<std::string>();
    auto lms_filename = index_["analysis"]["lms_file_name"].get<std::string>();
    auto full_filename = (root_ / analysis_folder / lms_filename);
    if (!std::filesystem::exists(full_filename)) {
        throw std::runtime_error(
            fmt::format("LMS parameters file: '{}' not found.", full_filename.string()));
    }

    rapidcsv::Document doc(full_filename.string());
    auto mapping = create_fields_index_mapping(doc.GetColumnNames(),
                                               {"age", "gender_id", "lambda", "mu", "sigma"});
    auto parameters = std::vector<LmsDataRow>{};
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
    auto analysis_folder = index_["analysis"]["path"].get<std::string>();
    auto disability_filename = index_["analysis"]["disability_file_name"].get<std::string>();
    const auto &cost_node = index_["analysis"]["cost_of_disease"];

    auto local_root_path = (root_ / analysis_folder);
    disability_filename = (local_root_path / disability_filename).string();
    if (!std::filesystem::exists(disability_filename)) {
        throw std::runtime_error(
            fmt::format("Disease disability weights file: '{}' not found.", disability_filename));
    }

    rapidcsv::Document doc(disability_filename);
    DiseaseAnalysisEntity entity;
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

std::map<int, std::map<Gender, double>>
DataManager::load_cost_of_diseases(const Country &country, const nlohmann::json &node,
                                   const std::filesystem::path &parent_path) const {
    auto cost_path = node["path"].get<std::string>();
    auto filename = node["file_name"].get<std::string>();

    // Tokenized file name BoD{COUNTRY_CODE}.xxx
    auto tokens = std::vector<std::string>{std::to_string(country.code)};
    filename = replace_string_tokens(filename, tokens);
    filename = (parent_path / cost_path / filename).string();
    if (!std::filesystem::exists(filename)) {
        throw std::runtime_error(
            fmt::format("{} cost of disease file: '{}' not found.", country.name, filename));
    }

    rapidcsv::Document doc(filename);
    std::map<int, std::map<Gender, double>> result;
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
    auto nodepath = index_["demographic"]["path"].get<std::string>();
    auto filefolder = index_["demographic"]["indicators"]["path"].get<std::string>();
    auto filename = index_["demographic"]["indicators"]["file_name"].get<std::string>();

    // Tokenized file name Pi{COUNTRY_CODE}.xxx
    auto tokens = std::vector<std::string>{std::to_string(country.code)};
    filename = replace_string_tokens(filename, tokens);
    filename = (root_ / nodepath / filefolder / filename).string();
    if (!std::filesystem::exists(filename)) {
        throw std::runtime_error(fmt::format("{}, demographic indicators file: '{}' not found.",
                                             country.name, filename));
    }

    rapidcsv::Document doc(filename);
    auto mapping =
        create_fields_index_mapping(doc.GetColumnNames(), {"Time", "LEx", "LExMale", "LExFemale"});
    std::vector<LifeExpectancyItem> result;
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

std::optional<PIFData> DataManager::get_pif_data(const DiseaseInfo &disease_info,
                                                 const Country &country,
                                                 const nlohmann::json &pif_config) const {
    // Check if PIF is enabled
    if (!pif_config.contains("enabled") || !pif_config["enabled"].get<bool>()) {
        return std::nullopt;
    }

    // Get PIF configuration
    auto data_root_path = pif_config["data_root_path"].get<std::string>();
    auto risk_factor = pif_config["risk_factor"].get<std::string>();
    auto scenario = pif_config["scenario"].get<std::string>();

    // Expand environment variables in the path
    data_root_path = expand_environment_variables(data_root_path);

    // Construct PIF data path:
    // {data_root_path}/diseases/{disease_code}/PIF/{risk_factor}/{scenario}
    auto pif_path = construct_pif_path(disease_info.code.to_string(), pif_config);

    // Look for PIF CSV file in the scenario folder
    // File naming convention: IF{country_code}.csv (e.g., IF356.csv for India)
    auto country_code = std::to_string(country.code);
    auto csv_filename = "IF" + country_code + ".csv";
    auto full_path = pif_path / csv_filename;

    if (std::filesystem::exists(full_path)) {
        try {
            auto pif_table = load_pif_from_csv(full_path);
            if (pif_table.has_data()) {
                PIFData pif_data;
                pif_data.add_scenario_data(scenario, std::move(pif_table));

                // Print verification message
                fmt::print(fg(fmt::color::green), "PIF Data Loaded Successfully:\n");
                fmt::print("  - Disease: {}\n", disease_info.code.to_string());
                fmt::print("  - Country: {} (Code: {})\n", country.name, country_code);
                fmt::print("  - Risk Factor: {}\n", risk_factor);
                fmt::print("  - Scenario: {}\n", scenario);
                fmt::print("  - Toatal Data Rows: {}\n",
                           pif_data.get_scenario_data(scenario)->size());
                fmt::print("  - File: {}\n", csv_filename);
                fmt::print("  - Path: {}\n", full_path.string());
                fmt::print("  - PIF Analysis: ENABLED and READY\n\n");

                return pif_data;
            }
        } catch (const std::exception &e) {
            notify_warning(
                fmt::format("Failed to load PIF data from {}: {}", full_path.string(), e.what()));
        }
    }

    notify_warning(fmt::format(
        "PIF data not found for disease {} in scenario {} at path {} (expected file: {})",
        disease_info.code.to_string(), scenario, pif_path.string(), csv_filename));
    return std::nullopt;
}

std::filesystem::path DataManager::construct_pif_path(const std::string &disease_code,
                                                      const nlohmann::json &pif_config) const {
    auto data_root_path = pif_config["data_root_path"].get<std::string>();
    auto risk_factor = pif_config["risk_factor"].get<std::string>();
    auto scenario = pif_config["scenario"].get<std::string>();

    // Expand environment variables
    data_root_path = expand_environment_variables(data_root_path);

    // Construct path: {data_root_path}/diseases/{disease_code}/PIF/{risk_factor}/{scenario}
    return std::filesystem::path(data_root_path) / "diseases" / disease_code / "PIF" / risk_factor /
           scenario;
}

std::string DataManager::expand_environment_variables(const std::string &path) const {
    std::string result = path;

    // Find ${VAR_NAME} patterns and replace with environment variables
    std::regex env_var_regex(R"(\$\{([^}]+)\})");
    std::smatch match;

    while (std::regex_search(result, match, env_var_regex)) {
        std::string var_name = match[1].str();
        const char *env_value = std::getenv(var_name.c_str());

        if (env_value) {
            result.replace(match.position(), match.length(), env_value);
        } else {
            // If environment variable not found, keep the original placeholder
            notify_warning(
                fmt::format("Environment variable {} not found, using placeholder", var_name));
        }
    }

    return result;
}

PIFTable DataManager::load_pif_from_csv(const std::filesystem::path &filepath) const {
    PIFTable table;

    try {
        rapidcsv::Document doc(filepath.string());
        auto mapping = create_fields_index_mapping(doc.GetColumnNames(),
                                                   {"Gender", "Age", "YearPostInt", "IF_Mean"});

        for (size_t i = 0; i < doc.GetRowCount(); i++) {
            auto row = doc.GetRow<std::string>(i);

            PIFDataItem item{};
            // Convert CSV gender to core::Gender enum
            // CSV: 0=male, 1=female
            int csv_gender = std::stoi(row[mapping["Gender"]]);
            item.gender = (csv_gender == 0) ? core::Gender::male : core::Gender::female;
            item.age = std::stoi(row[mapping["Age"]]);
            item.year_post_intervention = std::stoi(row[mapping["YearPostInt"]]);
            item.pif_value = std::stod(row[mapping["IF_Mean"]]);

            // Validate PIF value is in range [0.0, 1.0]
            if (item.pif_value < 0.0 || item.pif_value > 1.0) {
                notify_warning(fmt::format("PIF value {} out of range [0.0, 1.0] at row {}",
                                           item.pif_value, i + 1));
                item.pif_value = std::clamp(item.pif_value, 0.0, 1.0);
            }

            table.add_item(item);
        }

        // PHASE 1 OPTIMIZATION: Build hash table for O(1) lookups
        // This dramatically improves performance from O(n) to O(1) for PIF lookups
        table.build_hash_table();

        // Print CSV loading verification
        fmt::print(fg(fmt::color::cyan), "PIF CSV File Loaded: {} ({} rows)\n",
                   filepath.filename().string(), table.size());
        fmt::print(fg(fmt::color::green),
                   "PIF Hash Table Built: O(1) lookup optimization enabled\n");

    } catch (const std::exception &e) {
        throw std::runtime_error(
            fmt::format("Failed to load PIF CSV file {}: {}", filepath.string(), e.what()));
    }

    return table;
}

} // namespace hgps::input
