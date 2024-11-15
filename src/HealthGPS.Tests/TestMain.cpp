#include "data_config.h"
#include "pch.h"

#include <cxxopts.hpp>
#include <filesystem>
#include <iostream>

cxxopts::Options create_options() {
    cxxopts::Options options("HealthGPS.Test",
                             "Health-GPS microsimulation for policy options test.");
    options.add_options()("s,storage", "Path to root folder of the data storage.",
                          cxxopts::value<std::string>())("help",
                                                         "Help about this test application.");

    return options;
}

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "\nInitialising with a custom GTest main function.\n\n";

    auto options = create_options();
    auto result = options.parse(argc, argv);
    auto storage_path = std::filesystem::path{};
    if (result.count("help")) {
        std::cout << options.help() << '\n';
        return EXIT_SUCCESS;
    }
    if (result.count("storage")) {
        storage_path = std::filesystem::path{result["storage"].as<std::string>()};
        if (storage_path.is_relative()) {
            storage_path = std::filesystem::absolute(storage_path);
        }
    } else {
        storage_path = TEST_DATA_PATH;
        std::cout << "Using default test data store ...\n\n";
    }

    auto start_path = std::filesystem::current_path();
    std::cout << "Test location..: " << start_path.string() << "\n";
    if (std::filesystem::exists(storage_path)) {
        std::cout << "Test data store: " << storage_path.string() << "\n\n";
        test_datastore_path = storage_path.string();
    } else {
        std::cerr << "Test data store: " << storage_path.string() << " *** not found ***.\n\n";
    }

    return RUN_ALL_TESTS();
}
