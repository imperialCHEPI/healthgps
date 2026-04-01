#!/usr/bin/env python3
"""Check ALL CSV files (356 total) across all folders in pif-data-v7.zip for missing ages 0-100"""

import zipfile
import csv
import io
from pathlib import Path
from collections import defaultdict

zip_path = Path("c:/healthgps-data/pif-data-v7.zip")

issues = defaultdict(list)
all_ages_ok = []
total_files = 0

print("Checking ALL CSV files across all folders for missing ages 0-100...\n")
print("=" * 80)

with zipfile.ZipFile(zip_path, 'r') as z:
    # Get all CSV files (not just diseases folder)
    all_csv_files = [f for f in z.namelist() if f.endswith('.csv')]
    total_files = len(all_csv_files)

    print(f"Found {total_files} CSV files total\n")

    for zip_file_path in sorted(all_csv_files):
        parts = zip_file_path.split('/')
        folder_name = parts[0] if len(parts) > 1 else "root"
        file_name = parts[-1]

        # Read and check ages
        try:
            content = z.read(zip_file_path).decode('utf-8')
            reader = csv.DictReader(io.StringIO(content))

            # Check if 'age' column exists
            if 'age' not in reader.fieldnames:
                issues['no_age_column'].append(zip_file_path)
                continue

            ages = set()
            for row in reader:
                try:
                    age = int(row['age'])
                    ages.add(age)
                except (ValueError, KeyError):
                    pass

            if not ages:
                issues['empty_files'].append(zip_file_path)
                continue

            min_age = min(ages)
            max_age = max(ages)

            # Check for missing ages in range 0-100
            missing_ages = []
            for age in range(0, 101):  # 0 to 100 inclusive
                if age not in ages:
                    missing_ages.append(age)

            # Check specific critical ages
            has_age_0 = 0 in ages
            has_age_100 = 100 in ages
            has_age_101_plus = any(a > 100 for a in ages)

            if missing_ages:
                issues['missing_ages'].append((zip_file_path, missing_ages, min_age, max_age))
                print(f"[WARN] {zip_file_path}")
                print(f"        Age range: {min_age}-{max_age} ({len(ages)} ages)")
                print(f"        Missing {len(missing_ages)} ages: {missing_ages[:20]}", end="")
                if len(missing_ages) > 20:
                    print(f" ... and {len(missing_ages) - 20} more")
                else:
                    print()

                if not has_age_0:
                    print(f"        [CRITICAL] Missing age 0!")
                if not has_age_100:
                    print(f"        [CRITICAL] Missing age 100!")
                if has_age_101_plus:
                    print(f"        [INFO] Has ages > 100 (up to {max_age})")
            else:
                all_ages_ok.append(zip_file_path)
                status = "[OK]"
                if not has_age_0:
                    status = "[WARN]"
                    issues['missing_age_0'].append(zip_file_path)
                if has_age_101_plus:
                    status = "[OK*]"
                # Only print OK files if they're in diseases folder or have issues
                if folder_name == "diseases" or not has_age_0:
                    print(f"{status} {zip_file_path} (age range: {min_age}-{max_age}, {len(ages)} ages)")
        except Exception as e:
            issues['read_errors'].append((zip_file_path, str(e)))
            print(f"[ERROR] {zip_file_path} - Failed to read: {e}")

print("\n" + "=" * 80)
print("\nSUMMARY:")
print(f"  Total CSV files checked: {total_files}")
print(f"  [OK] Complete age coverage (0-100): {len(all_ages_ok)}")
print(f"  [WARN] Missing some ages: {len(issues['missing_ages'])}")
print(f"  [WARN] Missing age 0 only: {len(issues['missing_age_0'])}")
print(f"  [ERROR] Empty files: {len(issues['empty_files'])}")
print(f"  [ERROR] No age column: {len(issues['no_age_column'])}")
print(f"  [ERROR] Read errors: {len(issues['read_errors'])}")

if issues['missing_ages']:
    print(f"\n[WARN] FILES WITH MISSING AGES ({len(issues['missing_ages'])}):")
    for file_path, missing, min_a, max_a in sorted(issues['missing_ages']):
        print(f"   - {file_path}: missing {len(missing)} ages (range: {min_a}-{max_a})")
        if 0 in missing:
            print(f"     [CRITICAL] Missing age 0!")
        if 100 in missing:
            print(f"     [CRITICAL] Missing age 100!")

if issues['missing_age_0']:
    print(f"\n[WARN] FILES MISSING ONLY AGE 0 ({len(issues['missing_age_0'])}):")
    for file_path in sorted(issues['missing_age_0']):
        print(f"   - {file_path}")

if issues['empty_files']:
    print(f"\n[ERROR] EMPTY FILES ({len(issues['empty_files'])}):")
    for file_path in sorted(issues['empty_files']):
        print(f"   - {file_path}")

if issues['no_age_column']:
    print(f"\n[INFO] FILES WITHOUT AGE COLUMN ({len(issues['no_age_column'])}):")
    for file_path in sorted(issues['no_age_column']):
        print(f"   - {file_path}")

if issues['read_errors']:
    print(f"\n[ERROR] FILES WITH READ ERRORS ({len(issues['read_errors'])}):")
    for file_path, error in sorted(issues['read_errors']):
        print(f"   - {file_path}: {error}")

# Group by folder
folder_stats = defaultdict(lambda: {'total': 0, 'ok': 0, 'missing_ages': 0, 'missing_age_0': 0})
for file_path in all_ages_ok:
    folder = file_path.split('/')[0] if '/' in file_path else "root"
    folder_stats[folder]['total'] += 1
    folder_stats[folder]['ok'] += 1

for file_path, _, _, _ in issues['missing_ages']:
    folder = file_path.split('/')[0] if '/' in file_path else "root"
    folder_stats[folder]['total'] += 1
    folder_stats[folder]['missing_ages'] += 1

for file_path in issues['missing_age_0']:
    folder = file_path.split('/')[0] if '/' in file_path else "root"
    folder_stats[folder]['total'] += 1
    folder_stats[folder]['missing_age_0'] += 1

print(f"\n\nFOLDER BREAKDOWN:")
for folder in sorted(folder_stats.keys()):
    stats = folder_stats[folder]
    print(f"  {folder}: {stats['total']} files - {stats['ok']} OK, {stats['missing_ages']} missing ages, {stats['missing_age_0']} missing age 0")
