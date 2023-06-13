// -------------------------------------------------------------------------------------
#include <filesystem>
#include <fstream>
#include <random>
#include <tbb/parallel_for_each.h>
// -------------------------------------------------------------------------------------
#include "gflags/gflags.h"
#include "tbb/parallel_for.h"
#include "tbb/task_scheduler_init.h"
// -------------------------------------------------------------------------------------
#include "common/PerfEvent.hpp"
#include "common/Utils.hpp"
#include "compression/BtrReader.hpp"
#include "scheme/SchemePool.hpp"
// -------------------------------------------------------------------------------------
DEFINE_string(btr, "btr", "Directory with btr input");
DEFINE_int32(threads, 1, "Number of threads used. not specifying lets tbb decide");
DEFINE_int32(column, -1, "Select a specific column to measure");
DEFINE_string(typefilter, "", "Only measure columns with given type");
//DEFINE_int32(chunk, -1, "Select a specific chunk to measure");
DEFINE_uint32(reps, 1, "Loop reps times");
DEFINE_bool(perfevent, false, "Profile with perf event if true");
DEFINE_bool(output_summary, false, "Output a summary of total speed and size");
DEFINE_bool(output_columns, true, "Output speeds and sizes for single columns");
DEFINE_bool(print_simd_debug, false, "Print SIMD usage debug information");
// -------------------------------------------------------------------------------------
using namespace btrblocks;
// -------------------------------------------------------------------------------------
void reset_bitmaps(const FileMetadata *metadata, std::vector<std::vector<BtrReader>> &readers, std::vector<u32> &columns) {
    tbb::parallel_for_each(columns, [&](u32 column_i) {
        tbb::parallel_for(u32(0), metadata->parts[column_i].num_parts, [&](u32 part_i) {
            auto &reader = readers[column_i][part_i];
            tbb::parallel_for(u32(0), reader.getChunkCount(), [&](u32 chunk_i) {
                reader.releaseBitmap(chunk_i);
            });
        });
    });
}
// -------------------------------------------------------------------------------------
u64 measure(const FileMetadata *metadata, std::vector<std::vector<BtrReader>> &readers, std::vector<u64> &runtimes, std::vector<u32> &columns) {
    // Make sure no bitmap is cached
    reset_bitmaps(metadata, readers, columns);

    auto total_start_time = std::chrono::steady_clock::now();

    tbb::parallel_for_each(columns, [&](u32 column_i) {
        // TODO not sure if measuring the time like that will cause problems
        auto start_time = std::chrono::steady_clock::now();
        tbb::parallel_for(u32(0), metadata->parts[column_i].num_parts, [&](u32 part_i) {
            auto &reader = readers[column_i][part_i];
            tbb::parallel_for(u32(0), reader.getChunkCount(), [&](u32 chunk_i) {
                thread_local std::vector<u8> decompressed_data;
                reader.readColumn(decompressed_data, chunk_i);
            });
        });
        auto end_time = std::chrono::steady_clock::now();
        auto runtime = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        runtimes[column_i] += runtime.count();
    });

    auto total_end_time = std::chrono::steady_clock::now();
    auto total_runtime = std::chrono::duration_cast<std::chrono::microseconds>(total_end_time - total_start_time);
    return total_runtime.count();
}
// -------------------------------------------------------------------------------------
u64 measure_single_thread(const FileMetadata *metadata, std::vector<std::vector<BtrReader>> &readers, std::vector<u64> &runtimes, std::vector<u32> &columns) {
    reset_bitmaps(metadata, readers, columns);

    auto total_start_time = std::chrono::steady_clock::now();
    for (u32 column_i : columns) {
        for (u32 part_i = 0; part_i < metadata->parts[column_i].num_parts; part_i++) {
            auto &reader = readers[column_i][part_i];
            for (u32 chunk_i = 0; chunk_i < reader.getChunkCount(); chunk_i++) {
                for (u32 rep = 0; rep < FLAGS_reps; rep++) {
                    thread_local std::vector<u8> decompressed_data;
                    reader.releaseBitmap(chunk_i);
                    auto start_time = std::chrono::steady_clock::now();
                    reader.readColumn(decompressed_data, chunk_i);
                    auto end_time = std::chrono::steady_clock::now();
                    auto runtime = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
                    runtimes[column_i] += runtime.count();
                }
            }
        }
    }
    auto total_end_time = std::chrono::steady_clock::now();
    auto total_runtime = std::chrono::duration_cast<std::chrono::microseconds>(total_end_time - total_start_time);
    return total_runtime.count();
}
// -------------------------------------------------------------------------------------
int main(int argc, char **argv) {
    if (FLAGS_print_simd_debug) {
#if BTR_USE_SIMD
        std::cerr << "using simd" << std::endl;
#else
        std::cerr << "NOT using simd" << std::endl;
#endif
    }
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    std::filesystem::path btr_dir = FLAGS_btr;

    SchemePool::refresh();

    int threads;
    if (FLAGS_threads < 1) {
        // Automatic selection
        threads = -1;
    } else {
        threads = FLAGS_threads;
    }
    tbb::task_scheduler_init init(threads);

    // Read the metadata
    std::vector<char> raw_file_metadata;
    const FileMetadata *file_metadata;
    {
        auto metadata_path = btr_dir / "metadata";
        Utils::readFileToMemory(metadata_path.string(), raw_file_metadata);
        file_metadata = reinterpret_cast<const FileMetadata *>(raw_file_metadata.data());
    }

    // Filter columns
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
        throw std::runtime_error("filter_type must be one of [integer, double, string]");
    }

    std::vector<u32> columns;
    if (FLAGS_column != -1) {
        if (typefilter != ColumnType::UNDEFINED && file_metadata->parts[FLAGS_column].type != typefilter) {
            std::cerr << "Type of selected column " << FLAGS_column << " does not match filtered type" << std::endl;
            exit(EXIT_FAILURE);
        }
        columns.push_back(FLAGS_column);
    } else if (typefilter != ColumnType::UNDEFINED) {
        for (u32 column = 0; column < file_metadata->num_columns; column++) {
            if (file_metadata->parts[column].type == typefilter) {
                columns.push_back(column);
            }
        }
    } else {
        columns.resize(file_metadata->num_columns);
        std::iota(columns.begin(), columns.end(), 0);
    }

    std::cerr << "Decompressing columns: ";
    for (u32 column : columns) {
        std::cerr << column << " ";
    }
    std::cerr << std::endl;

    // Prepare the readers
    std::vector<std::vector<BtrReader>> readers(file_metadata->num_columns);
    std::vector<std::vector<std::vector<char>>> compressed_data(file_metadata->num_columns);
    tbb::parallel_for_each(columns, [&](u32 column_i) {
        compressed_data[column_i].resize(file_metadata->parts[column_i].num_parts);
        for (u32 part_i = 0; part_i < file_metadata->parts[column_i].num_parts; part_i++) {
            auto path = btr_dir / ("column" + std::to_string(column_i) + "_part" + std::to_string(part_i));
            Utils::readFileToMemory(path.string(), compressed_data[column_i][part_i]);
            readers[column_i].emplace_back(compressed_data[column_i][part_i].data());
        }
    });
    std::vector<u64> runtimes(file_metadata->num_columns);

    // Measure once to make sure all buffers are allocated properly
    measure(file_metadata, readers, runtimes, columns);
    std::fill(runtimes.begin(), runtimes.end(), 0);

    u64 total_runtime = 0;
    // Actual measurement
    if (false) {
        total_runtime = measure_single_thread(file_metadata, readers, runtimes, columns);
    } else {
        for (u32 rep = 0; rep < FLAGS_reps; rep++) {
            total_runtime += measure(file_metadata, readers, runtimes, columns);
        }
    }

    // Collect sizes
    std::vector<size_t> decompressed_sizes(file_metadata->num_columns, 0);
    std::vector<size_t> compressed_sizes(file_metadata->num_columns, 0);
    size_t total_size = 0;
    size_t total_compressed_size = 0;
    for (u32 column_i : columns) {
        for (u32 part_i = 0; part_i < file_metadata->parts[column_i].num_parts; part_i++) {
            BtrReader &reader = readers[column_i][part_i];
            compressed_sizes[column_i] += compressed_data[column_i][part_i].size();
            for (u32 chunk_i = 0; chunk_i < reader.getChunkCount(); chunk_i++) {
                size_t s = reader.getDecompressedDataSize(chunk_i);
                decompressed_sizes[column_i] += s;
                total_size += s;
            }
        }
        total_compressed_size += compressed_sizes[column_i];
    }

    if (FLAGS_output_columns) {
        for (u32 column_i : columns) {
            double average_runtime = static_cast<double>(runtimes[column_i]) / static_cast<double>(FLAGS_reps);
            double mb = static_cast<double>(decompressed_sizes[column_i]) / (1024.0 * 1024.0);
            double s = average_runtime / (1000.0 * 1000.0);
            double mbs = mb/s;

            BtrReader &reader = readers[column_i][0];
            double size_per_chunk = static_cast<double>(decompressed_sizes[column_i]) / static_cast<double>(file_metadata->num_chunks);
            std::cout << std::to_string(column_i)
                      << " " << ConvertTypeToString(reader.getColumnType())
                      << " " << reader.getBasicSchemeDescription(0)
                      << " " << compressed_sizes[column_i] << " Bytes"
                      << " " << decompressed_sizes[column_i] << " Bytes"
                      << " " << size_per_chunk << " Bytes"
                      << " " << average_runtime << " us"
                      << " " << mbs << " MB/s"
                      << std::endl;
        }
    }

    if (FLAGS_output_summary) {
        double average_runtime = static_cast<double>(total_runtime) / static_cast<double>(FLAGS_reps);
        double mb = static_cast<double>(total_size) / (1024.0 * 1024.0);
        double s = average_runtime / (1000.0 * 1000.0);
        double mbs = mb / s;

        std::cout << "Total:"
                  << " " << total_compressed_size << " Bytes"
                  << " " << total_size << " Bytes"
                  << " " << average_runtime << " us"
                  << " " << mbs << " MB/s"
                  << std::endl;
    }
}
// -------------------------------------------------------------------------------------
