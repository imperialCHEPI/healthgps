## Welcome to the Global Health Policy Simulation model (Health-GPS).

| [Quick Start](#quick-start) | [Overview](#overview) | [User Guide](#user-guide) | [Developer Guide](#developer-guide) | [License](#license) | [Support](#support) |

*Health-GPS* microsimulation is part of the [STOP project](https://www.stopchildobesity.eu/), and support researchers and policy makers in the analysis of the health and economic impacts of alternative measures to tackle chronic diseases and obesity in children. The model reproduces the characteristics of a population and simulates key individual event histories associated with key components of relevant behaviours, such as physical activity, energy balance and diseases such as diabetes or cancers.

Childhood obesity is one of the major public health challenges throughout the world and is rising at an alarming rate in most countries. In particular, the rates of increase in obesity prevalence in developing countries have been more than 30% higher than those in developed countries. Simulation models are especially useful for assessing the long-term impacts of the wide range of policies that will be needed to tackle the obesity epidemic.

<a name="quick-start"></a>
### Quick Start
The **Health GPS** application provides a command line interface (CLI) and runs on *Windows 10 (and newer)* devices. You may need to install the latest [Visual C++ Redistributable](https://docs.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-160) on the machine, the application requires the `2019 x64` version to be installed.

1. Download the latest [release](https://github.com/imperialCHEPI/healthgps/releases) binaries from the repository.
2. Unzip the file contents into a local directory of your choice (xxx).
3. Open a command terminal, e.g. [Windows Terminal](https://www.microsoft.com/en-gb/p/windows-terminal/9n0dx20hk701?rtc=1&activetab=pivot:overviewtab), and navigate to the directory used in step 2 (xxx).
4. Run `X:\xxx> .\HealthGPS.Console.exe -f ".\example\demo.json" -s ".\data"` whehe `-f` gives the *configuration file* fullname and `-s` the path to the root folder of the *backend storage* respectivelly.
5. The default output folder is `C:\HealthGPS\Result`, but this can be changed in the *configuration file* `(demo.json)`.

For example, if you unzip the files into local directory `C:\HealthGPS\Release` during step 2, run:
```cmd
C:\HealthGPS\Release> .\HealthGPS.Console.exe -f ".\example\demo.json" -s ".\data"
```
The following is the expected *Health GPS CLI* start screen (top lines only). 
![Health GPS CLI](/assets/image/cli_startscreen.png)

All supported running options are provided to the model via a *configuration file* (JSON format), including intervention scenarios and multiple runs. Users are encouraged to start exploring the model by changing this configuration file and running the model again.

**NOTE:** *The development datasets provided in this example are limited to 2010-2030 time frame. It is provided for demonstration purpuse to showcase the model's usage, input and output data formats. The backend data storage can be populated with new datasets, the `index.json` file defines the storage structure and file names, it also stores metadata to identify the data sources and respective limits for validation.*

***Known Issue:*** Windows 10 support for VT (Virtual Terminal) / ANSI escape sequences is turned OFF by default, this is required to display colours on console / shell terminals. You can enable this feature manually by editing windows [registry keys](https://superuser.com/questions/413073/windows-console-with-ansi-colors-handling/1300251#1300251), however we recommend the use of [Windows Terminal](https://www.microsoft.com/en-gb/p/windows-terminal/9n0dx20hk701?rtc=1&activetab=pivot:overviewtab), which is a modern terminal application for command-line tools, has no such limitation, and is now distributed as part of the Windows 11 installation.

<a name="overview"></a>
### Overview

*Health GPS microsimulation* is being developed in collaboration between the [Centre for Health Economics & Policy Innovation (CHEPI)](https://www.imperial.ac.uk/business-school/faculty-research/research-centres/centre-health-economics-policy-innovation/), Imperial College London; and [INRAE](https://www.inrae.fr), France; as part of the [STOP project](https://www.stopchildobesity.eu/). The software architecture uses a modular design approach to provide the building blocks necessary to create the `Health GPS application`, which is implemented using object-oriented principles in `Modern C++` programming language targeting the [C++20 standard](https://en.cppreference.com/w/cpp/20). The software application contains four main components:

|![Health GPS Components](/assets/image/component_diagram.png)|
|:--:|
|*Health GPS Component Diagram*|


- *Health GPS Model* defines the microsimulation executive, core modules and algorithms used to create the virtual population, simulate over time and produce results.
- *Host application* responsible for parsing the configuration file, initialising supporting infrastructure, compose the HealthGPS instance by assembling the required modules, running the configured experiment, and collect results. This compoent is provided as a *console terminal* application, but can equality be a *graphical user interface (GUI)*.
- *Backend Data Storage* provides an interface (`Data API`) and persistence-agnostic data types (plain old class object - POCO) are required by the modules definition. This component provides a contract for all concrete implementations that actually store and retrieve the data.
- *File-base Data Storage* implements the `Data API` interface to provide a file based storage for the modules data. This component can be replaced by a different storage type, e.g. database, without being noticed by the *Health GPS Model* component.

These componets along with the phisical data storage are the minimum *package* to deploy and use the Health GPS microsimulation. The *Health GPS Model* defines interfaces for modules and external communication, as shown below, to provide decoupling and flexibility during composition.

|![Health GPS Modules](/assets/image/modules_diagram.png)|
|:--:|
|*Health GPS Modules Diagram*|

A *modules factory* manages the registration and creation of the module instances required by the *Health GPS model*. A concrete implementation of the *backend data storage* interface is injected into the module factory to supply the required data during module creation. The simulation executive is reposible for managing the simulation clock and events scheduling, this functionality is provided by the [ADEVS](https://web.ornl.gov/~nutarojj/adevs) library. The simulation results are streamed asynchronous to the outside world via a concrete implementation of the *message bus* interface provided to the model by the *host application* during initialisation.

<a name="user-guide"></a>
### User Guide
The *Health-GPS* microsimulation is a data driven modelling framework, combining many disconnected data sources to support the various interacting modules during a typical simulation experiment run. The framework provides a pre-populated backend data storage to minimise the learning curve for simple use cases, however advance users are likely to need a more in-depth knowlege of the full workflows. A high-level representation of the `Health-GPS workflow` is shown below, it is crucial that users have a good appreciation for the general dataflow and processes to better design experiments and quantify the results.

|![Health GPS Workflow](/assets/image/workflow_diagram.png)|
|:--:|
|*Health GPS Workflow Diagram*|

As with any simulation model, it is the user's responsability to process and analyse input data, define models hierarchy and fit parameters to data. A configuration file (JSON) is used control the simulation running settings and map the *Health-GPS* expected parameters to the user input data and fitted values. Likewise, it is the user's responsability to and analyse and quantify the model results, which are saved to a chosen output folder in `JSON` format.

|![Health GPS Execution](/assets/image/execution_diagram.png)|
|:--:|
|*Health GPS Execution Diagram*|

The current model output format, JSON (JavaScript Object Notation), is an open standard file format designed for data interchange in human-readable text. It is language-independent, however all programming language and major data science tools supports JSON format because they have libraries and functions to read/write JSON structures. To read the model results in R, for example, you need the [`jsonlite`](https://cran.r-project.org/web/packages/jsonlite/vignettes/json-aaquickstart.html) package:
```R
require(jsonlite)
data <- fromJSON(result_filename)
View(data)
```
![Health GPS Results](/assets/image/model_results.png)

---
> **_UNDER DEVELOPMENT:_**  More content coming soon.
---


<a name="developer-guide"></a>
### Developer Guide
The *Health GPS* software is written in modern, standard ANSI C++, targeting the [C++20 version](https://en.cppreference.com/w/cpp/20) and using the C++ Standard Library. The project is fully managed by [Microsft Visual Studio](https://visualstudio.microsoft.com) 2019, therefore *Windows OS* only, however the code base is portable and should be easily build on other platforms when [compilers](https://en.cppreference.com/w/cpp/compiler_support) supporting the C++20 standard become avaialble. The development toolset users [vcpkg](https://github.com/microsoft/vcpkg) package manager for dependencies, [googletest](https://github.com/google/googletest) for unit testing and [GitHub Actions](https://docs.github.com/en/actions) for automate build.

To start working on the *Health GPS* code base, the development machine needs:
1. Windows 10 or newer
2. [Git](https://git-scm.com/downloads)
3. [Microsft Visual Studio](https://visualstudio.microsoft.com) 2019 or newer.
4. The latest [vcpkg](https://github.com/microsoft/vcpkg) installed globally for Visual Studio projects.
5. Internet connection

Download the *Health GPS* source code to the local machine, we recommend somewhere like `C:\src` or `C:\source`, since otherwise you may run into path issues with the build systems.
```cmd
> git clone https://github.com/imperialCHEPI/healthgps
```
Finally, open the project solution in Visual Studio `...\healthgps\source\HelathGPS.sln` and hit build. The first build takes considerable longer than normal due to the initial work required by the package manager.

---
> **_UNDER DEVELOPMENT:_**  More content coming soon.
---
The current modelling of *intervention policies* requires data synchronisation between the *baseline* and *intervention* scenarios at multiple points, this requirement is a major constraint on the size of the virtual population and independent running of the model, resulting on pairs of simulations being evaluated at a time, see the *Execution Diagram* in previous section. The current solution is based on shared memory and works on a single machine, it creates a unidirectional channel to asynchronous send messages from the baseline scenario to the intervention scenario as shown below. At the receiving end, the channel is synchronous, blocking until the required message arrives or a pre-defined time expires, forcing the experiment to terminate.

|![Health GPS DataSync](/assets/image/sync_diagram.png)|
|:--:|
|*Health GPS Scenario Data Synchronization*|

An alternative to this design is to use a message broker, e.g., [RabbitMQ](https://www.rabbitmq.com), or a distributed event streaming platform such as [Apache Kafka](https://kafka.apache.org) to distribute the messages over a network of computers running in pairs to scale-up the model virtual population size and throughput.

<a name="license"></a>
### License
The code in this repository is licensed under the [BSD 3-Clause](https://github.com/imperialCHEPI/healthgps/blob/main/LICENSE.md) license.

<a name="support"></a>
### Contact and Support
Imperial College London Business School, [Centre for Health Economics & Policy Innovation (CHEPI)](https://www.imperial.ac.uk/business-school/faculty-research/research-centres/centre-health-economics-policy-innovation/).
