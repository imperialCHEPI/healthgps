#include "pch.h"
#include "data_config.h"

#include <iostream>
#include <filesystem>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "\nInitialising with a custom GTest main function.\n\n";
    auto start_path = std::filesystem::current_path();
    std::cout << "Test location..: " << start_path.string() << "\n";
    std::cout << "Test data store: " << default_datastore_path() << "\n\n";

    return RUN_ALL_TESTS();
}