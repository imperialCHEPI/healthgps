#include "datamanager.h"
#include "download_file.h"
#include "schema.h"
#include "zip_file.h"

#include "HealthGPS.Core/math_util.h"
#include "HealthGPS.Core/string_util.h"

#include <fmt/color.h>
#include <rapidcsv.h>

#include <fstream>
#include <utility>

// structure of many of the functions below basically the same.
// create a vector of structs of whatever data type / class we're loading (e.g. Country, PopulationItem). Name this vector "results". 
// Define the file path (with reference to index.json) and file name for each data source. Load this data.
// Select the columns (variables) that are required. Each row will form one entry into the results vector. 
// And each variable selected will have a counterpart in whatever data type / class we're loading 

namespace {
//! The name of the index file
constexpr const char *IndexFileName = "index.json";

//! The name of the index.json schema file
constexpr const char *DataIndexSchemaFileName = "data_index.json";

//! The version of the index.json schema file
constexpr int DataIndexSchemaVersion = 1;

nlohmann::json read_input_files_from_directory(const std::filesystem::path &data_path) 
{
    return hgps::input::load_and_validate_json(data_path / IndexFileName, DataIndexSchemaFileName, DataIndexSchemaVersion);
}
} // anonymous namespace

namespace hgps::input {

DataManager::DataManager(std::filesystem::path data_path, VerboseMode verbosity) /// called in program.cpp to create data_api.
    :   root_(std::move(data_path)),                        // i.e. root_ = data_path
        verbosity_(verbosity), 
        index_(read_input_files_from_directory(root_)) {}   // i.e. initialize "index_" json using read_input_files_from_directory with argument root_

std::vector<Country> DataManager::get_countries() const 
{
    auto results    = std::vector<Country>();                               // create a vector of structs of type Country, named "results". 
    auto filepath   = index_["country"]["path"].get<std::string>();         // get file path from index.json / "country" / "path" (which in that file amounts to a simple "", i.e. not a subfolder within root directory.)
    auto filename   = index_["country"]["file_name"].get<std::string>();    // get file name from index.json / "country" / "file_name" (which in that file amounts to "countries.csv")
    filename        = (root_ / filepath / filename).string();               // append file name with root and file path, hence filename = [rootdirectory]/countries.csv

    if (!std::filesystem::exists(filename)) 
        throw std::runtime_error(fmt::format("countries file: '{}' not found.", filename));

    rapidcsv::Document doc(filename); // load file. 
    auto mapping = create_fields_index_mapping(doc.GetColumnNames(), {"Code", "Name", "Alpha2", "Alpha3"}); //// create mapping between column names and the varaibles listed here (e.g. "Code" may be third column, "Alpha2" may be fourth, it is arbitrary)
    
    //// for each row of data, extract relevant variables defined above. 
    //// place them firstly in the corresponding varibles in the Country strucutre
    //// Secondly make another instance of the Country structure as the next entry of results vector.
    for (size_t i = 0; i < doc.GetRowCount(); i++) /// loop over rows
    {
        auto row = doc.GetRow<std::string>(i); // get row i
        results.push_back(Country{.code     = std::stoi(row[mapping["Code"]]),  // extract "Code"   to .code    member of Country
                                  .name     = row[mapping["Name"]],             // extract "Name"   to .name    member of Country
                                  .alpha2   = row[mapping["Alpha2"]],           // extract "Alpha2" to .alpha2  member of Country
                                  .alpha3   = row[mapping["Alpha3"]]});         // extract "Alpha3" to .alpha3  member of Country... and therefore create new instance of Country to be pushed back to results.
    }
    std::sort(results.begin(), results.end()); // don't understand why this is being sorted, or indeed which variable within Country class is being sorted on.

    return results;
}

Country DataManager::get_country(const std::string &alpha) const 
{
    auto c = get_countries();
    auto is_target = [&alpha](const hgps::core::Country &c) 
        {
            return core::case_insensitive::equals(c.alpha2, alpha) || core::case_insensitive::equals(c.alpha3, alpha);
        };

    auto country = std::find_if(c.begin(), c.end(), is_target);
    if (country == c.end()) 
        throw std::runtime_error(fmt::format("Target country: '{}' not found.", alpha));

    return *country;
}

std::vector<PopulationItem> DataManager::get_population(const Country &country) const 
{
    return DataManager::get_population(country, [](unsigned int) { return true; });
}

std::vector<PopulationItem> DataManager::get_population(const Country &country, std::function<bool(unsigned int)> time_filter) const 
{
    auto results    = std::vector<PopulationItem>();                                        // create a vector of structs of type PopulationItem, named "results". 
    auto nodepath   = index_["demographic"]["path"].get<std::string>();                     // get nodepath path from index.json / "demographic" / "path" (which in that file amounts to the "undb" subfolder within root directory.)
    auto filepath   = index_["demographic"]["population"]["path"].get<std::string>();       // get filepath path from index.json / "demographic" / "population" / "path" (which in that file amounts to the "population" subfolder within undb sub directory.)
    auto filename   = index_["demographic"]["population"]["file_name"].get<std::string>();  // get file name from index.json / "demographic" / "population" / "file_name" (which in that file amounts to "P{COUNTRY_CODE}.csv")
    
    filename = replace_string_tokens(filename, {std::to_string(country.code)});             // Tokenized file names X{country.code}X.xxx
    filename = (root_ / nodepath / filepath / filename).string();                           // append file name with root, node path and file path, hence filename = "[rootdirectory]/undb/population/P{COUNTRY_CODE}.csv"

    // LocID,Location,VarID,Variant,Time,MidPeriod,AgeGrp,AgeGrpStart,AgeGrpSpan,PopMale,PopFemale,PopTotal
    if (!std::filesystem::exists(filename)) 
        throw std::runtime_error(fmt::format("{} population file: '{}' not found.", country.name, filename));

    rapidcsv::Document doc(filename);
    auto mapping = create_fields_index_mapping(doc.GetColumnNames(), {"LocID", "Time", "Age", "PopMale", "PopFemale", "PopTotal"});  //// create mapping between column names and the varaibles listed here

   //// for each row of data, extract relevant variables defined above. 
   //// place them firstly in the corresponding varibles in the PopulationItem strucutre
   //// Secondly make another instance of the PopulationItem structure as the next entry of results vector.
   for (size_t i = 0; i < doc.GetRowCount(); i++) 
    {
        auto row        = doc.GetRow<std::string>(i); // get row i
        auto row_time   = std::stoi(row[mapping["Time"]]);

        if (!time_filter(row_time))     continue;

        results.push_back(PopulationItem{.location_id   = std::stoi(row[mapping["LocID"]]),
                                         .at_time       = row_time,
                                         .with_age      = std::stoi(row[mapping["Age"]]),
                                         .males         = std::stof(row[mapping["PopMale"]]),
                                         .females       = std::stof(row[mapping["PopFemale"]]),
                                         .total         = std::stof(row[mapping["PopTotal"]])});
    }
    std::sort(results.begin(), results.end());
    return results;
}

std::vector<MortalityItem> DataManager::get_mortality(const Country &country) const 
{
    return DataManager::get_mortality(country, [](unsigned int) { return true; });
}

std::vector<MortalityItem> DataManager::get_mortality(const Country &country, std::function<bool(unsigned int)> time_filter) const 
{
    //// See previous functions get_population and get_countries where logic is the same
    auto results    = std::vector<MortalityItem>();
    auto nodepath   = index_["demographic"]["path"].get<std::string>();
    auto filepath   = index_["demographic"]["mortality"]["path"].get<std::string>();
    auto filename   = index_["demographic"]["mortality"]["file_name"].get<std::string>();

    // Tokenized file names X{country.code}X.xxx
    filename = replace_string_tokens(filename, {std::to_string(country.code)});
    filename = (root_ / nodepath / filepath / filename).string();

    if (!std::filesystem::exists(filename)) 
        throw std::runtime_error(fmt::format("{} mortality file: '{}' not found.", country.name, filename));

    rapidcsv::Document doc(filename);
    auto mapping = create_fields_index_mapping(doc.GetColumnNames(), {"LocID", "Time", "Age", "DeathMale", "DeathFemale", "DeathTotal"});
    
    for (size_t i = 0; i < doc.GetRowCount(); i++) 
    {
        auto row        = doc.GetRow<std::string>(i);
        auto row_time   = std::stoi(row[mapping["Time"]]);
        if (!time_filter(row_time))     continue;

        results.push_back(MortalityItem{.location_id    = std::stoi(row[mapping["LocID"]]),
                                        .at_time        = row_time,
                                        .with_age       = std::stoi(row[mapping["Age"]]),
                                        .males          = std::stof(row[mapping["DeathMale"]]),
                                        .females        = std::stof(row[mapping["DeathFemale"]]),
                                        .total          = std::stof(row[mapping["DeathTotal"]])});
    }
    std::sort(results.begin(), results.end());

    return results;
}

std::vector<DiseaseInfo> DataManager::get_diseases() const 
{
    auto result = std::vector<DiseaseInfo>();   // create a vector of structs of type DiseaseInfo, named "result". 

    const auto &registry = index_["diseases"]["registry"]; // Import part of index (diseases/registry) and store it as a json, named registry.

    for (const auto &item : registry) // loop through each item (list) within registry
    {
        // declare (empty) variables that will be added to result vector, 
        auto info       = DiseaseInfo{}; //// Not sure why this syntax is preferred to "DiseaseInfo info;" 
        auto group_str  = std::string{}; //// Likewise, not sure why this syntax is preferred to "std::string group_str;" 
        auto code_srt   = std::string{}; //// ditto

        // extract variables from registry item to variables declared above.
        item["group"].get_to(group_str);
        item["id"].get_to(code_srt);
        item["name"].get_to(info.name);

        info.group  = DiseaseGroup::other;
        info.code   = Identifier{code_srt};

        if (core::case_insensitive::equals(group_str, "cancer")) 
            info.group = DiseaseGroup::cancer;

        result.emplace_back(info); //// add new instance of DiseaseInfo (named "info") to result vector.
    }

    std::sort(result.begin(), result.end());

    return result;
}

DiseaseInfo DataManager::get_disease_info(const core::Identifier &code) const 
{
    /// THIS FUNCTION GETS DISEASE METADATA (very little actually there) from index.json, whereas "get_disease" actually loads disease data (e.g. mortaility, prevalence etc. into Cpp). 

    // gets the info (essentially whether it's a cancer or not) for a given disease from the registry. 
    // appears very similar to both DataManager::get_diseases above and to CachedRepository::get_disease_info and DataManager::get_disease() below. not sure what difference is.

    const auto &registry            = index_["diseases"]["registry"]; // Import part of index (diseases/registry) and store it as a json, named registry.
    const auto &disease_code_str    = code.to_string(); // code/ string for this disease
    auto info                       = DiseaseInfo{}; // create empty instance of DiseaseInfo structure

    /// loop over all diseases in (sub) registry, but only do anything for the disease that matches disease_code_str above, at which point stop.
    for (const auto &item : registry) 
    {
        auto item_code_str = std::string{};
        item["id"].get_to(item_code_str);

        if (item_code_str == disease_code_str) 
        {
            auto group_str = std::string{};
            item["group"].get_to(group_str);
            item["name"].get_to(info.name);

            info.code   = code;
            info.group  = DiseaseGroup::other;
            
            if (core::case_insensitive::equals(group_str, "cancer")) 
                info.group = DiseaseGroup::cancer;

            return info;
        }
    }
    throw std::runtime_error(fmt::format("Disease code: '{}' not found.", code.to_string())); /// throw an error if loop finishes without internal if statement having been satisfied.
}

DiseaseEntity DataManager::get_disease(const DiseaseInfo &info, const Country &country) const 
{
    /// this function will load data (i.e. prevalence, incidence, mortality and remission) for a particular disease (specified by function argument "DiseaseInfo info") 
    /// and store it in a DiseaseEntity structure

    DiseaseEntity result;       // create empty instance of DiseaseEntity structure named result
    result.info     = info;     // put info argument into result (DiseaseInfo subclass of DiseaseEntity)
    result.country  = country;  // put country argument into result (DiseaseInfo subclass of DiseaseEntity)

    auto diseases_path  = index_["diseases"]["path"].get<std::string>();                    // get subfolder containing all disease data
    auto disease_folder = index_["diseases"]["disease"]["path"].get<std::string>();         // get subfolder for this disease
    auto filename       = index_["diseases"]["disease"]["file_name"].get<std::string>();    // get filename of dataset D{COUNTRY_CODE}.csv

    // Tokenized folder name X{info.code}X
    disease_folder  = replace_string_tokens(disease_folder, {info.code.to_string()});

    // Tokenized file names X{country.code}X.xxx
    filename        = replace_string_tokens(filename, {std::to_string(country.code)});
    filename        = (root_ / diseases_path / disease_folder / filename).string();         // concatenate to get full file name and directory
    if (!std::filesystem::exists(filename)) 
        throw std::runtime_error(fmt::format("{}, {} file: '{}' not found.", country.name, info.name, filename));

    rapidcsv::Document doc(filename);

    auto table      = std::unordered_map<int, std::unordered_map<core::Gender, std::map<int, double>>>();
    auto mapping    = create_fields_index_mapping(doc.GetColumnNames(), {"age", "gender_id", "measure_id", "measure", "mean"});

    for (size_t i = 0; i < doc.GetRowCount(); i++) 
    {
        auto row            = doc.GetRow<std::string>(i);  // get row i

        auto age            = std::stoi(row[mapping["age"]]);
        auto gender         = static_cast<core::Gender>(std::stoi(row[mapping["gender_id"]]));
        auto measure_id     = std::stoi(row[mapping["measure_id"]]);
        auto measure_name   = core::to_lower(row[mapping["measure"]]);
        auto measure_value  = std::stod(row[mapping["mean"]]);

        if (!result.measures.contains(measure_name)) 
            result.measures.emplace(measure_name, measure_id);

        if (!table[age].contains(gender)) 
            table[age].emplace(gender, std::map<int, double>{});

        table[age][gender][measure_id] = measure_value;
    }

    for (auto &pair : table) 
        for (auto &child : pair.second) 
            result.items.emplace_back(DiseaseItem{.with_age = pair.first, .gender = child.first, .measures = child.second});

    result.items.shrink_to_fit();

    return result;
}

std::optional<RelativeRiskEntity> DataManager::get_relative_risk_to_disease(const DiseaseInfo &source, const DiseaseInfo &target) const 
{
    //// This function imports relative risk of one disease (specified by argument "target) given presence of another (specified by argument "source") 

    //// lines below get filenames, file paths etc.
    auto diseases_path      = index_["diseases"]["path"].get<std::string>();
    auto disease_folder     = index_["diseases"]["disease"]["path"].get<std::string>();
    const auto &risk_node   = index_["diseases"]["disease"]["relative_risk"];
    auto default_value      = risk_node["to_disease"]["default_value"].get<float>();

    auto risk_folder    = risk_node["path"].get<std::string>();
    auto file_folder    = risk_node["to_disease"]["path"].get<std::string>();
    auto filename       = risk_node["to_disease"]["file_name"].get<std::string>();

    // Tokenized folder name X{info.code}X
    auto source_code_str = source.code.to_string();
    disease_folder = replace_string_tokens(disease_folder, std::vector<std::string>{source_code_str});

    // Tokenized file name{ DISEASE_TYPE }_{ DISEASE_TYPE }.xxx
    auto target_code_str    = target.code.to_string();
    auto tokens             = {source_code_str, target_code_str};
    filename                = replace_string_tokens(filename, tokens);

    filename = (root_ / diseases_path / disease_folder / risk_folder / file_folder / filename).string(); // concatenate to get full file name and directory
    if (!std::filesystem::exists(filename)) 
    {
        notify_warning(fmt::format("{} to {} relative risk file not found", source_code_str, target_code_str));
        return std::nullopt;
    }
    rapidcsv::Document doc(filename);

    auto table      = RelativeRiskEntity();  // create empty instance of RelativeRiskEntity structure called table
    table.columns   = doc.GetColumnNames();

    auto row_size           = table.columns.size();
    auto non_default_count  = std::size_t{0};

    for (size_t i = 0; i < doc.GetRowCount(); i++) 
    {
        auto row        = doc.GetRow<std::string>(i); // get row i
        auto row_data   = std::vector<float>(row_size); // don't need to allocate this vector for all rows of this loop - can instead overwrite or even zero out.

        for (size_t col = 0; col < row_size; col++) 
            row_data[col] = std::stof(row[col]);

        non_default_count += std::count_if(std::next(row_data.cbegin()), row_data.cend(), [&default_value](auto &v) 
            {
                return !MathHelper::equal(v, default_value);
            });

        table.rows.emplace_back(row_data);
    }

    if (non_default_count < 1) 
    {
        notify_warning(fmt::format("{} to {} relative risk file has only default values.", source_code_str, target_code_str));
        return std::nullopt;
    }
    return table;
}

std::optional<RelativeRiskEntity> DataManager::get_relative_risk_to_risk_factor(const DiseaseInfo &source, Gender gender, const core::Identifier &risk_factor_key) const 
{
    //// This function imports relative risk of one disease (specified by argument "source") given presence of a particular risk factor (specified by argument risk_factor_key)

    //// lines below get filenames, file paths etc.
    auto diseases_path      = index_["diseases"]["path"].get<std::string>();
    auto disease_folder     = index_["diseases"]["disease"]["path"].get<std::string>();
    const auto &risk_node   = index_["diseases"]["disease"]["relative_risk"];

    auto risk_folder    = risk_node["path"].get<std::string>();
    auto file_folder    = risk_node["to_risk_factor"]["path"].get<std::string>();
    auto filename       = risk_node["to_risk_factor"]["file_name"].get<std::string>();

    // Tokenized folder name X{info.code}X
    auto source_code_str    = source.code.to_string();
    disease_folder          = replace_string_tokens(disease_folder, {source_code_str});

    // Tokenized file name {GENDER}_{DISEASE_TYPE}_{RISK_FACTOR}.xxx
    auto factor_key_str     = risk_factor_key.to_string();
    std::string gender_name = gender == Gender::male ? "male" : "female";
    auto tokens             = {gender_name, source_code_str, factor_key_str};
    
    filename = replace_string_tokens(filename, tokens);
    filename = (root_ / diseases_path / disease_folder / risk_folder / file_folder / filename).string(); // concatenate to get full file name and directory

    if (!std::filesystem::exists(filename)) 
    {
        notify_warning(fmt::format("{} to {} relative risk file not found, disabled.", source_code_str, factor_key_str));
        return std::nullopt;
    }

    rapidcsv::Document doc(filename);
    auto table      = RelativeRiskEntity{.columns = doc.GetColumnNames(), .rows = {}};  // create empty instance of RelativeRiskEntity structure called table
    auto row_size   = table.columns.size();

    for (size_t i = 0; i < doc.GetRowCount(); i++) 
    {
        auto row        = doc.GetRow<std::string>(i);
        auto row_data   = std::vector<float>(row_size);

        for (size_t col = 0; col < row_size; col++) 
            row_data[col] = std::stof(row[col]);

        table.rows.emplace_back(row_data);
    }

    table.rows.shrink_to_fit();
    table.columns.shrink_to_fit();
    return table;
}

CancerParameterEntity DataManager::get_disease_parameter(const DiseaseInfo &info, const Country &country) const 
{
    //// THis function is called for cancers, which have extra subfolders for each country containing three files: 
    ////    i) prevalence_distribution.csv (guessing this is the prevalence of people with this cancer currently in stages 1,2,3 and 4); 
    ////    ii) survival_rate_parameters.csv (appears to be the coefficients of the polynomials used to fit survival rates); iii) 
    ////    iii) death_weights.csv (guessing this is the proportion of deaths that occur in each cancer stage seeing as they sum to 1 for each sex)
    //// The function then loops over each file and imports them alll into one CancerParameterEntity structure named table, which is the return value.

    // Creates full file name from store configuration
    auto disease_path   = index_["diseases"]["path"].get<std::string>();
    auto disease_folder = index_["diseases"]["disease"]["path"].get<std::string>();

    const auto &params_node     = index_["diseases"]["disease"]["parameters"];
    auto params_folder          = params_node["path"].get<std::string>();
    const auto &params_files    = params_node["files"];

    // Tokenized folder name X{info.code}X
    auto info_code_str  = info.code.to_string();
    disease_folder      = replace_string_tokens(disease_folder, {info_code_str});

    // Tokenized path name P{COUNTRY_CODE}
    auto tokens     = std::vector<std::string>{std::to_string(country.code)};
    params_folder       = replace_string_tokens(params_folder, tokens);
    auto files_folder   = (root_ / disease_path / disease_folder / params_folder);
    if (!std::filesystem::exists(files_folder)) 
        throw std::runtime_error(fmt::format("{}, {} parameters folder: '{}' not found.", info_code_str, country.name, files_folder.string()));

    auto table = CancerParameterEntity();  // create empty instance of CancerParameterEntity structure called table, into which we import the various files.

    for (const auto &file : params_files.items()) 
    {
        auto file_name = (files_folder / file.value().get<std::string>());

        if (!std::filesystem::exists(file_name)) 
            throw std::runtime_error(fmt::format("{}, {} parameters file: '{}' not found.", info_code_str, country.name, file_name.string()));

        auto lookup = std::vector<LookupGenderValue>{};

        rapidcsv::Document doc(file_name.string());
        auto mapping = create_fields_index_mapping(doc.GetColumnNames(), {"Time", "Male", "Female"});

        for (size_t i = 0; i < doc.GetRowCount(); i++) 
        {
            auto row = doc.GetRow<std::string>(i);
            lookup.emplace_back(LookupGenderValue{
                .value  = std::stoi(row[mapping["Time"]]),
                .male   = std::stod(row[mapping["Male"]]),
                .female = std::stod(row[mapping["Female"]]),
            });
        }

             if (file.key() == "distribution")      table.prevalence_distribution   = lookup;
        else if (file.key() == "survival_rate")     table.survival_rate             = lookup;
        else if (file.key() == "death_weight")      table.death_weight              = lookup;
        else                                        throw std::out_of_range(fmt::format("Unknown disease parameter file type: {}", file.key()));
    }

    index_["diseases"]["time_year"].get_to(table.at_time);
    return table;
}

std::vector<BirthItem> DataManager::get_birth_indicators(const Country &country) const 
{
    return DataManager::get_birth_indicators(country, [](unsigned int) { return true; });
}

std::vector<BirthItem> DataManager::get_birth_indicators(const Country &country, std::function<bool(unsigned int)> time_filter) const 
{
    auto nodepath   = index_["demographic"]["path"].get<std::string>();
    auto filefolder = index_["demographic"]["indicators"]["path"].get<std::string>();
    auto filename   = index_["demographic"]["indicators"]["file_name"].get<std::string>();

    // Tokenized file name Pi{COUNTRY_CODE}.xxx
    auto tokens = std::vector<std::string>{std::to_string(country.code)};
    filename    = replace_string_tokens(filename, tokens);
    filename    = (root_ / nodepath / filefolder / filename).string();

    if (!std::filesystem::exists(filename)) 
        throw std::runtime_error(fmt::format("{}, demographic indicators file: '{}' not found.", country.name, filename));

    rapidcsv::Document doc(filename);
    auto mapping = create_fields_index_mapping(doc.GetColumnNames(), {"Time", "Births", "SRB"}); //// year, births that year, sex ratio at birth (male:female)

    std::vector<BirthItem> result;
    for (size_t i = 0; i < doc.GetRowCount(); i++) 
    {
        auto row        = doc.GetRow<std::string>(i);
        auto row_time   = std::stoi(row[mapping["Time"]]);

        if (!time_filter(row_time)) continue;

        result.push_back(BirthItem{.at_time     = row_time,
                                   .number      = std::stof(row[mapping["Births"]]),
                                   .sex_ratio   = std::stof(row[mapping["SRB"]])});
    }
    return result;
}

std::vector<LmsDataRow> DataManager::get_lms_parameters() const 
{
    // imports and stores the Lambda-Mu-Sigma (LMS) model parameters, which is used to convert BMI risk factor values to z-scores for children.

    auto analysis_folder    = index_["analysis"]["path"].get<std::string>();
    auto lms_filename       = index_["analysis"]["lms_file_name"].get<std::string>();
    auto full_filename      = (root_ / analysis_folder / lms_filename);

    if (!std::filesystem::exists(full_filename)) 
        throw std::runtime_error(fmt::format("LMS parameters file: '{}' not found.", full_filename.string()));

    rapidcsv::Document doc(full_filename.string());
    auto mapping    = create_fields_index_mapping(doc.GetColumnNames(), {"age", "gender_id", "lambda", "mu", "sigma"});
    auto parameters = std::vector<LmsDataRow>{};

    for (size_t i = 0; i < doc.GetRowCount(); i++) 
    {
        auto row = doc.GetRow<std::string>(i);
        if (row.size() < 6)     continue;

        parameters.emplace_back(
            LmsDataRow{.age     = std::stoi(row[mapping["age"]]),
                       .gender  = static_cast<core::Gender>(std::stoi(row[mapping["gender_id"]])),
                       .lambda  = std::stod(row[mapping["lambda"]]),
                       .mu      = std::stod(row[mapping["mu"]]),
                       .sigma   = std::stod(row[mapping["sigma"]])});
    }
    return parameters;
}

DiseaseAnalysisEntity DataManager::get_disease_analysis(const Country &country) const 
{
    auto analysis_folder        = index_["analysis"]["path"].get<std::string>();
    auto disability_filename    = index_["analysis"]["disability_file_name"].get<std::string>();
    const auto &cost_node       = index_["analysis"]["cost_of_disease"];

    auto local_root_path    = (root_ / analysis_folder);
    disability_filename     = (local_root_path / disability_filename).string();
    if (!std::filesystem::exists(disability_filename)) 
        throw std::runtime_error(fmt::format("Disease disability weights file: '{}' not found.", disability_filename));

    rapidcsv::Document doc(disability_filename);

    DiseaseAnalysisEntity entity;
    for (size_t i = 0; i < doc.GetRowCount(); i++) 
    {
        auto row = doc.GetRow<std::string>(i);
        if (row.size() < 2)     continue;

        entity.disability_weights.emplace(row[0], std::stof(row[1]));
    }

    entity.cost_of_diseases = load_cost_of_diseases(country, cost_node, local_root_path);
    entity.life_expectancy  = load_life_expectancy(country);
    return entity;
}

std::map<int, std::map<Gender, double>> DataManager::load_cost_of_diseases(const Country &country, const nlohmann::json &node, const std::filesystem::path &parent_path) const 
{
    auto cost_path  = node["path"].get<std::string>();
    auto filename   = node["file_name"].get<std::string>();

    // Tokenized file name BoD{COUNTRY_CODE}.xxx
    auto tokens = std::vector<std::string>{std::to_string(country.code)};
    filename    = replace_string_tokens(filename, tokens);
    filename    = (parent_path / cost_path / filename).string();

    if (!std::filesystem::exists(filename)) 
        throw std::runtime_error(fmt::format("{} cost of disease file: '{}' not found.", country.name, filename));

    rapidcsv::Document doc(filename);
    std::map<int, std::map<Gender, double>> result;

    for (size_t i = 0; i < doc.GetRowCount(); i++) 
    {
        auto row = doc.GetRow<std::string>(i);

        if (row.size() < 13)     continue; // 13 is a bit dubious.

        auto age            = std::stoi(row[6]);
        auto gender         = static_cast<core::Gender>(std::stoi(row[8]));
        result[age][gender] = std::stod(row[12]);
    }

    return result;
}

std::vector<LifeExpectancyItem> DataManager::load_life_expectancy(const Country &country) const 
{
    //// Imports the life expectancy by year (1950-2100) and sex

    auto nodepath   = index_["demographic"]["path"].get<std::string>();
    auto filefolder = index_["demographic"]["indicators"]["path"].get<std::string>();
    auto filename   = index_["demographic"]["indicators"]["file_name"].get<std::string>();

    // Tokenized file name Pi{COUNTRY_CODE}.xxx
    auto tokens = std::vector<std::string>{std::to_string(country.code)};
    filename    = replace_string_tokens(filename, tokens);
    filename    = (root_ / nodepath / filefolder / filename).string();
    
    if (!std::filesystem::exists(filename)) 
        throw std::runtime_error(fmt::format("{}, demographic indicators file: '{}' not found.", country.name, filename));

    rapidcsv::Document doc(filename);
    auto mapping = create_fields_index_mapping(doc.GetColumnNames(), {"Time", "LEx", "LExMale", "LExFemale"});
    
    std::vector<LifeExpectancyItem> result;
    for (size_t i = 0; i < doc.GetRowCount(); i++) 
    {
        auto row = doc.GetRow<std::string>(i);
        if (row.size() < 4)    continue;

        result.emplace_back(LifeExpectancyItem{.at_time = std::stoi(row[mapping["Time"]]),
                                               .both    = std::stof(row[mapping["LEx"]]),
                                               .male    = std::stof(row[mapping["LExMale"]]),
                                               .female  = std::stof(row[mapping["LExFemale"]])});
    }
    return result;
}

std::string DataManager::replace_string_tokens(const std::string &source, const std::vector<std::string> &tokens) 
{
    std::string output = source;
    std::size_t tk_end = 0;
    
    for (const auto &tk : tokens) 
    {
        auto tk_start = output.find_first_of('{', tk_end);
        if (tk_start != std::string::npos) 
        {
            tk_end = output.find_first_of('}', tk_start + 1);
            if (tk_end != std::string::npos) 
            {
                output = output.replace(tk_start, tk_end - tk_start + 1, tk);
                tk_end = tk_start + tk.length();
            }
        }
    }
    return output;
}

std::map<std::string, std::size_t> DataManager::create_fields_index_mapping(const std::vector<std::string> &column_names, const std::vector<std::string> &fields) 
{
    auto mapping = std::map<std::string, std::size_t>();

    for (const auto &field : fields) 
    {
        auto field_index = core::case_insensitive::index_of(column_names, field);
        if (field_index < 0) 
            throw std::out_of_range(fmt::format("File-based store, required field {} not found", field));
        mapping.emplace(field, field_index);
    }
    return mapping;
}

void DataManager::notify_warning(std::string_view message) const 
{
    if (verbosity_ == VerboseMode::none)   return;

    fmt::print(fg(fmt::color::dark_salmon), "File-based store, {}\n", message);
}
} // namespace hgps::input
