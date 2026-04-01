# Age Group Data Capture Guide

## Overview

This guide helps you capture data for **all ages** by running the simulation multiple times with different age groups. This prevents overwhelming the system while ensuring you get complete data coverage.

## Age Groups to Capture

| Group | Age Range | Expected Records | Notes |
|-------|-----------|------------------|-------|
| 1 | 0-20 | ~15,000-20,000 | Children & young adults |
| 2 | 21-30 | ~8,000-12,000 | Young adults |
| 3 | 31-40 | ~8,000-12,000 | Middle-aged adults |
| 4 | 41-50 | ~8,000-12,000 | Middle-aged adults |
| 5 | 51-60 | ~8,000-12,000 | Older adults |
| 6 | 61-70 | ~6,000-10,000 | Senior adults |
| 7 | 71-80 | ~4,000-8,000 | Elderly |
| 8 | 81-90 | ~2,000-4,000 | Very elderly |
| 9 | 91-110 | ~500-1,000 | Oldest adults |

## Step-by-Step Process

### Step 1: Prepare Directory Structure

```bash
mkdir age_group_data
mkdir age_group_data\group_1_0_20
mkdir age_group_data\group_2_21_30
mkdir age_group_data\group_3_31_40
mkdir age_group_data\group_4_41_50
mkdir age_group_data\group_5_51_60
mkdir age_group_data\group_6_61_70
mkdir age_group_data\group_7_71_80
mkdir age_group_data\group_8_81_90
mkdir age_group_data\group_9_91_110
```

### Step 2: For Each Age Group

#### 2.1 Edit simulation.cpp

Change lines 94-95 in `src/HealthGPS/simulation.cpp`:

**For Group 1 (Ages 0-20):**

```cpp
int min_age = 0;
int max_age = 20;
```

**For Group 2 (Ages 21-30):**

```cpp
int min_age = 21;
int max_age = 30;
```

**For Group 3 (Ages 31-40):**

```cpp
int min_age = 31;
int max_age = 40;
```

**And so on...**

#### 2.2 Build and Run

```bash
cmake --build out\build\windows-release --target HealthGPS --config Release
# Run your simulation
```

#### 2.3 Move CSV Files

After each run, move the generated CSV files:

```bash
move risk_factor_inspection\foodvegetable_inspection.csv age_group_data\group_1_0_20\
move risk_factor_inspection\foodfruit_inspection.csv age_group_data\group_1_0_20\
```

### Step 3: Combine Data (Optional)

After capturing all age groups, you can combine the CSV files:

```bash
# Combine all foodvegetable files
copy age_group_data\group_*\foodvegetable_inspection.csv combined_foodvegetable_all_ages.csv

# Combine all foodfruit files
copy age_group_data\group_*\foodfruit_inspection.csv combined_foodfruit_all_ages.csv
```

## Expected Output Files

For each age group, you'll get:

- `foodvegetable_inspection.csv` - Vegetable consumption data
- `foodfruit_inspection.csv` - Fruit consumption data

Each CSV contains these **economist-friendly columns**:

- `actual_energy_intake` - Energy intake (kcal)
- `expected_energy_intake` - Expected energy intake
- `final_value_after_second_clamp` - **Food consumption (g/day)**
- `age`, `gender`, `year`, `scenario`
- `region`, `ethnicity`, `income_continuous`
- All calculation details for debugging

## Tips

1. **Start with smaller age groups** (0-20, 21-30) to test the process
2. **Monitor memory usage** - if system gets slow, reduce age range further
3. **Keep CSV files organized** by age group for easy analysis
4. **Check file sizes** - each CSV should be manageable (under 100MB)

## Quick Reference

| Edit These Lines | Age Group | Expected Time |
|------------------|-----------|---------------|
| `min_age = 0; max_age = 20;` | 0-20 | 5-10 min |
| `min_age = 21; max_age = 30;` | 21-30 | 3-5 min |
| `min_age = 31; max_age = 40;` | 31-40 | 3-5 min |
| `min_age = 41; max_age = 50;` | 41-50 | 3-5 min |
| `min_age = 51; max_age = 60;` | 51-60 | 3-5 min |
| `min_age = 61; max_age = 70;` | 61-70 | 2-4 min |
| `min_age = 71; max_age = 80;` | 71-80 | 2-3 min |
| `min_age = 81; max_age = 90;` | 81-90 | 1-2 min |
| `min_age = 91; max_age = 110;` | 91-110 | 1-2 min |

## Total Expected Data

- **Complete age coverage**: 0-110 years
- **Both risk factors**: foodvegetable + foodfruit
- **Both years**: 2022 + 2023
- **Both genders**: male + female
- **Total records**: ~60,000-80,000 per risk factor
- **Total time**: ~30-45 minutes for all age groups
