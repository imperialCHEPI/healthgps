#include "pch.h"

#include "HealthGPS.Input/schema.h"

TEST(SchemaUri, KeepsUriWhenThereIsNoFragment) {
    // MAHIMA: A whole-file $ref has no JSON pointer. The resolver must keep it.
    const auto uri = std::string_view{
        "https://raw.githubusercontent.com/imperialCHEPI/healthgps/main/schemas/v1/config.json"};
    EXPECT_EQ(std::string{uri}, hgps::input::strip_schema_fragment(uri));
}

TEST(SchemaUri, DropsJsonPointerFragment) {
    // MAHIMA: jsoncons 1.x appends the pointer. The file on disk stops at '#'.
    const auto with_fragment = std::string_view{
        "https://raw.githubusercontent.com/imperialCHEPI/healthgps/main/schemas/v1/"
        "config.json#/$defs/foo"};
    EXPECT_EQ(
        "https://raw.githubusercontent.com/imperialCHEPI/healthgps/main/schemas/v1/config.json",
        hgps::input::strip_schema_fragment(with_fragment));
}

TEST(SchemaUri, DropsEmptyFragment) {
    EXPECT_EQ("schemas/v1/config.json",
              hgps::input::strip_schema_fragment("schemas/v1/config.json#"));
}
