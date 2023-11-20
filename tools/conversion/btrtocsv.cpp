// ------------------------------------------------------------------------------
// This program takes a directory containing btrblocks files and a yaml schema
// and converts it back to CSV files.
//
// Example call: ./btrtocsv -btr pbi/Arade/ -csv Arade.csv
// ------------------------------------------------------------------------------
// Standard libs
#include <filesystem>
#include <fstream>
// ------------------------------------------------------------------------------
// External libs
#include "gflags/gflags.h"
#include "yaml-cpp/yaml.h"
#include "spdlog/spdlog.h"
#include "tbb/parallel_for.h"
// #define TBB_PREVIEW_GLOBAL_CONTROL
#include "tbb/global_control.h"
// ------------------------------------------------------------------------------
// Btrfiles library
#include "btrfiles.hpp"
// ------------------------------------------------------------------------------
// Btr internal includes
#include "common/Utils.hpp"
#include "scheme/SchemePool.hpp"
#include "cache/ThreadCache.hpp"
#include "compression/Datablock.hpp"
#include "compression/BtrReader.hpp"
#include "storage/StringPointerArrayViewer.hpp"
#include "storage/Relation.hpp"
// ------------------------------------------------------------------------------
DEFINE_string(btr, "btr", "Directory with btr input");
// TODO we need to get rid of the schema here. It MUST be possible to read the files in without it, or generate it from the files.
DEFINE_string(csv, "data.csv", "File for CSV output");
DEFINE_uint32(threads, 8, "");
// ------------------------------------------------------------------------------
using namespace btrblocks;
// ------------------------------------------------------------------------------
void outputChunk(std::ofstream &csvstream, u32 tuple_count,
                 const std::vector<std::pair<u32, u32>> &counters,
                 const std::vector<std::vector<u8>> &decompressed_columns,
                 std::vector<std::vector<BtrReader>> &readers,
                 const std::vector<u32>& requires_copy) {
    const std::string field_separator = "|";
    const std::string line_separator = "\n";

    for (size_t row = 0; row < tuple_count; row++) {
        for (size_t col = 0; col < decompressed_columns.size(); col++) {
            BtrReader &reader = readers[col][counters[col].first];
            BitmapWrapper *nullmap = reader.getBitmap(counters[col].second-1);

            bool is_null;
            if (nullmap->type() == BitmapType::ALLZEROS) {
                is_null = true;
            } else if (nullmap->type() == BitmapType::ALLONES) {
                is_null = false;
            } else {
                is_null = !(nullmap->get_bitset()->test(row));
            }

            if (!is_null) {
                switch (reader.getColumnType()) {
                    case ColumnType::INTEGER: {
                        auto int_array = reinterpret_cast<const INTEGER *>(decompressed_columns[col].data());
                        csvstream << int_array[row];
                        break;
                    }
                    case ColumnType::DOUBLE: {
                        auto double_array = reinterpret_cast<const DOUBLE *>(decompressed_columns[col].data());
                        csvstream << double_array[row];
                        break;
                    }
                    case ColumnType::STRING: {
                        std::string data;
                        if (requires_copy[col]) {
                            auto string_pointer_array_viewer = StringPointerArrayViewer(reinterpret_cast<const u8 *>(decompressed_columns[col].data()));
                            data = string_pointer_array_viewer(row);
                        } else {
                            auto string_array_viewer = StringArrayViewer(reinterpret_cast<const u8 *>(decompressed_columns[col].data()));
                            data = string_array_viewer(row);
                        }
                        csvstream << data;
                        break;
                    }
                    default: {
                        throw Generic_Exception(
                                "Type " + ConvertTypeToString(reader.getColumnType()) + " not supported");
                    }
                }
            } else {
                csvstream << "null";
            }

            if (col+1 != decompressed_columns.size()) {
                csvstream << field_separator;
            }
        }
        csvstream << line_separator;
    }
}

int main(int argc, char **argv)
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    std::filesystem::path btr_dir = FLAGS_btr;

    // This seems necessary to be
    SchemePool::refresh();

    // Init TBB TODO: is that actually still necessary ?
    tbb::global_control thread_ctrl(tbb::global_control::max_allowed_parallelism, FLAGS_threads);

    // Open output file
    auto csvstream = std::ofstream(FLAGS_csv);
    csvstream << std::setprecision(32);

    // Read the metadata
    std::vector<char> raw_file_metadata;
    const FileMetadata *file_metadata;
    {
        auto metadata_path = btr_dir / "metadata";
        Utils::readFileToMemory(metadata_path.string(), raw_file_metadata);
        file_metadata = reinterpret_cast<const FileMetadata *>(raw_file_metadata.data());
    }

    // Prepare the readers
    std::vector<std::vector<BtrReader>> readers(file_metadata->num_columns);
    std::vector<std::vector<std::vector<char>>> compressed_data(file_metadata->num_columns);
    tbb::parallel_for(u32(0), file_metadata->num_columns, [&](u32 column_i) {
        compressed_data[column_i].resize(file_metadata->parts[column_i].num_parts);
        for (u32 part_i = 0; part_i < file_metadata->parts[column_i].num_parts; part_i++) {
            auto path = btr_dir / ("column" + std::to_string(column_i) + "_part" + std::to_string(part_i));
            Utils::readFileToMemory(path.string(), compressed_data[column_i][part_i]);
            readers[column_i].emplace_back(compressed_data[column_i][part_i].data());
        }
    });

    // For each column counters contains a pair of <current_part_i, current_chunk_within_part_i>
    std::vector<std::pair<u32, u32>> counters(file_metadata->num_columns, {0, 0});

    for (u32 chunk_i = 0; chunk_i < file_metadata->num_chunks; chunk_i++) {
        std::vector<std::vector<u8>> outputs(file_metadata->num_columns);
        /*
         * Intuitively we would use vector<bool> for requires_copy. However the
         * stdlib may implement vector<bool> as a bitset. When reading and
         * writing and operating won't just touch the current bit. It will also
         * touch all the bits around it (in the same byte). This leads to
         * contention and ultimately wrong values. Using another integral type
         * vector solves the contention problem.
         */
        std::vector<u32> requires_copy(file_metadata->num_columns);
        u32 tuple_count = 0;
        tbb::parallel_for(u32(0), file_metadata->num_columns, [&](u32 column_i) {
            u32 part_i = counters[column_i].first;
            BtrReader &reader = readers[column_i][part_i];
            if (counters[column_i].second >= reader.getChunkCount()) {
                counters[column_i].first++;
                part_i++;
                counters[column_i].second = 0;
                reader = readers[column_i][part_i];
            }

            u32 part_chunk_i = counters[column_i].second;
            if (column_i == 0) {
                tuple_count = reader.getTupleCount(part_chunk_i);
            }
            requires_copy[column_i] = reader.readColumn(outputs[column_i], part_chunk_i);
            counters[column_i].second++;
        });
        outputChunk(csvstream, tuple_count, counters, outputs, readers, requires_copy);
    }
}
