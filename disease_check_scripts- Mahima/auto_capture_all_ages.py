#!/usr/bin/env python3
"""
Automated Age Group Data Capture Script for HealthGPS
This script automatically runs the simulation for all age groups and organizes the output.
"""

import os
import shutil
import subprocess
import time
import re
from pathlib import Path

# Configuration
SIMULATION_CPP_PATH = "src/HealthGPS/simulation.cpp"
BUILD_TARGET = "HealthGPS"
OUTPUT_BASE_DIR = "age_group_data"
RISK_FACTOR_INSPECTION_DIR = "risk_factor_inspection"

# Age groups to capture
AGE_GROUPS = [
    (0, 20, "group_1_0_20"),
    (21, 30, "group_2_21_30"),
    (31, 40, "group_3_31_40"),
    (41, 50, "group_4_41_50"),
    (51, 60, "group_5_51_60"),
    (61, 70, "group_6_61_70"),
    (71, 80, "group_7_71_80"),
    (81, 90, "group_8_81_90"),
    (91, 110, "group_9_91_110")
]

def create_directories():
    """Create output directories for each age group."""
    print("📁 Creating output directories...")

    # Create base directory
    Path(OUTPUT_BASE_DIR).mkdir(exist_ok=True)

    # Create subdirectories for each age group
    for min_age, max_age, group_name in AGE_GROUPS:
        group_dir = Path(OUTPUT_BASE_DIR) / group_name
        group_dir.mkdir(exist_ok=True)
        print(f"   Created: {group_dir}")

def update_simulation_cpp(min_age, max_age):
    """Update simulation.cpp with the specified age range."""
    print(f"🔧 Updating simulation.cpp for ages {min_age}-{max_age}...")

    # Read the file
    with open(SIMULATION_CPP_PATH, 'r', encoding='utf-8') as f:
        content = f.read()

    # Replace the age range lines
    pattern = r'(int min_age = )\d+;\s*\n\s*(int max_age = )\d+;'
    replacement = f'\\1{min_age};\\n            \\2{max_age};'

    new_content = re.sub(pattern, replacement, content)

    # Write back to file
    with open(SIMULATION_CPP_PATH, 'w', encoding='utf-8') as f:
        f.write(new_content)

    print(f"   Updated: min_age = {min_age}, max_age = {max_age}")

def build_project():
    """Build the HealthGPS project."""
    print("🔨 Building HealthGPS project...")

    try:
        result = subprocess.run([
            "cmake", "--build", "out\\build\\windows-release",
            "--target", BUILD_TARGET, "--config", "Release"
        ], check=True, capture_output=True, text=True)
        print("   ✅ Build successful!")
        return True
    except subprocess.CalledProcessError as e:
        print(f"   ❌ Build failed: {e}")
        print(f"   Error output: {e.stderr}")
        return False

def run_simulation():
    """Run the HealthGPS simulation."""
    print("🚀 Running HealthGPS simulation...")

    try:
        # Note: You'll need to replace this with your actual simulation command
        # This is a placeholder - adjust based on how you normally run your simulation
        result = subprocess.run([
            "your_simulation_command_here"
        ], check=True, capture_output=True, text=True, timeout=600)  # 10 minute timeout

        print("   ✅ Simulation completed successfully!")
        return True
    except subprocess.TimeoutExpired:
        print("   ⏰ Simulation timed out (10 minutes)")
        return False
    except subprocess.CalledProcessError as e:
        print(f"   ❌ Simulation failed: {e}")
        return False
    except FileNotFoundError:
        print("   ⚠️  Simulation command not found. Please update the script with your actual simulation command.")
        return False

def organize_output_files(group_name):
    """Move generated CSV files to the appropriate age group directory."""
    print(f"📦 Organizing output files for {group_name}...")

    group_dir = Path(OUTPUT_BASE_DIR) / group_name
    inspection_dir = Path(RISK_FACTOR_INSPECTION_DIR)

    if not inspection_dir.exists():
        print(f"   ⚠️  Inspection directory {inspection_dir} not found")
        return

    # Move CSV files
    csv_files = list(inspection_dir.glob("*_inspection.csv"))

    if not csv_files:
        print(f"   ⚠️  No CSV files found in {inspection_dir}")
        return

    for csv_file in csv_files:
        dest_path = group_dir / csv_file.name
        shutil.move(str(csv_file), str(dest_path))
        print(f"   Moved: {csv_file.name} -> {group_name}/")

    # Clean up any remaining files in inspection directory
    for file in inspection_dir.glob("*"):
        if file.is_file():
            file.unlink()

def run_age_group(min_age, max_age, group_name):
    """Run the complete process for one age group."""
    print(f"\n{'='*60}")
    print(f"🎯 Processing Age Group: {min_age}-{max_age} ({group_name})")
    print(f"{'='*60}")

    start_time = time.time()

    try:
        # Step 1: Update simulation.cpp
        update_simulation_cpp(min_age, max_age)

        # Step 2: Build project
        if not build_project():
            print(f"❌ Failed to build for age group {min_age}-{max_age}")
            return False

        # Step 3: Run simulation
        if not run_simulation():
            print(f"❌ Failed to run simulation for age group {min_age}-{max_age}")
            return False

        # Step 4: Organize output files
        organize_output_files(group_name)

        elapsed_time = time.time() - start_time
        print(f"✅ Age group {min_age}-{max_age} completed in {elapsed_time:.1f} seconds")

        return True

    except Exception as e:
        print(f"❌ Error processing age group {min_age}-{max_age}: {e}")
        return False

def create_summary_report():
    """Create a summary report of all captured data."""
    print("\n📊 Creating summary report...")

    report_path = Path(OUTPUT_BASE_DIR) / "CAPTURE_SUMMARY.md"

    with open(report_path, 'w') as f:
        f.write("# Age Group Data Capture Summary\n\n")
        f.write(f"Generated on: {time.strftime('%Y-%m-%d %H:%M:%S')}\n\n")

        f.write("## Captured Age Groups\n\n")
        f.write("| Group | Age Range | Directory | Files |\n")
        f.write("|-------|-----------|-----------|-------|\n")

        for min_age, max_age, group_name in AGE_GROUPS:
            group_dir = Path(OUTPUT_BASE_DIR) / group_name
            if group_dir.exists():
                csv_files = list(group_dir.glob("*.csv"))
                f.write(f"| {group_name} | {min_age}-{max_age} | {group_name}/ | {len(csv_files)} files |\n")
            else:
                f.write(f"| {group_name} | {min_age}-{max_age} | ❌ Not captured | 0 files |\n")

        f.write("\n## File Locations\n\n")
        f.write("- **Base Directory**: `age_group_data/`\n")
        f.write("- **Individual Groups**: `age_group_data/group_X_YY_ZZ/`\n")
        f.write("- **CSV Files**: `foodvegetable_inspection.csv`, `foodfruit_inspection.csv`\n")

        f.write("\n## Next Steps\n\n")
        f.write("1. Review the captured CSV files\n")
        f.write("2. Combine files if needed for analysis\n")
        f.write("3. Use the economist-friendly columns for plotting\n")

    print(f"   Summary report created: {report_path}")

def main():
    """Main function to run the automated capture process."""
    print("🚀 HealthGPS Automated Age Group Data Capture")
    print("=" * 50)

    # Check if simulation.cpp exists
    if not Path(SIMULATION_CPP_PATH).exists():
        print(f"❌ Error: {SIMULATION_CPP_PATH} not found!")
        return

    # Create directories
    create_directories()

    # Process each age group
    successful_groups = []
    failed_groups = []

    for min_age, max_age, group_name in AGE_GROUPS:
        success = run_age_group(min_age, max_age, group_name)

        if success:
            successful_groups.append((min_age, max_age, group_name))
        else:
            failed_groups.append((min_age, max_age, group_name))

        # Small delay between runs
        time.sleep(2)

    # Create summary report
    create_summary_report()

    # Final summary
    print(f"\n{'='*60}")
    print("📋 FINAL SUMMARY")
    print(f"{'='*60}")
    print(f"✅ Successful age groups: {len(successful_groups)}")
    print(f"❌ Failed age groups: {len(failed_groups)}")

    if successful_groups:
        print("\nSuccessful groups:")
        for min_age, max_age, group_name in successful_groups:
            print(f"  - {group_name} (ages {min_age}-{max_age})")

    if failed_groups:
        print("\nFailed groups:")
        for min_age, max_age, group_name in failed_groups:
            print(f"  - {group_name} (ages {min_age}-{max_age})")

    print(f"\n📁 All data saved to: {OUTPUT_BASE_DIR}/")
    print("🎉 Automated capture process completed!")

if __name__ == "__main__":
    main()
