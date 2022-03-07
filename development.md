## Global Health Policy Simulation model

| [Home](index) | [Quick Start](getstarted) | [User Guide](userguide) | [Software Architecture](architecture) | [Data Model](datamodel) | Developer Guide |

# Developer Guide

The *Health GPS* software is written in modern, standard ANSI C++, targeting the [C++20 version](https://en.cppreference.com/w/cpp/20) and using the C++ Standard Library. The project is fully managed by [CMake](https://cmake.org/) and [Microsoft Visual Studio](https://visualstudio.microsoft.com), the code base is portable but requires a C++20 compatible compiler to build. The development toolset users [Ninja](https://ninja-build.org/) for build, [vcpkg](https://github.com/microsoft/vcpkg) package manager for dependencies, [googletest](https://github.com/google/googletest) for unit testing and [GitHub Actions](https://docs.github.com/en/actions) for automate build.

To start working on the *Health GPS* code base, the suggested development machine needs:
1. Windows 10 or newer, [WSL 2](https://docs.microsoft.com/en-us/windows/wsl/) enabled and Linux distribution of choice installed.
2. [Git](https://git-scm.com/downloads) 2.3 or newer.
3. [CMake](https://cmake.org/) 3.20 or newer with support for [presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html).
4. [Ninja](https://ninja-build.org/) 1.10 or newer.
5. [Microsoft Visual Studio](https://visualstudio.microsoft.com) 2019 or newer with CMake presets integration.
6. [GCC](https://gcc.gnu.org/) 11.1 or newer installed on *Linux* environment.
7. The latest [vcpkg](https://github.com/microsoft/vcpkg) installed globally for Visual Studio projects, and the VCPKG_ROOT environment variable set to the installation directory.
8. Internet connection.

Download the *Health GPS* source code to the local machine, we recommend somewhere like `C:\src` or `C:\source`, since otherwise you may run into path issues with the build systems.
```cmd
> git clone https://github.com/imperialCHEPI/healthgps
```
Finally, open the *'.../healthgps/source'* folder in Visual Studio and hit build. The first build takes considerably longer than normal due to the initial work required by CMake and the package manager. Alternatively, there is also support for visual studio projects by opening the solution file `.../healthgps/source/HelathGPS.sln`, but this option will be removed in future version.

**NOTE:** *This is the current toolset being used for developing the HealthGPS model, however CMake is supported by VS Code and many other IDE of choice, e.g. the model is current being compiled and built on Ubuntu Linux 20.04 LTS using only the CMake command line.*

### CMake Build

```cmd
cmake --list-presets=all

# Windows
cmake --preset='x64-release'
cmake --build --preset='release-build-windows'

# Linux
cmake --preset='linux-release'
cmake --build --preset='release-build-linux'
```

The `HealthGPS` binary will now be inside the `./out/build/[preset]/HealthGPS.Console` directory.

To run the unit tests:
```cmd
# Windows
cmake --preset='x64-debug'
cmake --build --preset='debug-build-windows'
ctest --preset='core-test-windows'

# Linux
cmake --preset='linux-debug'
cmake --build --preset='debug-build-linux'
ctest --preset='core-test-linux'
```
All available options for CMake *prestes* are defined in the `CMakePresets.json` file, which also declare many of the options previously provided to CMake via command line arguments. The use of presets provides consistent builds scripts across development and CI/CD environments using source control for reproducibility.

## Third-party components

The project dependencies are included using [vcpkg](https://github.com/microsoft/vcpkg) package manager, and declared in the *vcpkg.json* manifest file. The package manager is integrated with CMake and automatically invoked by the ```find_package``` command during build.

| Name  | License |
|:---   |:---     |
| [Adevs](https://sourceforge.net/projects/adevs)                            | BSD 3-Clause |
| [crossguid](https://github.com/graeme-hill/crossguid)                      | MIT          |
| [cxxopts](https://github.com/jarro2783/cxxopts)                            | MIT          |
| [indicators](https://github.com/p-ranav/indicators)                        | MIT          |
| [fmt](https://github.com/fmtlib/fmt)                                       | MIT          |
| [nlohmann-json](https://github.com/nlohmann/json)                          | MIT          |
| [rapidcsv](https://github.com/d99kris/rapidcsv)                            | BSD 3-Clause |
| [googletest](https://github.com/google/googletest)   | BSD 3-Clause |

# Implementation

The software application provides a Command Line Interface (CLI) for the user to inform the configuration to run and backend storage location. The experiment options are provided to the model via a configuration file (JSON format), including population size, intervention scenarios and number of runs.

 The console terminal application aims to provide the users of with a wider range of cross-platform options to run the microsimulation, including hardware ranging from desktops to high performance computers. However, the microsimulation software program can equality be a graphical user interface (GUI) or web page program.

## Composing a Microsimulation
To run a microsimulation experiment, at least one simulation engine and one simulation executive must be created, the HealthGPS class implements the engine, and ModelRunner class implements the executive respectively as shown below. To create a simulation engine instance, the user must provide a SimulationDefinition with the model configuration, the SimulationModuleFactory with builders for each module type registered, and one implementation of the EventAggregator interface for external communication.

|![Composing Health GPS](/assets/image/compose_simulation.png)|
|:--:|
|*Composing a Health GPS Microsimulation*|

The simulation executive requires a RandomBitGenerator interface implementation for master seed generation and an implementation of the EventAggregator interface, in this example the DefaultEventBus class, which should be shared by the engines and executive to provide a centralised source of communication. The simulation engine must have its own random number generator instance as part of the simulation definition, the Mersenne Twister pseudorandom number generator algorithms is the default implementation, however other algorithms can easily be used. 

EventMonitor class has been created to receive all messages from the microsimulation, notifications and error messages are displayed on the application terminal, and result messages are queued to be processed by an implementation of the ResultWriter interface, ResultFileWriter class in this example, which writes the results to a file in JSON format.

The following code snippet shows how to compose a microsimulation using the classes discussed above. The modules factory holds the backend datastore instance, and allows dynamic registration of implementations for the required module types, the default module factory function registers the current production implementations. The contents of the input configuration file is loaded and processed to create the model input, a read-only data structure shared with all the simulation engines. An implementation of *scenario* interface must be provided for each simulation definition, the *BaselineScenario* class is a generic type, while the intervention scenarios are defined to test specific policies.

```cpp
// Create factory with backend data store and modules implementation
auto factory = get_default_simulation_module_factory(...);

// Create the complete model input from configuration
auto model_input = create_model_input(...);

// Create event bus and monitor
auto event_bus = DefaultEventBus();
auto json_file_logger = ResultFileWriter{
    create_output_file_name(config.output),
    ExperimentInfo{.model = "Health GPS", .version = "1.0.0.0",
        .intervention = config.intervention.identifier}
};
auto event_monitor = EventMonitor{ event_bus, json_file_logger };

try {
    // Create simulation executive
    auto seed_generator = std::make_unique<hgps::MTRandom32>();
    if (model_input.seed().has_value()) {
        seed_generator->seed(model_input.seed().value());
    }
    auto executive = ModelRunner(event_bus, std::move(seed_generator));
    
    // Create baseline scenario with data sync channel
    auto channel = SyncChannel{};
    auto baseline_scenario = std::make_unique<BaselineScenario>(channel);
    
    // Create simulation engine for baseline scenario
    auto baseline_rnd = std::make_unique<hgps::MTRandom32>();
    auto baseline = HealthGPS{
        SimulationDefinition{
            model_input, std::move(baseline_scenario),
            std::move(baseline_rnd)},
        factory, event_bus };

    std::atomic<bool> done(false);
    auto runtime = 0.0;
    if (config.has_active_intervention) {
        // Create intervention scenario
        auto policy_scenario = create_intervention_scenario(channel, config.intervention);

        // Create simulation engine for intervention scenario
        auto policy_rnd = std::make_unique<hgps::MTRandom32>();
        auto intervention = HealthGPS{
            SimulationDefinition{
                model_input, std::move(policy_scenario),
                std::move(policy_rnd)},
            factory, event_bus };

        // Create worker thread to run the two scenarios side by side
        auto worker = std::jthread{ [&runtime, &executive, &baseline, &intervention, &config, &done] {
            runtime = executive.run(baseline, intervention, config.trial_runs);
            done.store(true);
        } };

        // Waits for it to finish, cancellation can be enabled here
        while (!done.load()) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        worker.join();
    }
    else {
        // Create worker thread to run only the baseline scenario
        channel.close(); // Will not store any message
        auto worker = std::jthread{[&runtime, &executive, &baseline, &config, &done] {
            runtime = executive.run(baseline, config.trial_runs);
            done.store(true);
        } };
        
        // Waits for it to finish, cancellation can be enabled here
        while (!done.load()) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        worker.join();
    }
}
catch (const std::exception& ex) {
    fmt::print(fg(fmt::color::red), "\n\nFailed with message - {}.\n", ex.what());
}

// Stop listening for new messages.
event_monitor.stop();
```

The simulation executive can run experiments for baseline scenario only, or baseline and intervention scenarios combination as shown above. The results message is a polymorphic type carrying a customisable data payload, table below shows the default implementation outputs.

| Property     | Overall | Male  | Female | Description             |
| :---         | :---:   | :---: | :---:  |:---                     |
| Id           | √       | -     | -      | The message type identifier (results type)
| Source       | √       | -     | -      | The results experiment identification |
| Run number   | √       | -     | -      | The results rum number identification |
| Model time   | √       | -     | -      | The results model time identification |
| Average Age  | -       | √     | √      | Average age of the population alive |
| Prevalence   | -       | √     | √      | Prevalence for each disease in the population |
| Risk factors | -       | √     | √      | Average risk factor values in the population |
| Indicators (DALYs) | √ | - | - | YLL, YLD and DALY values per 10000 people |
| Population Counts | √ | - | - |  Total size, number alive, dead and emigrants |
| Metrics | √ | - | - | Custom key/value metrics for algorithms |

These measures are calculated and published by the analysis module at the end of each simulation time step, the combination of *source*, *run number* and *model time* is unique.

The following code snippet illustrates the dynamic registration of module builder functions with the simulation module factory by using the default module factory function used above. A similar mechanism can be used to register dummy or mock module versions, with deterministic behaviour for testing purpose.

```cpp
SimulationModuleFactory get_default_simulation_module_factory(Repository& manager)
{
    auto factory = SimulationModuleFactory(manager);
    factory.register_builder(SimulationModuleType::SES,
        [](Repository& repository, const ModelInput& config) ->
        SimulationModuleFactory::ModuleType {
            return build_ses_noise_module(repository, config); });

    factory.register_builder(SimulationModuleType::Demographic,
        [](Repository& repository, const ModelInput& config) -> 
        SimulationModuleFactory::ModuleType {
            return build_population_module(repository, config); });

    factory.register_builder(SimulationModuleType::RiskFactor,
        [](Repository& repository, const ModelInput& config) ->
        SimulationModuleFactory::ModuleType {
            return build_risk_factor_module(repository, config); });

    factory.register_builder(SimulationModuleType::Disease,
        [](Repository& repository, const ModelInput& config) -> 
        SimulationModuleFactory::ModuleType {
            return build_disease_module(repository, config); });

    factory.register_builder(SimulationModuleType::Analysis,
        [](Repository& repository, const ModelInput& config) -> 
        SimulationModuleFactory::ModuleType {
            return build_analysis_module(repository, config); });

    return factory;
}
```

The factory must provide builder functions for all module types, however, the user can disable a particular module behaviour by registering an implementation that makes no change to virtual population properties when invoked by the simulation engine.