#include <benchmark/benchmark.h>
#include <iostream>

#include "string_benchs.h"

int main(int argc, char** argv)
{
	::benchmark::Initialize(&argc, argv);

	std::cout << "\nInitialising benchmarks with a custom main function.\n\n";

	::benchmark::RunSpecifiedBenchmarks();
	::benchmark::Shutdown();
}