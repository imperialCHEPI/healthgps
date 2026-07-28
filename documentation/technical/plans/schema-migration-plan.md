# Schema Migration Plan: Unified Schema

## Global Health Policy Simulation model

{% include nav-technical-subdir.md %}

**Related:** [Project requirements plan](project-requirements-plan.md) | [Technical index](../README.md) | [Documentation index](../../README.md)

## ðŸŽ¯ **Goal**

Consolidate v1 and v2 schemas into a single unified schema that works for all use cases.

## ðŸ“‹ **Current State**

- **v1 schemas**: Basic features, used by India project
- **v2 schemas**: Extended features (PhysicalActivityModels, Trend properties), used by FINCH project
- **Problem**: Version management complexity, maintenance overhead

## âœ… **New Unified Schema**

- **Location**: `schemas/config/models/staticlinear.json`
- **Features**: Combines all v1 and v2 features
- **Backward Compatibility**: Works with both old and new configurations

## ðŸ”„ **Migration Steps**

### **Step 1: Update CMake Configuration**

Update `src/HealthGPS.Console/CMakeLists.txt` to copy unified schemas:

```cmake
# Copy unified schemas instead of versioned schemas
file(COPY ${CMAKE_SOURCE_DIR}/schemas/config
     DESTINATION ${CMAKE_BINARY_DIR}/src/HealthGPS.Console/schemas)
```

### **Step 2: Update Schema References**

- Update `$id` references in configuration files
- Update schema loading code to use unified path

### **Step 3: Test Both Projects**

- âœ… India project (v1 features)
- âœ… FINCH project (v2 features)
- âœ… Future projects (all features)

### **Step 4: Clean Up (Optional)**

- Remove v1 and v2 schema directories
- Update documentation

## ðŸš€ **Benefits**

### **For Developers:**

- **Single source of truth** for schemas
- **No version confusion** - one schema works for all
- **Easier maintenance** - only one schema to update
- **Future-proof** - new features can be added as optional properties

### **For Projects:**

- **India**: Works with 3-category income, no regions
- **FINCH**: Works with continuous income, PhysicalActivityModels
- **Future**: Can use any combination of features

## ðŸ“Š **Schema Features Matrix**

| Feature | v1 | v2 | Unified | Status |
|---------|----|----|---------|--------|
| Basic IncomeModels | âœ… | âœ… | âœ… | âœ… |
| Flexible Income Categories | âœ… | âœ… | âœ… | âœ… |
| Region Definitions | âœ… | âœ… | âœ… | âœ… |
| Ethnicity Definitions | âœ… | âœ… | âœ… | âœ… |
| PhysicalActivityModels | âŒ | âœ… | âœ… | âœ… |
| Trend Properties | âŒ | âœ… | âœ… | âœ… |
| Required Fields | âŒ | âœ… | âœ… | âœ… |
| project_requirements (optional) | N/A | âœ… | âœ… | Optional in v1 config; legacy configs work without it |
| output.individual_id_tracking | âŒ | âŒ | âœ… | Optional; per-person CSV tracking (MAHIMA) |

## ðŸ”§ **Implementation Notes**

### **Backward Compatibility:**

- All v1 configurations work unchanged
- All v2 configurations work unchanged
- New configurations can use any combination of features

### **Schema Structure:**

- **Optional properties**: All new features are optional
- **Flexible income**: Supports any income category names
- **Dynamic regions/ethnicities**: Configurable per project
- **Extensible**: Easy to add new features

### **Output schema (v1):**

- **`schemas/v1/config/output.json`**: Defines `output` (folder, file_name, comorbidities). Supports `additionalProperties: true`.
- **Optional `individual_id_tracking`** (MAHIMA): When present, enables a second CSV output (`_IndividualIDTracking.csv`) with filtered per-person rows (run, time, scenario, id, age, gender, region, ethnicity, income_category, plus selected risk factors). Filters: enabled, age_min, age_max, gender, regions, ethnicities, risk_factors, years, scenarios. See example in `input-data/data/KevinHall_FINCH/config.json`.

### **Top-level config (v1):**

- **`project_requirements`** is **optional** in `schemas/v1/config.json`. If omitted, code uses default-constructed values (e.g. income.type = "categorical" for legacy configs like India). If present, full demographics/income/PA/trend/two_stage settings are read from the config.

### **Future Improvements:**

- Add new properties as optional
- Maintain backward compatibility
- Single schema for all projects

---

**Author:** Mahima Ghosh
