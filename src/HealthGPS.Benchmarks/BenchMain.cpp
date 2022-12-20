#include <benchmark/benchmark.h>
#include <iostream>
#include <thread>
#include <new>

#include "string_benchs.h"
#include "identifier_benchs.h"

int main(int argc, char** argv)
{
	::benchmark::Initialize(&argc, argv);

	std::cout << "\nInitialising benchmarks with a custom main function.\n\n";
	std::cout << "Hardware: " << '\n';
	std::cout << "Concurrent threads............: " << std::thread::hardware_concurrency() << '\n';

	::benchmark::RunSpecifiedBenchmarks();
	::benchmark::Shutdown();
}