#!/usr/bin/env python3
"""Check which simulated diseases for India are missing age 0"""

import zipfile
import csv
import io
from pathlib import Path

# Diseases being simulated
smoking_pif = [
    "ischemicheartdisease",
    "diabetes",
    "asthma",
    "pulmonary",
    "trachealbronchuslungcancer",
    "otherpharynxcancer",
    "lowerrespiratoryinfections",
    "tuberculosis",
    "stroke",
    "cervicalcancer",
    "larynxcancer"
]

alcohol_pif = [
    "alcoholusedisorders",
    "cirrhosis",
    "lipandoralcavitycancer",
    "livercancer",
    "otherpharynxcancer",
    "roadinjuries",
    "selfharm",
    "intracerebralhemorrhage",
    "subarachnoidhemorrhage",
    "ischemicstroke",
    "tuberculosis"
]

joint_pif = [
    "pulmonary",
    "diabetes",
    "ischemicheartdisease",
    "larynxcancer",
    "trachealbronchuslungcancer",
    "otherpharynxcancer",
    "asthma",
    "lowerrespiratoryinfections",
    "tuberculosis",
    "stroke",
    "cervicalcancer",
    "alcoholusedisorders",
    "cirrhosis",
    "lipandoralcavitycancer",
    "livercancer",
    "roadinjuries",
    "selfharm",
    "intracerebralhemorrhage",
    "subarachnoidhemorrhage",
    "ischemicstroke"
]

# Combine all unique diseases
all_diseases = set(smoking_pif + alcohol_pif + joint_pif)

zip_path = Path("c:/healthgps-data/pif-data-v7.zip")

missing_age0 = []
has_age0 = []
not_found = []

print("Checking India (D356.csv) disease files for age 0...\n")
print("=" * 80)

with zipfile.ZipFile(zip_path, 'r') as z:
    for disease in sorted(all_diseases):
        zip_file_path = f"diseases/{disease}/D356.csv"

        if zip_file_path not in z.namelist():
            not_found.append(disease)
            print(f"[NOT FOUND] {disease}/D356.csv")
            continue

        # Read and check for age 0
        content = z.read(zip_file_path).decode('utf-8')
        reader = csv.DictReader(io.StringIO(content))

        ages = set()
        for row in reader:
            try:
                age = int(row['age'])
                ages.add(age)
            except (ValueError, KeyError):
                pass

        min_age = min(ages) if ages else None
        max_age = max(ages) if ages else None

        if 0 not in ages:
            missing_age0.append(disease)
            print(f"[MISSING AGE 0] {disease}/D356.csv (age range: {min_age}-{max_age})")
        else:
            has_age0.append(disease)
            print(f"[OK] {disease}/D356.csv (age range: {min_age}-{max_age})")

print("\n" + "=" * 80)
print("\nSUMMARY:")
print(f"  Total diseases checked: {len(all_diseases)}")
print(f"  [OK] Has age 0: {len(has_age0)}")
print(f"  [MISSING] Missing age 0: {len(missing_age0)}")
print(f"  [NOT FOUND] File not found: {len(not_found)}")

if missing_age0:
    print(f"\n⚠️  DISEASES MISSING AGE 0 ({len(missing_age0)}):")
    for disease in sorted(missing_age0):
        print(f"   - {disease}")

if not_found:
    print(f"\n⚠️  DISEASES NOT FOUND IN ZIP ({len(not_found)}):")
    for disease in sorted(not_found):
        print(f"   - {disease}")

if not missing_age0 and not not_found:
    print("\n✅ All simulated diseases have age 0 data!")
