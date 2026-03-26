#!/usr/bin/env python3
"""Get summary only from check_all_csv_files.py"""

import subprocess
import sys

result = subprocess.run([sys.executable, "check_all_csv_files.py"],
                       capture_output=True, text=True, encoding='utf-8', errors='replace')

# Find and print summary section
lines = result.stdout.split('\n')
in_summary = False
summary_lines = []

for line in lines:
    if "SUMMARY:" in line:
        in_summary = True
    if in_summary:
        summary_lines.append(line)
        if "FOLDER BREAKDOWN:" in line:
            # Get rest of output
            idx = lines.index(line)
            summary_lines.extend(lines[idx:])
            break

print('\n'.join(summary_lines))
