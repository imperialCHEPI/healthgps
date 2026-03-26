@echo off
setlocal enabledelayedexpansion

echo ========================================
echo HealthGPS Automated Age Group Capture
echo ========================================
echo.

REM Configuration
set SIMULATION_CPP=src\HealthGPS\simulation.cpp
set BUILD_TARGET=HealthGPS
set OUTPUT_DIR=age_group_data

REM Age groups: min_age max_age group_name
set AGE_GROUPS=0 20 group_1_0_20 21 30 group_2_21_30 31 40 group_3_31_40 41 50 group_4_41_50 51 60 group_5_51_60 61 70 group_6_61_70 71 80 group_7_71_80 81 90 group_8_81_90 91 110 group_9_91_110

echo Creating output directories...
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

REM Create subdirectories for each age group
for %%A in (group_1_0_20 group_2_21_30 group_3_31_40 group_4_41_50 group_5_51_60 group_6_61_70 group_7_71_80 group_8_81_90 group_9_91_110) do (
    if not exist "%OUTPUT_DIR%\%%A" mkdir "%OUTPUT_DIR%\%%A"
    echo   Created: %OUTPUT_DIR%\%%A
)

echo.
echo Starting automated capture process...
echo.

REM Process each age group
set SUCCESS_COUNT=0
set TOTAL_COUNT=0

for %%A in (%AGE_GROUPS%) do (
    set /A TOTAL_COUNT+=1
    if !TOTAL_COUNT! equ 1 (
        set MIN_AGE=%%A
    ) else if !TOTAL_COUNT! equ 2 (
        set MAX_AGE=%%A
    ) else if !TOTAL_COUNT! equ 3 (
        set GROUP_NAME=%%A
        set /A TOTAL_COUNT=0

        echo ========================================
        echo Processing Age Group: !MIN_AGE!-!MAX_AGE! (!GROUP_NAME!)
        echo ========================================

        REM Step 1: Update simulation.cpp
        echo Updating simulation.cpp for ages !MIN_AGE!-!MAX_AGE!...
        powershell -Command "(Get-Content '%SIMULATION_CPP%') -replace 'int min_age = \d+;', 'int min_age = !MIN_AGE!;' -replace 'int max_age = \d+;', 'int max_age = !MAX_AGE!;' | Set-Content '%SIMULATION_CPP%'"

        REM Step 2: Build project
        echo Building HealthGPS project...
        cmake --build out\build\windows-release --target %BUILD_TARGET% --config Release
        if errorlevel 1 (
            echo ERROR: Build failed for age group !MIN_AGE!-!MAX_AGE!
            goto :next_group
        )

        REM Step 3: Run simulation (you need to replace this with your actual simulation command)
        echo Running simulation...
        echo WARNING: Please replace this with your actual simulation command
        echo Your simulation command should go here...
        REM Your actual simulation command goes here
        REM Example: your_simulation.exe --config your_config.json

        REM Step 4: Organize output files
        echo Organizing output files...
        if exist "risk_factor_inspection\foodvegetable_inspection.csv" (
            move "risk_factor_inspection\foodvegetable_inspection.csv" "%OUTPUT_DIR%\!GROUP_NAME!\"
            echo   Moved: foodvegetable_inspection.csv
        )
        if exist "risk_factor_inspection\foodfruit_inspection.csv" (
            move "risk_factor_inspection\foodfruit_inspection.csv" "%OUTPUT_DIR%\!GROUP_NAME!\"
            echo   Moved: foodfruit_inspection.csv
        )

        REM Clean up inspection directory
        if exist "risk_factor_inspection" (
            del "risk_factor_inspection\*" /q
        )

        set /A SUCCESS_COUNT+=1
        echo Age group !MIN_AGE!-!MAX_AGE! completed successfully!

        :next_group
        echo.
        timeout /t 2 /nobreak >nul
    )
)

echo ========================================
echo FINAL SUMMARY
echo ========================================
echo Successful age groups: %SUCCESS_COUNT%
echo Total age groups processed: 9
echo.
echo All data saved to: %OUTPUT_DIR%\
echo.
echo Next steps:
echo 1. Review the captured CSV files
echo 2. Combine files if needed for analysis
echo 3. Use the economist-friendly columns for plotting
echo.
pause
