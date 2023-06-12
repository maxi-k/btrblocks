// ------------------------------------------------------------------------------
#include "btrfiles.hpp"
#include "storage/Relation.hpp"
// ------------------------------------------------------------------------------
#include <yaml-cpp/yaml.h>
// ------------------------------------------------------------------------------
namespace btrblocks::files {
Relation readDirectory(const YAML::Node& schema, const string& columns_dir, const string& only_type) {
  die_if(columns_dir.back() == '/');
  Relation result;
  result.columns.reserve(schema["columns"].size());
  const auto& columns = schema["columns"];
  for (u32 column_i = 0; column_i < columns.size(); column_i++) {
    const auto& column = columns[column_i];
    const auto column_name = column["name"].as<string>();
    auto column_type = column["type"].as<string>();
    if (column_type == "smallint") {
      column_type = "integer";
    } else if (column_type == "float") {
      column_type = "double";
    }
    // -------------------------------------------------------------------------------------
    if (only_type != "" && column_type != only_type) { continue; }
    // -------------------------------------------------------------------------------------
    const string column_file_prefix =
        columns_dir + std::to_string(column_i + 1) + "_" + column_name;
    const string column_file_path = column_file_prefix + "." + column_type;
    if (column_type == "integer" || column_type == "double" || column_type == "string") {
      result.addColumn(column_file_path);
    }
  }
  return result;
}
// ------------------------------------------------------------------------------
}  // namespace btrblocks::files
// -------------------------------------------------------------------------------------
