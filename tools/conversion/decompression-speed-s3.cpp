// -------------------------------------------------------------------------------------
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <functional>
#include <queue>
// -------------------------------------------------------------------------------------
#include <gflags/gflags.h>
// -------------------------------------------------------------------------------------
#include "common/Utils.hpp"
#include "compression/BtrReader.hpp"
#include "scheme/SchemePool.hpp"
// -------------------------------------------------------------------------------------
#include "s3-management.hpp"
// -------------------------------------------------------------------------------------
static std::atomic<uint64_t> total_decompressed_size = 0;
static std::atomic<uint64_t> chunk_count = 0;
static std::atomic<uint64_t> part_count = 0;
// -------------------------------------------------------------------------------------
DEFINE_string(region, "", "Region of the S3 bucket");
DEFINE_string(bucket, "", "Bucket that contains the btr data");
DEFINE_string(prefix, "", "Prefix within the bucket that contains btr data");
DEFINE_uint64(reps, 1, "Number of repetitions");
DEFINE_uint64(prealloc, 1024, "Number of preallocated buffers");
DEFINE_int32(threads, -1, "Limit threads of decompressor node");
DEFINE_string(columns, "all", "List of columns to decompress");
// -------------------------------------------------------------------------------------
using namespace btrblocks;
// -------------------------------------------------------------------------------------
std::vector<char> metadata_v;
// -------------------------------------------------------------------------------------
static inline const FileMetadata *read_metatdata(const s3_client_t &s3_client) {
    s3::Model::GetObjectRequest get_request;
    get_request.SetBucket(FLAGS_bucket);
    get_request.SetKey(FLAGS_prefix + "/metadata");
    
    auto outcome = s3_client.GetObject(get_request);
    if (!outcome.IsSuccess()) {
        throw std::runtime_error(outcome.GetError().GetMessage());
    }

    metadata_v.resize(outcome.GetResult().GetContentLength());
    outcome.GetResult().GetBody().read(metadata_v.data(), metadata_v.size());
    auto metadata = reinterpret_cast<const FileMetadata *>(metadata_v.data());

    /* This stream is intentionally not release */
    return metadata;
}
// -------------------------------------------------------------------------------------
static uint64_t decompressPart(long idx) {
    void *buffer = streambufarrays[idx].data();

    // Open Reader
    BtrReader reader(buffer);

    // Decompress content
    uint64_t local_decompressed_size = 0;
    for (u32 chunk_i = 0; chunk_i < reader.getChunkCount(); chunk_i++) {
        thread_local std::vector<u8> decompressed_data;
        reader.readColumn(decompressed_data, chunk_i);
        local_decompressed_size += reader.getDecompressedDataSize(chunk_i);
    }

    total_decompressed_size += local_decompressed_size;
    chunk_count += reader.getChunkCount();
    part_count += 1;

    s3_decompressPartFinish(idx);

    return local_decompressed_size;
}
// -------------------------------------------------------------------------------------
static inline void requestColumn(const s3_client_t& s3_client, const FileMetadata *file_metadata, u32 column) {
    for (u32 part_i = 0; part_i < file_metadata->parts[column].num_parts; part_i++) {
        std::stringstream key;
        key << FLAGS_prefix << "/" << "column" << column << "_part" << part_i;
        s3_requestFile(s3_client, key.str());
    }
}
// -------------------------------------------------------------------------------------
int main(int argc, char **argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    SchemePool::refresh();

    if (FLAGS_region.empty() || FLAGS_bucket.empty()) {
        std::cerr << "Region and Bucket are required" << std::endl;
        return 1;
    }

    if (FLAGS_prealloc == 0) {
        std::cerr << "Must prealloc at least 1 buffer" << std::endl;
        return 1;
    }

    // Prepare S3 API.
    Aws::SDKOptions options;
    Aws::InitAPI(options);
    {
        s3_client_t s3_client = s3_get_client(FLAGS_region);

        // Read the metadata
        const FileMetadata *file_metadata = read_metatdata(s3_client);

        std::vector<u32> columns;
        if (FLAGS_columns == "all") {
            columns.resize(file_metadata->num_columns);
            std::iota(columns.begin(), columns.end(), 0);
        } else {
            char column_string[FLAGS_columns.length()];
            strcpy(column_string, FLAGS_columns.c_str());
            char *i_str = strtok(column_string, ",");
            while (i_str != nullptr) {
                int i = std::stoi(i_str);
                if (i < 0 || i >= file_metadata->num_columns) {
                    std::cerr << "Selected column is out of range. " << i << " not in [0, " << file_metadata->num_columns-1 << "]" << std::endl;
                    return 1;
                }
                columns.push_back(i);
                i_str = strtok(nullptr, ",");
            }
            if (columns.empty()) {
                std::cerr << "columns invalid" << std::endl;
                exit(1);
            }
        }

        u32 total_parts = 0;
        for (u32 column : columns) {
            total_parts += file_metadata->parts[column].num_parts;
        }

        std::size_t threads = tbb::flow::unlimited;
        if (FLAGS_threads > 0) {
            threads = FLAGS_threads;
        }

        s3_init(FLAGS_reps * total_parts, FLAGS_prealloc, threads, FLAGS_bucket, decompressPart);

        auto t1 = std::chrono::high_resolution_clock::now();
        // Start all requests asynchronously
        for (uint64_t rep = 0; rep < FLAGS_reps; rep++) {
            for (u32 column : columns) {
                requestColumn(s3_client, file_metadata, column);
            }
        }

        s3_wait_for_end();
        auto t2 = std::chrono::high_resolution_clock::now();

        auto us = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
        double s = static_cast<double>(us.count()) / static_cast<double>(1e6);

        double total_downloaded_size_gib = static_cast<double>(total_downloaded_size) / static_cast<double>(1<<30);
        double total_downloaded_size_gigabits = static_cast<double>(total_downloaded_size * 8) / static_cast<double>(1<<30);
        double gbps = total_downloaded_size_gigabits / s;

        double total_decompressed_size_gib = static_cast<double>(total_decompressed_size) / static_cast<double>(1<<30);
        double total_decompressed_size_mib = static_cast<double>(total_decompressed_size) / static_cast<double>(1<<20);
        double mibs = total_decompressed_size_mib / s;

        // Add requests for metadata
        total_requests += FLAGS_reps;

        //std::cout << (total_parts * FLAGS_reps) << " " << part_count << " " << chunk_count_2 << " " << chunk_count << " " << total_downloaded_size << " " << total_decompressed_size << std::endl;

        std::cout << "Runtime[s]: " << s
            << " Downloaded[GiB]: " << total_downloaded_size_gib
            << " Bandwidth[Gbps]: " << gbps
            << " Decompressed[GiB]: " << total_decompressed_size_gib
            << " Decompression[MiB/s]: " << mibs
            << " Requests: " << total_requests
            << std::endl;

        s3_free_buffers();
    }
    Aws::ShutdownAPI(options);
}
// -------------------------------------------------------------------------------------
