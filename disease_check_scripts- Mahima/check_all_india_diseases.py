#!/usr/bin/env python3
"""Check ALL India (D356.csv) disease files for missing ages in range 0-100"""

import zipfile
import csv
import io
from pathlib import Path
from collections import defaultdict

zip_path = Path("c:/healthgps-data/pif-data-v7.zip")

issues = defaultdict(list)
all_ages_ok = []

print("Checking ALL India (D356.csv) disease files for missing ages 0-100...\n")
print("=" * 80)

with zipfile.ZipFile(zip_path, 'r') as z:
    # Find all D356.csv files
    disease_files = [f for f in z.namelist() if f.endswith('D356.csv') and 'diseases/' in f]

    for zip_file_path in sorted(disease_files):
        parts = zip_file_path.split('/')
        if len(parts) >= 3 and parts[0] == 'diseases':
            disease_name = parts[1]

            # Read and check ages
            content = z.read(zip_file_path).decode('utf-8')
            reader = csv.DictReader(io.StringIO(content))

            ages = set()
            for row in reader:
                try:
                    age = int(row['age'])
                    ages.add(age)
                except (ValueError, KeyError):
                    pass

            if not ages:
                issues['empty_files'].append(disease_name)
                print(f"[ERROR] {disease_name}/D356.csv - No age data found")
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
                issues['missing_ages'].append((disease_name, missing_ages, min_age, max_age))
                print(f"[WARN] {disease_name}/D356.csv")
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
                all_ages_ok.append(disease_name)
                status = "[OK]"
                if not has_age_0:
                    status = "[WARN]"
                    issues['missing_age_0'].append(disease_name)
                if has_age_101_plus:
                    status = "[OK*]"
                print(f"{status} {disease_name}/D356.csv (age range: {min_age}-{max_age}, {len(ages)} ages)")

print("\n" + "=" * 80)
print("\nSUMMARY:")
print(f"  Total D356.csv files checked: {len(disease_files)}")
print(f"  [OK] Complete age coverage (0-100): {len(all_ages_ok)}")
print(f"  [WARN] Missing some ages: {len(issues['missing_ages'])}")
print(f"  [WARN] Missing age 0 only: {len(issues['missing_age_0'])}")
print(f"  [ERROR] Empty files: {len(issues['empty_files'])}")

if issues['missing_ages']:
    print(f"\n[WARN] DISEASES WITH MISSING AGES ({len(issues['missing_ages'])}):")
    for disease, missing, min_a, max_a in sorted(issues['missing_ages']):
        print(f"   - {disease}: missing {len(missing)} ages (range: {min_a}-{max_a})")
        if 0 in missing:
            print(f"     [CRITICAL] Missing age 0!")
        if 100 in missing:
            print(f"     [CRITICAL] Missing age 100!")

if issues['missing_age_0']:
    print(f"\n[WARN] DISEASES MISSING ONLY AGE 0 ({len(issues['missing_age_0'])}):")
    for disease in sorted(issues['missing_age_0']):
        print(f"   - {disease}")

if issues['empty_files']:
    print(f"\n[ERROR] EMPTY FILES ({len(issues['empty_files'])}):")
    for disease in sorted(issues['empty_files']):
        print(f"   - {disease}")

# Check which of the user's simulated diseases have issues
simulated_diseases = {
    "ischemicheartdisease", "diabetes", "asthma", "pulmonary",
    "trachealbronchuslungcancer", "otherpharynxcancer", "lowerrespiratoryinfections",
    "tuberculosis", "stroke", "cervicalcancer", "larynxcancer",
    "alcoholusedisorders", "cirrhosis", "lipandoralcavitycancer", "livercancer",
    "roadinjuries", "selfharm", "intracerebralhemorrhage", "subarachnoidhemorrhage",
    "ischemicstroke"
}

affected_simulated = []
for disease, missing, _, _ in issues['missing_ages']:
    if disease in simulated_diseases:
        affected_simulated.append((disease, missing))

if affected_simulated:
    print(f"\n[CRITICAL] YOUR SIMULATED DISEASES WITH ISSUES ({len(affected_simulated)}):")
    for disease, missing in sorted(affected_simulated):
        print(f"   - {disease}: missing {len(missing)} ages")
        if 0 in missing:
            print(f"     [CRITICAL] Missing age 0!")
        if 100 in missing:
            print(f"     [CRITICAL] Missing age 100!")
else:
    print(f"\n[OK] None of your simulated diseases have missing age issues!")
