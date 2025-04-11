import pandas as pd
import numpy as np

# Load the CSV file into a DataFrame
file_path = "carbohydrate_over_years.csv"
df = pd.read_csv(file_path)

# Ensure Year is an integer
df["Year"] = df["Year"].astype(int)

# Sort the data by PersonID and Year
df = df.sort_values(by=["PersonID", "Year"])

# Define a very small tolerance for floating-point comparisons (more sensitive)
TOLERANCE = 0.0001  # Decreased from previous value

# Initialize a list to store results
mismatches = []

# Group by PersonID
for person_id, person_df in df.groupby("PersonID"):
    person_df = person_df.sort_values("Year")

    # Get all years for this person
    years = sorted(person_df["Year"].unique())

    # Get the person's age (using latest year)
    age = person_df.iloc[-1]["Age"]

    # Initialize data dictionary for this person
    person_data = {
        "PersonID": person_id,
        "Age": age
    }

    # Add data for all years (whether mismatched or not)
    for year in years:
        year_data = person_df[person_df["Year"] == year]
        person_data[f"CI_{year}"] = year_data["CI"].values[0]
        person_data[f"CI_0_{year}"] = year_data["CI_0"].values[0]

    # Check for mismatches between consecutive years
    mismatch_found = False
    for i in range(len(years) - 1):
        current_year = years[i]
        next_year = years[i + 1]

        # Only check consecutive years
        if next_year == current_year + 1:
            current_ci = person_df[person_df["Year"] == current_year]["CI"].values[0]
            next_ci_0 = person_df[person_df["Year"] == next_year]["CI_0"].values[0]

            # Compare with a small tolerance
            if abs(current_ci - next_ci_0) > TOLERANCE:
                mismatch_found = True
                person_data[f"Diff_{current_year}_{next_year}"] = current_ci - next_ci_0

    # Either include all records, or only those with mismatches
    SHOW_ALL_RECORDS = True  # Change to False to only show mismatches
    if mismatch_found or SHOW_ALL_RECORDS:
        mismatches.append(person_data)

# Create DataFrame from the data
result_df = pd.DataFrame(mismatches)

# Reorder columns to group by year
columns = ["PersonID", "Age"]
year_columns = [col for col in result_df.columns if col not in columns and not col.startswith("Diff_")]
year_columns = sorted(year_columns, key=lambda x: (int(x.split('_')[-1]), x.split('_')[0]))
diff_columns = [col for col in result_df.columns if col.startswith("Diff_")]
diff_columns = sorted(diff_columns)
columns.extend(year_columns)
columns.extend(diff_columns)

# Reorder the DataFrame columns
result_df = result_df[columns]

# Print results
if not result_df.empty:
    print(f"Found {len(result_df)} people with data")

    # Count actual mismatches
    mismatch_count = sum(1 for idx, row in result_df.iterrows()
                         if any(col.startswith("Diff_") for col in row.index if not pd.isna(row[col])))
    print(f"Of these, {mismatch_count} people have mismatches between consecutive years")

    print("\nSample of data (first 5 rows):")
    print(result_df.head().to_string())

    # Save to CSV
    output_file = "carb_analysis_complete.csv"
    result_df.to_csv(output_file, index=False)
    print(f"\nData saved to {output_file}")
else:
    print("No data found that matches criteria.")
