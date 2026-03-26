#!/usr/bin/env python3
"""Check ALL D356.csv files across all folders for simulated diseases and missing ages"""

import zipfile
import csv
import io
from pathlib import Path
from collections import defaultdict

# User's simulated diseases
simulated_diseases = {
    "ischemicheartdisease", "diabetes", "asthma", "pulmonary",
    "trachealbronchuslungcancer", "otherpharynxcancer", "lowerrespiratoryinfections",
    "tuberculosis", "stroke", "cervicalcancer", "larynxcancer",
    "alcoholusedisorders", "cirrhosis", "lipandoralcavitycancer", "livercancer",
    "roadinjuries", "selfharm", "intracerebralhemorrhage", "subarachnoidhemorrhage",
    "ischemicstroke"
}

zip_path = Path("c:/healthgps-data/pif-data-v7.zip")

issues = defaultdict(list)
all_ok = []

print("Checking ALL D356.csv files across ALL folders for simulated diseases...\n")
print("=" * 80)

with zipfile.ZipFile(zip_path, 'r') as z:
    # Find ALL D356.csv files (not just in diseases folder)
    all_d356_files = [f for f in z.namelist() if f.endswith('D356.csv')]

    print(f"Found {len(all_d356_files)} D356.csv files total\n")

    for zip_file_path in sorted(all_d356_files):
        parts = zip_file_path.split('/')
        folder_name = parts[0] if len(parts) > 1 else "root"

        # Extract disease name from path
        disease_name = None
        if folder_name == "diseases" and len(parts) >= 3:
            disease_name = parts[1]
        elif folder_name == "analysis":
            # Check if it's related to a disease
            for disease in simulated_diseases:
                if disease in zip_file_path.lower():
                    disease_name = disease
                    break
        elif folder_name == "undb":
            # UNDB files might not be disease-specific
            disease_name = "undb_data"

        # Check if this file is relevant to simulated diseases
        is_simulated = False
        if disease_name and disease_name in simulated_diseases:
            is_simulated = True
        elif folder_name == "diseases":
            # Check if any simulated disease name appears in path
            for disease in simulated_diseases:
                if disease in zip_file_path.lower():
                    is_simulated = True
                    disease_name = disease
                    break

        # Read and check ages
        try:
            content = z.read(zip_file_path).decode('utf-8')
            reader = csv.DictReader(io.StringIO(content))

            # Check if 'age' column exists
            if 'age' not in reader.fieldnames:
                if is_simulated:
                    issues['no_age_column'].append((zip_file_path, disease_name))
                continue

            ages = set()
            for row in reader:
                try:
                    age = int(row['age'])
                    ages.add(age)
                except (ValueError, KeyError):
                    pass

            if not ages:
                if is_simulated:
                    issues['empty_files'].append((zip_file_path, disease_name))
                continue

            min_age = min(ages)
            max_age = max(ages)

            # Check for missing ages in range 0-100
            missing_ages = []
            for age in range(0, 101):  # 0 to 100 inclusive
                if age not in ages:
                    missing_ages.append(age)

            has_age_0 = 0 in ages
            has_age_100 = 100 in ages

            if is_simulated:
                if missing_ages:
                    issues['missing_ages'].append((zip_file_path, disease_name, missing_ages, min_age, max_age))
                    print(f"[WARN] {zip_file_path}")
                    print(f"        Disease: {disease_name}")
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
                else:
                    all_ok.append((zip_file_path, disease_name))
                    status = "[OK]"
                    if not has_age_0:
                        status = "[WARN]"
                        issues['missing_age_0'].append((zip_file_path, disease_name))
                    print(f"{status} {zip_file_path} (disease: {disease_name}, age range: {min_age}-{max_age})")
        except Exception as e:
            if is_simulated:
                issues['read_errors'].append((zip_file_path, disease_name, str(e)))
                print(f"[ERROR] {zip_file_path} - Failed to read: {e}")

print("\n" + "=" * 80)
print("\nSUMMARY FOR SIMULATED DISEASES:")
print(f"  Total D356.csv files checked: {len(all_d356_files)}")
print(f"  [OK] Complete age coverage (0-100): {len(all_ok)}")
print(f"  [WARN] Missing some ages: {len(issues['missing_ages'])}")
print(f"  [WARN] Missing age 0 only: {len(issues['missing_age_0'])}")
print(f"  [ERROR] Empty files: {len(issues['empty_files'])}")
print(f"  [ERROR] No age column: {len(issues['no_age_column'])}")
print(f"  [ERROR] Read errors: {len(issues['read_errors'])}")

if issues['missing_ages']:
    print(f"\n[CRITICAL] SIMULATED DISEASE FILES WITH MISSING AGES ({len(issues['missing_ages'])}):")
    for file_path, disease, missing, min_a, max_a in sorted(issues['missing_ages']):
        print(f"   - {file_path}")
        print(f"     Disease: {disease}, Missing {len(missing)} ages (range: {min_a}-{max_a})")
        if 0 in missing:
            print(f"     [CRITICAL] Missing age 0!")
        if 100 in missing:
            print(f"     [CRITICAL] Missing age 100!")

if issues['missing_age_0']:
    print(f"\n[WARN] SIMULATED DISEASE FILES MISSING ONLY AGE 0 ({len(issues['missing_age_0'])}):")
    for file_path, disease in sorted(issues['missing_age_0']):
        print(f"   - {file_path} (disease: {disease})")

if issues['empty_files']:
    print(f"\n[ERROR] EMPTY SIMULATED DISEASE FILES ({len(issues['empty_files'])}):")
    for file_path, disease in sorted(issues['empty_files']):
        print(f"   - {file_path} (disease: {disease})")

if issues['no_age_column']:
    print(f"\n[INFO] SIMULATED DISEASE FILES WITHOUT AGE COLUMN ({len(issues['no_age_column'])}):")
    for file_path, disease in sorted(issues['no_age_column']):
        print(f"   - {file_path} (disease: {disease})")

if issues['read_errors']:
    print(f"\n[ERROR] SIMULATED DISEASE FILES WITH READ ERRORS ({len(issues['read_errors'])}):")
    for file_path, disease, error in sorted(issues['read_errors']):
        print(f"   - {file_path} (disease: {disease}): {error}")

# Group by folder
folder_stats = defaultdict(lambda: {'total': 0, 'ok': 0, 'missing_ages': 0})
for file_path, _ in all_ok:
    folder = file_path.split('/')[0] if '/' in file_path else "root"
    folder_stats[folder]['total'] += 1
    folder_stats[folder]['ok'] += 1

for file_path, _, _, _ in issues['missing_ages']:
    folder = file_path.split('/')[0] if '/' in file_path else "root"
    folder_stats[folder]['total'] += 1
    folder_stats[folder]['missing_ages'] += 1

print(f"\n\nFOLDER BREAKDOWN FOR SIMULATED DISEASES:")
for folder in sorted(folder_stats.keys()):
    stats = folder_stats[folder]
    print(f"  {folder}: {stats['total']} files - {stats['ok']} OK, {stats['missing_ages']} missing ages")
