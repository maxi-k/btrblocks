// -------------------------------------------------------------------------------------
// Created by david on 21.04.22.
// -------------------------------------------------------------------------------------
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>
// -------------------------------------------------------------------------------------
#include <aws/core/Aws.h>
#include <aws/core/utils/StringUtils.h>
#include <aws/core/utils/memory/AWSMemory.h>
#include <aws/core/utils/memory/stl/AWSStreamFwd.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>
// -------------------------------------------------------------------------------------
/* Uncomment for S3 Crt */
#define USES3CRT
#ifdef USES3CRT
#include <aws/s3-crt/S3CrtClient.h>
#include <aws/s3-crt/model/GetObjectRequest.h>
#include <unordered_set>
namespace s3 = Aws::S3Crt;
using s3_client_t = s3::S3CrtClient;
#else  // USES3CRT not defined
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
namespace s3 = Aws::S3;
using s3_client_t = s3::S3Client;
#endif  // USES3CRT
// -------------------------------------------------------------------------------------
std::mutex mutex;
std::condition_variable condition_variable;
long remaining_results;
std::vector<std::vector<s3::Model::GetObjectRequest>> get_requests;
std::function<void(const s3_client_t*,
                   const s3::Model::GetObjectRequest& get_request,
                   s3::Model::GetObjectOutcome outcome,
                   const std::shared_ptr<const Aws::Client::AsyncCallerContext>&)>
    callback;
size_t next_index = 0;
// -------------------------------------------------------------------------------------
long num_preallocated_buffers;
long part_size;
std::vector<std::vector<unsigned char>> streambufarrays;
std::vector<Aws::Utils::Stream::PreallocatedStreamBuf*> streambufs;
std::mutex buffer_mutex;
std::condition_variable buffer_cv;
std::unordered_set<long> buffers_available;
std::unordered_map<Aws::IOStream*, long> occupied_map;
static const char* allocation_tag = "test-s3-crt";
static std::atomic<uint64_t> total_downloaded_size = 0;
// -------------------------------------------------------------------------------------
static void free_buffers() {
  for (auto ptr : streambufs) {
    Aws::Delete<Aws::Utils::Stream::PreallocatedStreamBuf>(ptr);
  }
}
// -------------------------------------------------------------------------------------
static void prepare_buffers() {
  for (long i = 0; i < num_preallocated_buffers; i++) {
    streambufarrays.emplace_back(part_size);
    /* Touch every page to avoid faults later */
    for (long pos = 0; pos < part_size; pos++) {
      streambufarrays[i][pos] = 0;
    }
    streambufs.emplace_back(Aws::New<Aws::Utils::Stream::PreallocatedStreamBuf>(
        allocation_tag, streambufarrays[i].data(), static_cast<uint64_t>(part_size)));
    // TODO we never call delete, but for now that's also not important
    buffers_available.insert(i);
  }
}
// -------------------------------------------------------------------------------------
static Aws::IOStream* getStream() {
  Aws::IOStream* result;
  {
    std::unique_lock lock(buffer_mutex);
    buffer_cv.wait(lock, [&] { return !buffers_available.empty(); });

    long idx = *buffers_available.begin();
    buffers_available.erase(buffers_available.begin());
    result = Aws::New<Aws::IOStream>(allocation_tag, streambufs[idx]);
    // The SDK will call delete on the stream
    occupied_map[result] = idx;
  }
  return result;
}
Aws::IOStreamFactory response_stream_factory;
static void releaseStream(Aws::IOStream* stream) {
  {
    std::lock_guard guard(buffer_mutex);
    long idx = occupied_map[stream];
    occupied_map.erase(stream);
    streambufs[idx] = Aws::New<Aws::Utils::Stream::PreallocatedStreamBuf>(
        allocation_tag, streambufarrays[idx].data(),
        static_cast<uint64_t>(streambufarrays[idx].size()));
    buffers_available.insert(idx);
  }

  buffer_cv.notify_one();
}
// -------------------------------------------------------------------------------------
static void GetObjectResponseReceiveHandler(
    const s3_client_t*,
    const s3::Model::GetObjectRequest& get_request,
    s3::Model::GetObjectOutcome outcome,
    const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) {
  /* Called once the request finishes */
  if (!outcome.IsSuccess()) {
    throw std::runtime_error(outcome.GetError().GetMessage());
  }

  total_downloaded_size += outcome.GetResult().GetContentLength();
  // std::cerr << get_request.GetKey() << std::endl;
  long remaining;
  {
    std::lock_guard guard(mutex);
    remaining = --remaining_results;
  }

  if (remaining == 0) {
    condition_variable.notify_one();
  }

  releaseStream(&(outcome.GetResult().GetBody()));
}
// -------------------------------------------------------------------------------------
static void GetObjects(const s3_client_t& s3_client,
                       const Aws::String& fromBucket,
                       long object_count,
                       long object_size) {
  for (long current_object = 0; current_object < object_count; current_object++) {
    std::vector<s3::Model::GetObjectRequest>& current_request_vector = get_requests[next_index++];

    long num_requests = (object_size + part_size - 1) / part_size;
    current_request_vector.resize(num_requests);

    // Make request in chunks of part_size. Otherwise, we would try to store the whole file in
    // memory.
    long current_offset = 0;
    for (long request_idx = 0; request_idx < num_requests; request_idx++) {
      {
        std::lock_guard guard(mutex);
        remaining_results++;
      }
      // s3::Model::GetObjectRequest &current_request = current_request_vector[request_idx];
      static s3::Model::GetObjectRequest current_request;
      current_request.SetBucket(fromBucket);
      std::stringstream key;
      key << object_size << "/" << current_object;
      current_request.SetKey(key.str());

      // The range is inclusive
      std::stringstream range;
      long last_byte = std::min(current_offset + part_size - 1, object_size - 1);
      range << "bytes=" << current_offset << "-" << last_byte;
      current_request.SetRange(range.str());

      current_request.SetResponseStreamFactory(response_stream_factory);

      s3_client.GetObjectAsync(current_request, callback);
      current_offset = last_byte + 1;
    }

    if (current_offset != object_size) {
      throw std::logic_error("Invalid offset value after last request");
    }
  }
}
// -------------------------------------------------------------------------------------
static void usage(const char* program) {
  std::cerr << "Usage: " << program
            << " region bucket object_count object_size part_size num_pre_allocate repetitions "
               "target_throughput"
            << std::endl;
  exit(EXIT_FAILURE);
}
// -------------------------------------------------------------------------------------
int main(int argc, char** argv) {
  if (argc != 9) {
    usage(argv[0]);
  }

  Aws::SDKOptions options;
  Aws::InitAPI(options);
  {
    const Aws::String bucket_name = argv[2];

    long object_count = std::stol(argv[3]);
    if (object_count < 1) {
      usage(argv[0]);
    }
    long object_size = std::stol(argv[4]);
    if (object_size < 1) {
      usage(argv[0]);
    }

    part_size = std::stol(argv[5]);
    if (part_size < 1) {
      usage(argv[0]);
    }

    num_preallocated_buffers = std::stol(argv[6]);
    if (num_preallocated_buffers < 1) {
      usage(argv[0]);
    }

    long repetitions = std::stol(argv[7]);
    if (repetitions < 1) {
      usage(argv[0]);
    }

    double target_throughput = std::stod(argv[8]);
    if (target_throughput <= 0.0) {
      usage(argv[0]);
    }

    get_requests.resize(object_count * repetitions);
    next_index = 0;
    callback = GetObjectResponseReceiveHandler;

    const Aws::String region = argv[1];

#ifdef USES3CRT
    s3::ClientConfiguration config;
    // config.partSize = 16 * 1024 * 1024;
    config.partSize = part_size;
    config.throughputTargetGbps = target_throughput;
    config.region = region;
    // TODO maybe experiment with this one
    // config.useDualStack = true;
    config.scheme = Aws::Http::Scheme::HTTP;
    s3_client_t s3_client(config);
#else
    s3_client_t s3_client;
#endif

    prepare_buffers();
    response_stream_factory = getStream;

    auto t1 = std::chrono::high_resolution_clock::now();
    // Start all requests asynchronously
    for (long rep = 0; rep < repetitions; rep++) {
      GetObjects(s3_client, bucket_name, object_count, object_size);
    }
    // Wait until all requests are actually finished
    {
      std::unique_lock lock(mutex);
      condition_variable.wait(lock, [] { return remaining_results == 0; });
    }
    auto t2 = std::chrono::high_resolution_clock::now();

    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
    double s = static_cast<double>(us.count()) / static_cast<double>(1e6);
    std::size_t total_size_bytes =
        static_cast<size_t>(object_count) * static_cast<size_t>(object_size);
    double total_size_gigabits =
        static_cast<double>(total_size_bytes * 8) / static_cast<double>(1 << 30);
    double gbps = (total_size_gigabits / s) * static_cast<double>(repetitions);
    std::cout << "total_size_bytes = " << total_size_bytes
              << " total_downloaded_size = " << total_downloaded_size << std::endl;
    std::cout << "Speed: " << gbps << " Gbps" << std::endl;

    free_buffers();
  }
  Aws::ShutdownAPI(options);
}
// -------------------------------------------------------------------------------------
