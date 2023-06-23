#include <benchmark/benchmark.h>
#include <iostream>
#include <new>
#include <thread>

#include "identifier_benchs.h"
#include "string_benchs.h"

int main(int argc, char **argv) {
    ::benchmark::Initialize(&argc, argv);

    std::cout << "\nInitialising benchmarks with a custom main function.\n\n";
    std::cout << "Hardware: " << '\n';
    std::cout << "Concurrent threads............: " << std::thread::hardware_concurrency() << '\n';

    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
}