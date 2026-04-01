#!/usr/bin/env python3
"""Check for gaps in age data (not just missing 0, but any missing ages in 0-100)"""

import zipfile
import csv
import io
from pathlib import Path

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

print("Checking for age gaps in simulated diseases (D356.csv)...\n")
print("=" * 80)

issues_found = []

with zipfile.ZipFile(zip_path, 'r') as z:
    for disease in sorted(simulated_diseases):
        zip_file_path = f"diseases/{disease}/D356.csv"

        if zip_file_path not in z.namelist():
            print(f"[NOT FOUND] {disease}/D356.csv")
            continue

        content = z.read(zip_file_path).decode('utf-8')
        reader = csv.DictReader(io.StringIO(content))

        ages = set()
        for row in reader:
            try:
                age = int(row['age'])
                ages.add(age)
            except (ValueError, KeyError):
                pass

        # Check for gaps in 0-100
        missing_ages = [age for age in range(0, 101) if age not in ages]

        if missing_ages:
            issues_found.append((disease, missing_ages))
            print(f"[GAPS] {disease}/D356.csv")
            print(f"        Age range: {min(ages)}-{max(ages)}")
            print(f"        Missing {len(missing_ages)} ages: {missing_ages}")
        else:
            print(f"[OK] {disease}/D356.csv - Complete coverage 0-100")

print("\n" + "=" * 80)
if issues_found:
    print(f"\n[WARN] Found gaps in {len(issues_found)} disease(s):")
    for disease, missing in issues_found:
        print(f"   - {disease}: missing {len(missing)} ages")
else:
    print("\n[OK] All simulated diseases have complete age coverage 0-100!")
