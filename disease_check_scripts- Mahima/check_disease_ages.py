#!/usr/bin/env python3
"""
Simple script to check if disease data files have complete age coverage.
Checks for missing ages in the expected range [0, 100] or [1, 100] depending on disease type.
"""

import csv
import sys
import glob
import zipfile
import io
from pathlib import Path
from collections import defaultdict

def check_disease_file(filepath_or_content, expected_min_age=0, expected_max_age=100, is_zip_content=False):
    """Check a single disease CSV file for age coverage."""
    ages_found = set()
    genders_found = set()
    measures_found = set()

    try:
        if is_zip_content:
            # filepath_or_content is actually the file content (bytes or string)
            if isinstance(filepath_or_content, bytes):
                f = io.StringIO(filepath_or_content.decode('utf-8'))
            else:
                f = io.StringIO(filepath_or_content)
        else:
            f = open(filepath_or_content, 'r', encoding='utf-8')

        reader = csv.DictReader(f)

        # Check required columns
        required_cols = ['age', 'gender_id', 'measure_id', 'measure', 'mean']
        if not all(col in reader.fieldnames for col in required_cols):
            return {
                'error': f"Missing required columns. Found: {list(reader.fieldnames)}"
            }

        for row in reader:
            try:
                age = int(row['age'])
                gender_id = int(row['gender_id'])
                measure_id = int(row['measure_id'])

                ages_found.add(age)
                genders_found.add(gender_id)
                measures_found.add(measure_id)
            except (ValueError, KeyError) as e:
                return {'error': f"Invalid row data: {e}"}

        if not is_zip_content:
            f.close()

        # Check age coverage
        min_age = min(ages_found) if ages_found else None
        max_age = max(ages_found) if ages_found else None

        missing_ages = []
        for age in range(expected_min_age, expected_max_age + 1):
            if age not in ages_found:
                missing_ages.append(age)

        return {
            'file': str(filepath_or_content) if not is_zip_content else filepath_or_content,
            'min_age': min_age,
            'max_age': max_age,
            'age_count': len(ages_found),
            'expected_range': f"{expected_min_age}-{expected_max_age}",
            'missing_ages': missing_ages,
            'missing_count': len(missing_ages),
            'genders': sorted(genders_found),
            'measures': sorted(measures_found),
            'has_age_0': 0 in ages_found,
            'has_age_100': 100 in ages_found,
            'has_age_101': 101 in ages_found,
        }
    except Exception as e:
        return {'error': str(e)}

def main():
    if len(sys.argv) < 2:
        print("Usage: python check_disease_ages.py <disease_data_directory_or_zip>")
        print("\nExample:")
        print("  python check_disease_ages.py input-data/disease-data/data/diseases/")
        print("  python check_disease_ages.py c:/healthgps-data/pif-data-v7.zip")
        print("  python check_disease_ages.py /path/to/diseases/*/D356.csv")
        sys.exit(1)

    # Get file pattern
    pattern = sys.argv[1]

    # Check if it's a zip file
    if pattern.endswith('.zip'):
        zip_path = Path(pattern)
        if not zip_path.exists():
            print(f"[ERROR] Zip file not found: {zip_path}")
            sys.exit(1)

        print(f"Reading from zip file: {zip_path}\n")
        files = []
        with zipfile.ZipFile(zip_path, 'r') as z:
            # Find all D*.csv files in diseases/ folders
            disease_files = [f for f in z.namelist() if '/D' in f and f.endswith('.csv') and 'diseases/' in f]
            for zip_file_path in disease_files:
                # Extract just the disease name from path like "diseases/pulmonary/D356.csv"
                parts = zip_file_path.split('/')
                if len(parts) >= 3 and parts[0] == 'diseases':
                    disease_name = parts[1]
                    files.append((zip_file_path, disease_name, z.read(zip_file_path)))

        if not files:
            print(f"[ERROR] No disease files (D*.csv) found in zip file")
            sys.exit(1)
    else:
        files = glob.glob(pattern) if '*' in pattern else [pattern]

        # Also check if it's a directory
        if len(files) == 1 and Path(files[0]).is_dir():
            # Find all D*.csv files (disease files)
            disease_dir = Path(files[0])
            files = list(disease_dir.rglob("D*.csv"))
            if not files:
                print(f"No disease files (D*.csv) found in {disease_dir}")
                sys.exit(1)
        # Convert to same format as zip files
        files = [(str(f), Path(f).name, None) for f in files]

    print(f"Checking {len(files)} disease file(s)...\n")
    print("=" * 80)

    issues_found = []
    for file_info in sorted(files):
        if len(file_info) == 3:
            # Zip file format: (zip_path, disease_name, content)
            zip_path, disease_name, content = file_info
            result = check_disease_file(content, is_zip_content=True)
            display_name = f"{disease_name}/{Path(zip_path).name}"
        else:
            # Regular file
            filepath = file_info
            result = check_disease_file(filepath)
            display_name = Path(filepath).name

        if 'error' in result:
            print(f"[ERROR] {display_name}: {result['error']}")
            issues_found.append((display_name, result['error']))
            continue

        # Print summary
        status = "[OK]" if result['missing_count'] == 0 else "[WARN]"
        print(f"{status} {display_name}")
        print(f"   Age range: {result['min_age']}-{result['max_age']} ({result['age_count']} ages)")
        print(f"   Expected: {result['expected_range']}")

        if result['missing_count'] > 0:
            print(f"   [WARN] MISSING {result['missing_count']} ages: {result['missing_ages'][:10]}", end="")
            if len(result['missing_ages']) > 10:
                print(f" ... and {len(result['missing_ages']) - 10} more")
            else:
                print()
            issues_found.append((display_name, f"Missing {result['missing_count']} ages"))
        else:
            print(f"   [OK] Complete age coverage")

        # Check for age 0 and age 100/101
        if not result['has_age_0']:
            print(f"   [WARN] Missing age 0 (simulation starts at age 0)")
            issues_found.append((display_name, "Missing age 0"))

        if not result['has_age_100']:
            print(f"   [WARN] Missing age 100 (simulation goes to age 100)")
            issues_found.append((display_name, "Missing age 100"))

        if result['has_age_101']:
            print(f"   [WARN] Has age 101 (simulation only goes to 100, may cause issues)")

        print()

    # Summary
    print("=" * 80)
    if issues_found:
        print(f"\n[WARN] Found issues in {len(set(f[0] for f in issues_found))} file(s):")
        for filepath, issue in issues_found:
            print(f"   - {filepath}: {issue}")
        print("\n[INFO] These missing ages could cause the 'vector::_M_range_check' error!")
        return 1
    else:
        print("\n[OK] All disease files have complete age coverage!")
        return 0

if __name__ == "__main__":
    sys.exit(main())
