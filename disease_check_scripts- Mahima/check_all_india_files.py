#!/usr/bin/env python3
"""Check ALL India-related files (D356, IF356, P356, M356, etc.) across ALL folders for simulated diseases"""

import zipfile
import csv
import io
from pathlib import Path
from collections import defaultdict
import re

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

# Patterns for India files
india_patterns = [
    r'D356\.csv$',      # Disease data
    r'IF356\.csv$',     # Intervention factors
    r'P356/.*\.csv$',   # P356 folder files
    r'M356\.csv$',       # Mortality
    r'Pi356\.csv$',     # Population indicators
    r'P356\.csv$',      # Population
]

issues = defaultdict(list)
all_ok = []
all_files_checked = []

print("Checking ALL India-related files (D356, IF356, P356, M356, etc.) across ALL folders...\n")
print("=" * 80)

def is_india_file(filename):
    """Check if file matches India file patterns"""
    for pattern in india_patterns:
        if re.search(pattern, filename):
            return True
    return False

def extract_disease_from_path(filepath):
    """Extract disease name from file path"""
    parts = filepath.split('/')
    if len(parts) >= 3 and parts[0] == "diseases":
        return parts[1]
    # Check if any simulated disease appears in path
    for disease in simulated_diseases:
        if disease in filepath.lower():
            return disease
    return None

def check_file_ages(z, filepath, disease_name):
    """Check age coverage in a CSV file"""
    try:
        content = z.read(filepath).decode('utf-8')
        reader = csv.DictReader(io.StringIO(content))

        # Check if 'age' column exists
        if 'age' not in reader.fieldnames:
            return ('no_age_column', None)

        ages = set()
        for row in reader:
            try:
                age = int(row['age'])
                ages.add(age)
            except (ValueError, KeyError):
                pass

        if not ages:
            return ('empty', None)

        min_age = min(ages)
        max_age = max(ages)

        # Check for missing ages in range 0-100
        missing_ages = []
        for age in range(0, 101):
            if age not in ages:
                missing_ages.append(age)

        return ('ok', {
            'ages': ages,
            'min_age': min_age,
            'max_age': max_age,
            'missing_ages': missing_ages,
            'has_age_0': 0 in ages,
            'has_age_100': 100 in ages
        })
    except Exception as e:
        return ('error', str(e))

with zipfile.ZipFile(zip_path, 'r') as z:
    # Find all files
    all_files = z.namelist()

    # Filter for India files
    india_files = [f for f in all_files if is_india_file(f)]

    print(f"Found {len(india_files)} India-related files total\n")

    for zip_file_path in sorted(india_files):
        all_files_checked.append(zip_file_path)
        folder_name = zip_file_path.split('/')[0] if '/' in zip_file_path else "root"
        disease_name = extract_disease_from_path(zip_file_path)

        # Check if this file is relevant to simulated diseases
        is_simulated = disease_name and disease_name in simulated_diseases

        if not is_simulated:
            # Still check it but don't report unless it's in diseases folder
            if folder_name == "diseases":
                continue  # Skip non-simulated disease files

        status, data = check_file_ages(z, zip_file_path, disease_name)

        if status == 'no_age_column':
            if is_simulated:
                issues['no_age_column'].append((zip_file_path, disease_name))
            continue
        elif status == 'empty':
            if is_simulated:
                issues['empty_files'].append((zip_file_path, disease_name))
            continue
        elif status == 'error':
            if is_simulated:
                issues['read_errors'].append((zip_file_path, disease_name, data))
            continue

        # File has age data
        min_age = data['min_age']
        max_age = data['max_age']
        missing_ages = data['missing_ages']
        has_age_0 = data['has_age_0']
        has_age_100 = data['has_age_100']

        if is_simulated:
            if missing_ages:
                issues['missing_ages'].append((zip_file_path, disease_name, missing_ages, min_age, max_age))
                print(f"[WARN] {zip_file_path}")
                print(f"        Disease: {disease_name}")
                print(f"        Age range: {min_age}-{max_age} ({len(data['ages'])} ages)")
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
                status_str = "[OK]"
                if not has_age_0:
                    status_str = "[WARN]"
                    issues['missing_age_0'].append((zip_file_path, disease_name))
                print(f"{status_str} {zip_file_path} (disease: {disease_name}, age range: {min_age}-{max_age})")

print("\n" + "=" * 80)
print("\nSUMMARY FOR SIMULATED DISEASES:")
print(f"  Total India-related files checked: {len(india_files)}")
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

# List all India files found (for reference)
print(f"\n\nALL INDIA FILES FOUND (for reference):")
for file_path in sorted(india_files):
    folder = file_path.split('/')[0] if '/' in file_path else "root"
    disease = extract_disease_from_path(file_path)
    is_sim = " [SIMULATED]" if disease and disease in simulated_diseases else ""
    print(f"  {file_path}{is_sim}")
