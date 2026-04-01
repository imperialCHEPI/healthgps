#!/usr/bin/env python3
"""Check undb files for age coverage (they use 'Age' not 'age')"""

import zipfile
import csv
import io
from pathlib import Path

zip_path = Path("c:/healthgps-data/pif-data-v7.zip")

files_to_check = [
    'undb/mortality/M356.csv',
    'undb/population/P356.csv'
]

print("Checking undb India files for age coverage (using 'Age' column)...\n")
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

            # Check for 'Age' or 'age' column
            age_col = None
            if 'Age' in reader.fieldnames:
                age_col = 'Age'
            elif 'age' in reader.fieldnames:
                age_col = 'age'

            if not age_col:
                print("[ERROR] No age column found")
                continue

            # Check ages
            ages = set()
            row_count = 0
            for row in reader:
                try:
                    age = int(row[age_col])
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
