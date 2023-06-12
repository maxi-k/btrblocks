#pragma once
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
#include <tbb/flow_graph.h>
#include <tbb/parallel_for.h>
#include <tbb/concurrent_unordered_map.h>
// -------------------------------------------------------------------------------------
#include <aws/core/Aws.h>
#include <aws/core/utils/memory/stl/AWSStreamFwd.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>
#include <aws/core/utils/StringUtils.h>
#include <aws/core/utils/memory/AWSMemory.h>
#include <aws/core/utils/memory/stl/AWSStreamFwd.h>
#include <aws/s3-crt/S3CrtClient.h>
#include <aws/s3-crt/model/GetObjectRequest.h>
// -------------------------------------------------------------------------------------
namespace s3 = Aws::S3Crt;
using s3_client_t = s3::S3CrtClient;
// -------------------------------------------------------------------------------------
static const char *allocation_tag = "btr";
std::string s3_bucket;
std::string s3_region;
// -------------------------------------------------------------------------------------
// These counters are here for debugging
static std::atomic<uint64_t> allocated = 0;
static std::atomic<uint64_t> releasedStreams = 0;
static std::atomic<uint64_t> releasedBuffers = 0;
// -------------------------------------------------------------------------------------
std::vector<s3::Model::GetObjectRequest> get_requests;
static long num_preallocated_buffers; // We may be able to tune this down.
static const long part_size = 16 * 1024 * 1024; // 16 MiB
static Aws::IOStreamFactory response_stream_factory;
static std::vector<std::vector<unsigned char>> streambufarrays;
static std::vector<unsigned long> streambuflens;
static std::vector<Aws::Utils::Stream::PreallocatedStreamBuf *> streambufs;
static std::mutex buffer_mutex;
static std::condition_variable buffer_cv;
static std::queue<long> buffers_available;
static tbb::concurrent_unordered_map<Aws::IOStream *, long> occupied_map;
// -------------------------------------------------------------------------------------
static std::atomic<uint64_t> total_downloaded_size = 0;
static std::atomic<uint64_t> total_requests = 0;
// -------------------------------------------------------------------------------------
std::mutex mutex;
std::condition_variable condition_variable;
long remaining_results;
std::function<void(const s3_client_t*, const s3::Model::GetObjectRequest& get_request, s3::Model::GetObjectOutcome outcome, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&)> callback;
size_t next_index = 0;
// -------------------------------------------------------------------------------------
tbb::flow::graph g;
size_t node_limit = 72;
tbb::flow::function_node<long, uint64_t> *decompressor = nullptr;
// -------------------------------------------------------------------------------------
static void s3_free_buffers() {
    for (auto ptr : streambufs) {
        Aws::Delete<Aws::Utils::Stream::PreallocatedStreamBuf>(ptr);
    }
}
// -------------------------------------------------------------------------------------
static void s3_prepare_buffers(long prealloc) {
    num_preallocated_buffers = prealloc;
    for (long i = 0; i < num_preallocated_buffers; i++) {
        streambufarrays.emplace_back(part_size*2);
        /* Touch every page to avoid faults later */
        for (long pos = 0; pos < streambufarrays[i].size(); pos++) {
            streambufarrays[i][pos] = 0;
        }
        streambufs.emplace_back(Aws::New<Aws::Utils::Stream::PreallocatedStreamBuf>(allocation_tag, streambufarrays[i].data(), static_cast<uint64_t>(streambufarrays[i].size())));
        streambuflens.push_back(0);
        // TODO we never call delete, but for now that's also not important
        buffers_available.push(i);
    }
    std::cerr << "Prepared " << num_preallocated_buffers << " buffers." << std::endl;
}
// -------------------------------------------------------------------------------------
inline Aws::IOStream *s3_getStream() {
    long idx;
    {
        std::unique_lock lock(buffer_mutex);
        buffer_cv.wait(lock, [&] {
            return !buffers_available.empty();
        });

        idx = buffers_available.front();
        buffers_available.pop();
    }
    // The SDK will call delete on the stream after the callback for getobect finishes
    Aws::IOStream *result = Aws::New<Aws::IOStream>(allocation_tag, streambufs[idx]);
    occupied_map[result] = idx;
    allocated++;
    return result;
}
// -------------------------------------------------------------------------------------
inline long s3_releaseStream(Aws::IOStream *stream) {
    releasedStreams++;
    return occupied_map[stream];
}
// -------------------------------------------------------------------------------------
inline void s3_releaseBuffer(long idx) {
    // No idea why, but we cannot reuse the PreallocatedStreamBufs. Even seeking back to 0 manually does not help.
    // If we don't recreate the object it will read at random positions on second use.
    // We cannot delete the of streambuf here. The deconstructor of the GetObjectRequestResult may still try to operate on it.
    // Effectively we leak memory here, but for the purpose of this benchmark this is the best we can do.
    streambufs[idx] = Aws::New<Aws::Utils::Stream::PreallocatedStreamBuf>(allocation_tag, streambufarrays[idx].data(), static_cast<uint64_t>(streambufarrays[idx].size()));
    {
        std::lock_guard guard(buffer_mutex);
        buffers_available.push(idx);
    }
    buffer_cv.notify_one();
    releasedBuffers++;
}
// -------------------------------------------------------------------------------------
void s3_GetObjectResponseReceiveHandler(
        const s3_client_t*,
        const s3::Model::GetObjectRequest&,
        s3::Model::GetObjectOutcome outcome,
        const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) {
    /*
     * Called once the request finishes.
     * There is only one instance of this function running at any time. Therefore we pass on the downloaded object to
     * a tbb construct that can actually process multiple of them in parallel.
     */
    if (!outcome.IsSuccess()) {
        throw std::runtime_error(outcome.GetError().GetMessage());
    }

    unsigned long length = outcome.GetResult().GetContentLength();
    total_downloaded_size += length;
    unsigned long num_request = (length + part_size - 1) / part_size;
    total_requests += num_request;

    // Insert pointer into node
    auto stream_ptr = &(outcome.GetResult().GetBody());
    long idx = s3_releaseStream(stream_ptr);
    streambuflens[idx] = length;
    decompressor->try_put(idx);
}
// -------------------------------------------------------------------------------------
inline void s3_requestFile(const s3_client_t &s3_client, const std::string &key) {
    auto &current_request = get_requests[next_index++];
    current_request.SetBucket(s3_bucket);
    current_request.SetKey(key);
    current_request.SetResponseStreamFactory(response_stream_factory);

    s3_client.GetObjectAsync(current_request, callback);
}
// -------------------------------------------------------------------------------------
inline void s3_decompressPartFinish(long idx) {
    s3_releaseBuffer(idx);
    long remaining;
    {
        std::lock_guard guard(mutex);
        remaining = --remaining_results;
    }

    if (remaining == 0) {
        condition_variable.notify_one();
    }
}
// -------------------------------------------------------------------------------------
inline void s3_wait_for_end() {
    // Wait until all requests are actually finished
    {
        std::unique_lock lock(mutex);
        condition_variable.wait(lock, []{return remaining_results == 0;});
    }
    // Just to make sure
    g.wait_for_all();
}
// -------------------------------------------------------------------------------------
inline void s3_init(std::size_t total_requests, long prealloc, std::size_t threads, std::string &bucket, uint64_t (*decompressPart)(long)) {
    s3_prepare_buffers(prealloc);

    callback = s3_GetObjectResponseReceiveHandler;
    response_stream_factory = s3_getStream;

    get_requests.resize(total_requests);
    remaining_results = get_requests.size();

    node_limit = threads;
    decompressor = new tbb::flow::function_node<long, uint64_t>(g, node_limit, decompressPart);

    s3_bucket = bucket;
}
// -------------------------------------------------------------------------------------
inline s3_client_t s3_get_client(std::string &region) {
    s3_region = region;

    s3::ClientConfiguration config;
    config.partSize = part_size;
    config.throughputTargetGbps = 100.0; // Throughput target for c5n.18xlarge
    config.region = s3_region;
    // Tested dual stack once, but did not see any improvement in performance.
    //config.useDualStack = true;
    config.scheme = Aws::Http::Scheme::HTTP;
    return s3_client_t(config);
}
// -------------------------------------------------------------------------------------
