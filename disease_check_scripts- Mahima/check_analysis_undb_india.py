#!/usr/bin/env python3
"""Check analysis and undb India files for age data"""

import zipfile
import csv
import io
from pathlib import Path

zip_path = Path("c:/healthgps-data/pif-data-v7.zip")

files_to_check = [
    'analysis/cost/BoD356.csv',
    'undb/indicators/Pi356.csv',
    'undb/mortality/M356.csv',
    'undb/population/P356.csv'
]

print("Checking India files in analysis/undb folders for age data...\n")
print("=" * 80)

with zipfile.ZipFile(zip_path, 'r') as z:
    for file_path in files_to_check:
        if file_path not in z.namelist():
            print(f"[NOT FOUND] {file_path}")
            continue

        print(f"\n=== {file_path} ===")
        try:
            content = z.read(file_path).decode('utf-8')
            reader = csv.DictReader(io.StringIO(content))

            print(f"Columns: {reader.fieldnames}")

            if 'age' not in reader.fieldnames:
                print("[INFO] No age column - this file doesn't use age data")
                # Show first few rows
                print("\nFirst 3 rows:")
                for i, row in enumerate(reader):
                    if i >= 3:
                        break
                    print(f"  {dict(list(row.items())[:5])}")  # First 5 columns
                continue

            # Check ages
            ages = set()
            row_count = 0
            for row in reader:
                try:
                    age = int(row['age'])
                    ages.add(age)
                    row_count += 1
                except (ValueError, KeyError):
                    pass

            if not ages:
                print("[ERROR] No age data found")
                continue

            min_age = min(ages)
            max_age = max(ages)

            # Check for missing ages in range 0-100
            missing_ages = []
            for age in range(0, 101):
                if age not in ages:
                    missing_ages.append(age)

            print(f"Total rows: {row_count}")
            print(f"Age range: {min_age}-{max_age} ({len(ages)} unique ages)")

            if missing_ages:
                print(f"[WARN] Missing {len(missing_ages)} ages in range 0-100")
                print(f"Missing ages: {missing_ages[:20]}", end="")
                if len(missing_ages) > 20:
                    print(f" ... and {len(missing_ages) - 20} more")
                else:
                    print()
                if 0 in missing_ages:
                    print("[CRITICAL] Missing age 0!")
                if 100 in missing_ages:
                    print("[CRITICAL] Missing age 100!")
            else:
                print("[OK] Complete age coverage (0-100)")

        except Exception as e:
            print(f"[ERROR] Failed to read: {e}")
