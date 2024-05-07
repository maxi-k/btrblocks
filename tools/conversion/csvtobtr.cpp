// ------------------------------------------------------------------------------
// This program takes a csv file and its yaml schema and converts it to
// btrblocks files
//
// Example call: ./csvtobtr -create_btr
//                          -csv pbi/Arade/Arade_1.csv
//                          -yaml pbi/Arade/Arade_1.yaml
// ------------------------------------------------------------------------------
// Standard libs
#include <filesystem>
#include <fstream>
// ------------------------------------------------------------------------------
// External libs
#include <gflags/gflags.h>
#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>
#include <tbb/parallel_for.h>
#include <thread>
#define TBB_PREVIEW_GLOBAL_CONTROL 1
#include "tbb/global_control.h"
// ------------------------------------------------------------------------------
// Btr internal includes
#include "common/Utils.hpp"
#include "storage/Relation.hpp"
#include "scheme/SchemePool.hpp"
#include "compression/Datablock.hpp"
#include "compression/BtrReader.hpp"
#include "cache/ThreadCache.hpp"
// ------------------------------------------------------------------------------
// Btrfiles include
#include "btrfiles.hpp"
// ------------------------------------------------------------------------------
// Define command line flags
// TODO make the required flags mandatory/positional
DEFINE_string(btr, "btr", "Directory for btr output");
DEFINE_string(binary, "binary", "Directory for binary output");
DEFINE_string(yaml, "schema.yaml", "Schema in YAML format");
DEFINE_string(csv, "data.csv", "Original data in CSV format");
DEFINE_string(stats, "stats.txt", "File where stats are being stored");
DEFINE_string(compressionout,"compressionout.txt", "File where compressin times are being stored");
DEFINE_string(typefilter, "", "Only include columns with the selected type");
DEFINE_bool(create_binary, false, "Set if binary files are supposed to be created");
DEFINE_bool(create_btr, false, "If false will exit after binary creation");
DEFINE_bool(verify, true, "Verify that decompression works");
DEFINE_int32(chunk, -1, "Select a specific chunk to measure");
DEFINE_int32(column, -1, "Select a specific column to measure");
DEFINE_uint32(threads, -1, "");
// ------------------------------------------------------------------------------
using namespace btrblocks;
// ------------------------------------------------------------------------------
void verify_or_die(const std::string& filename, const std::vector<InputChunk> &input_chunks) {
    if (!FLAGS_verify) {
        return;
    }
    // Verify that decompression works
    thread_local std::vector<char> compressed_data;
    Utils::readFileToMemory(filename, compressed_data);
    BtrReader reader(compressed_data.data());
    for (SIZE chunk_i = 0; chunk_i < reader.getChunkCount(); chunk_i++) {
        std::vector<u8> output(reader.getDecompressedSize(chunk_i));
        bool requires_copy = reader.readColumn(output, chunk_i);
        auto bm = reader.getBitmap(chunk_i)->writeBITMAP();
        if (!input_chunks[chunk_i].compareContents(output.data(), bm, reader.getTupleCount(chunk_i),
                                         requires_copy)) {
            throw Generic_Exception("Decompression yields different contents");
        }
    }
}
// ------------------------------------------------------------------------------
int main(int argc, char **argv)
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    std::string binary_path = FLAGS_binary + "/";
    // This seems necessary to be
    SchemePool::refresh();

    if (FLAGS_threads < 1) {
      tbb::global_control c(tbb::global_control::max_allowed_parallelism,
                            std::thread::hardware_concurrency());
    } else {
      tbb::global_control c(tbb::global_control::max_allowed_parallelism,
                            FLAGS_threads);
    }

    // Load schema
    const auto schema = YAML::LoadFile(FLAGS_yaml);

    uint64_t binary_creation_time = 0;
    if (FLAGS_create_binary) {
        spdlog::info("Creating binary files in " + FLAGS_binary);
        // Load and parse CSV
        auto start_time = std::chrono::steady_clock::now();
        std::ifstream csv(FLAGS_csv);
        if (!csv.good()) {
            throw Generic_Exception("Unable to open specified csv file");
        }
        // parse writes the binary files
        files::convertCSV(FLAGS_csv, schema, FLAGS_binary);
        auto end_time = std::chrono::steady_clock::now();
        binary_creation_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    }

    if (!FLAGS_create_btr) {
        return 0;
    }

    spdlog::info("Creating btr files in " + FLAGS_btr);


    ColumnType typefilter;
    if (FLAGS_typefilter.empty()) {
        typefilter = ColumnType::UNDEFINED;
    } else if (FLAGS_typefilter == "integer") {
        typefilter = ColumnType::INTEGER;
    } else if (FLAGS_typefilter == "double") {
        typefilter = ColumnType::DOUBLE;
    } else if (FLAGS_typefilter == "string") {
        typefilter = ColumnType::STRING;
    } else {
        throw std::runtime_error("typefilter must be one of [integer, double, string]");
    }

    if (typefilter != ColumnType::UNDEFINED) {
        spdlog::info("Only considering columns with type " + FLAGS_typefilter);
    }

    // Create relation
    Relation relation = files::readDirectory(schema, FLAGS_binary.back() == '/' ? FLAGS_binary : FLAGS_binary + "/");
    std::filesystem::path yaml_path = FLAGS_yaml;
    relation.name = yaml_path.stem();

    // Prepare datastructures for btr compression
    //auto ranges = relation.getRanges(static_cast<SplitStrategy>(1), 9999);
    auto ranges = relation.getRanges(SplitStrategy::SEQUENTIAL, -1);
    assert(ranges.size() > 0);
    Datablock datablockV2(relation);
    std::filesystem::create_directory(FLAGS_btr);
//    if (!std::filesystem::create_directory(FLAGS_btr)) {
//        throw Generic_Exception("Unable to create btr output directory");
//    }

    // These counter are for statistics that match the harbook.
    std::vector<std::atomic_size_t> sizes_uncompressed(relation.columns.size());
    std::vector<std::atomic_size_t> sizes_compressed(relation.columns.size());
    std::vector<u32> part_counters(relation.columns.size());
    std::vector<ColumnType> types(relation.columns.size());

    // TODO run in parallel over individual columns and handle chunks inside
    // TODO collect statistics for overall metadata like
    //      - total tuple count
    //      - for every column: total number of parts
    //      - for every column: name, type
    // TODO chunk flag
    auto start_time = std::chrono::steady_clock::now();
    tbb::parallel_for(SIZE(0), relation.columns.size(), [&](SIZE column_i) {
        types[column_i] = relation.columns[column_i].type;
        if (typefilter != ColumnType::UNDEFINED && typefilter != types[column_i]) {
            return;
        }
        if (FLAGS_column != -1 && FLAGS_column != column_i) {
            return;
        }

        std::vector<InputChunk> input_chunks;
        std::string path_prefix = FLAGS_btr + "/" + "column" + std::to_string(column_i) + "_part";
        ColumnPart part;
        for (SIZE chunk_i = 0; chunk_i < ranges.size(); chunk_i++) {
            if (FLAGS_chunk != -1 && FLAGS_chunk != chunk_i) {
                continue;
            }

            auto input_chunk = relation.getInputChunk(ranges[chunk_i], chunk_i, column_i);
            std::vector<u8> data = Datablock::compress(input_chunk);
            sizes_uncompressed[column_i] += input_chunk.size;

            if (!part.canAdd(data.size())) {
                std::string filename = path_prefix + std::to_string(part_counters[column_i]);
                sizes_compressed[column_i] += part.writeToDisk(filename);
                part_counters[column_i]++;
                verify_or_die(filename, input_chunks);
                input_chunks.clear();
            }

            input_chunks.push_back(std::move(input_chunk));
            part.addCompressedChunk(std::move(data));
        }

        if (!part.chunks.empty()) {
            std::string filename = path_prefix + std::to_string(part_counters[column_i]);
            sizes_compressed[column_i] += part.writeToDisk(filename);
            part_counters[column_i]++;
            verify_or_die(filename, input_chunks);
            input_chunks.clear();
        }
    });

    Datablock::writeMetadata(FLAGS_btr + "/metadata", types, part_counters, ranges.size());
    std::ofstream stats_stream(FLAGS_stats);
    size_t total_uncompressed = 0;
    size_t total_compressed = 0;
    stats_stream << "Column,uncompressed,compressed\n";
    for (SIZE col=0; col  < relation.columns.size(); col++) {
        total_uncompressed += sizes_uncompressed[col];
        total_compressed += sizes_compressed[col];

        stats_stream << relation.columns[col].name << "," << sizes_uncompressed[col] << "," << sizes_compressed[col] << "\n";
    }
    auto end_time = std::chrono::steady_clock::now();
    uint64_t btr_creation_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

    stats_stream << "Total," << total_uncompressed << "," << total_compressed << std::endl;

    std::ofstream compressionout_stream(FLAGS_compressionout);
    double binary_creation_time_seconds = static_cast<double>(binary_creation_time) / 1e6;
    double btr_creation_time_seconds = static_cast<double>(btr_creation_time) / 1e6;

   compressionout_stream << "binary: " << binary_creation_time_seconds
                        << " btr: " << btr_creation_time_seconds
                        << " total: " << (binary_creation_time_seconds + btr_creation_time_seconds)
                        << " verify: " << FLAGS_verify << std::endl;
    return 0;
}
