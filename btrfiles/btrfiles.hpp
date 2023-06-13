#ifndef BTRFILES_H_
#define BTRFILES_H_
// ------------------------------------------------------------------------------
#include "storage/Relation.hpp"
// ------------------------------------------------------------------------------
#include <yaml-cpp/yaml.h>
// ------------------------------------------------------------------------------
// This library uses YAML files to store the schema of a table.
//
// A table (for example a CSV file) converted to btrblocks files will
// be a directory containing a YAML schema file and binary column files.
//
// An example schema file from the Public BI benchmark Arade workbook table:
// ```yaml
// columns:
//   - name: F1
//     type: string
//   - name: F2
//     type: string
//   - name: F3
//     type: skip     # skip unsupported date column
//   - name: F4
//     type: double
//   - name: F5
//     type: double
//   - name: F6
//     type: string
//   - name: F7
//     type: string
//   - name: F8
//     type: double
//   - name: F9
//     type: double
//   - name: Number of Records
//     type: integer
//   - name: WNET (bin)
//     type: integer
// ```
// ------------------------------------------------------------------------------
namespace btrblocks::files {
// ------------------------------------------------------------------------------
/// Given a YAML schema and an input directory where uncompressed (!) binary files are stored,
/// read the binary files and return a relation. If only_type is not empty, only the
/// btrfiles of the given column type are read.
Relation readDirectory(const YAML::Node& schema, const string& columns_dir, const string& only_type = "");
/// Given a YAML schema, parse the given csv file and write uncompressed (!)
/// binary files to the given output directory.
void convertCSV(const string csv_path, const YAML::Node &schema, const string &out_dir, const string &csv_separator = "|");
// ------------------------------------------------------------------------------
} // namespace btrblocks::files
// ------------------------------------------------------------------------------
#endif // BTRFILES_H_
