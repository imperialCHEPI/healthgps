## Welcome to the Global Health Policy Simulation model (Health GPS).

| Home |[Quick Start](getstarted) | [Software Architecture](architecture) | [Data Model](datamodel) | [Development](development) | [User Guide](userguide) | [License](index#license) |

# Introduction
**Health GPS** is a modular and flexible microsimulation framework developed in collaboration between the Centre for Health Economics & Policy Innovation ([CHEPI](https://www.imperial.ac.uk/business-school/faculty-research/research-centres/centre-health-economics-policy-innovation/)), Imperial College London; and [INRAE](https://www.inrae.fr), France; as part of the [STOP project](https://www.stopchildobesity.eu/). *Health GPS* allow researchers to test the effectiveness of a variety of health policies and interventions designed for tackling childhood obesity in European Countries.

Childhood obesity is one of the major public health challenges throughout the world and is rising at an alarming rate in most countries. In particular, the rates of increase in obesity prevalence in developing countries have been more than 30% higher than those in developed countries. Simulation models are especially useful for assessing the long-term impacts of the wide range of policies that will be needed to tackle the obesity epidemic.

*Health GPS* simulates close to reality life histories from birth to death of each member of a population including key characteristics such as gender, age, socio-economic status, risk factors, and disease profiles. These characteristics evolve over time and are updated annually using statistical and probabilistic models which are calibrated to reproduce key demographic and epidemiological statistics from European countries.

The model uses proprietary equations to account for a variety of complex interactions such as risk factor-disease interactions and disease-disease interactions. Modellers are then able to evaluate health-related policies by changing some of the parameters and comparing the outputs with a baseline simulation. The model produces detailed quantitative outputs covering demographics, risk factors, diseases, mortality, and health care expenditure which could then be used to complement qualitative policy evaluation tools.

## Energy Balanace Model (EBM)

National dietary surveys from various European countries are analysed with a view to build a comprehensive model of childhood obesity. The model estimates yearly changes in physical activity, diet, energy balance, and Body Mass Index (BMI) using statistical models calibrated using dietary and anthropometric surveys. These dynamics include measures of physical activity expressed in metabolic equivalents (METs) and macronutrients intakes measures including grams of fat, carbohydrates, protein, fibre, salt, and sugar. The general concept for an EBM is shown below (top diagram), and a possible Health GPS translation provided for illustration purpose.

|![Health GPS Components](/assets/image/energy_balance_model.png)|
|:--:|
|*Energy balance model structure example*|

The calibration of the equations is carried out outside of the model by gender for children and adults separately to ensure capturing gender- and age-related differences. Emphasis is placed on capturing the rapid growth and changes in children BMIs. International anthropometric references are used to properly classify individuals as normal weight, overweight or obese. This classification is used throughout the simulation to identify children with growth problems and devise appropriate policies and interventions to tackle the health issues.

## Policy Evaluation
The overall approach adopted to evaluate the impacts and cost-effectiveness of intervention policies to reduce childhood obesity using the policy simulation tool is based upon *“what-if”* analyses to quantify the causal relations between variables, in simple terms, scenarios can be classified as:
* *Baseline scenarios*: elaborated to define the trends in observed childhood obesity we measure outcomes against (e.g., population, calories intake, diseases prevalence trends).

* *Intervention scenarios*: policies designed to change the observed trends in childhood obesity during a specific timeframe (e.g., food labelling, healthy eating promotion, BMI reduction).

The choice of baseline scenario is critical for analyses as it serves as a reference for comparison and can influence outcomes. Having defined the baseline scenario, the simulation assesses the impacts of different intervention policies by projecting populations, risk factors, diseases, and life trajectories into the future comparing pairs of no-intervention and intervention scenarios.

The first run evaluates the no-intervention, *“baseline scenario”* where demographics, risk factors, and diseases are projected based solely on estimates from historical data. The second run evaluates the *“intervention scenario”* where a specific policy is applied to the same population with the aim of modifying the underlying trends and risk factor distribution. 

Finally, detailed analysis can be carried out, externally, to compare the two simulated scenarios results in terms of population demographics and burden of diseases to estimate the cost-effectiveness and impacts of the targeted intervention in tackling childhood obesity.

<a name="license"></a>
# License
The code in this repository is licensed under the [BSD 3-Clause](https://github.com/imperialCHEPI/healthgps/blob/main/LICENSE.md) license.

## Contact and Support
Imperial College London Business School, Centre for Health Economics & Policy Innovation ([CHEPI](https://www.imperial.ac.uk/business-school/faculty-research/research-centres/centre-health-economics-policy-innovation/)).