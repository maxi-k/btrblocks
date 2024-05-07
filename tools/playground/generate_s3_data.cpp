// -------------------------------------------------------------------------------------
#include <algorithm>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>
// -------------------------------------------------------------------------------------
#include <tbb/parallel_for.h>
#define TBB_PREVIEW_GLOBAL_CONTROL 1
#include <tbb/global_control.h>
// -------------------------------------------------------------------------------------
#include <aws/core/Aws.h>
#include <aws/s3-crt/S3CrtClient.h>
#include <aws/s3-crt/model/AbortMultipartUploadRequest.h>
#include <aws/s3-crt/model/CompleteMultipartUploadRequest.h>
#include <aws/s3-crt/model/CreateMultipartUploadRequest.h>
#include <aws/s3-crt/model/ListPartsRequest.h>
#include <aws/s3-crt/model/PutObjectRequest.h>
#include <aws/s3-crt/model/UploadPartRequest.h>
// -------------------------------------------------------------------------------------
static const char* const bucket_prefix = "bucketprefix-s3-test-data";
static const size_t part_size = 128 * 1024 * 1024;
// -------------------------------------------------------------------------------------
static void usage(const char* program) {
  std::cerr << "Usage: " << program << " <number_of_objects> <object_size> <region>" << std::endl;
  exit(EXIT_FAILURE);
}
// -------------------------------------------------------------------------------------
static std::string get_key(long object_size, long object_idx) {
  std::stringstream key;
  key << object_size << "/" << object_idx;
  return key.str();
}
// -------------------------------------------------------------------------------------
static bool put_object(const Aws::S3Crt::S3CrtClient& s3_client,
                       const std::string& bucket,
                       std::shared_ptr<std::stringstream>& sstream,
                       long object_idx,
                       long object_size) {
  Aws::S3Crt::Model::PutObjectRequest put_request;

  put_request.SetBucket(bucket);
  put_request.SetKey(get_key(object_size, object_idx));
  put_request.SetBody(sstream);

  Aws::S3Crt::Model::PutObjectOutcome outcome = s3_client.PutObject(put_request);

  if (!outcome.IsSuccess()) {
    std::cerr << "Put request failed" << std::endl;
    return false;
  }
  return true;
}
// -------------------------------------------------------------------------------------
static void generate_data(std::shared_ptr<std::stringstream>& sstream, long object_size) {
  thread_local std::random_device rnd_device;
  // Specify the engine and distribution.
  thread_local std::mt19937 mersenne_engine{rnd_device()};  // Generates random integers
  thread_local std::uniform_int_distribution<int> dist{std::numeric_limits<char>::min(),
                                                       std::numeric_limits<char>::max()};

  while (object_size--) {
    char c = static_cast<char>(dist(mersenne_engine));
    sstream->put(c);
  }
}
// -------------------------------------------------------------------------------------
static std::pair<bool, std::string> upload_part(const Aws::S3Crt::S3CrtClient& s3_client,
                                                const std::string& bucket,
                                                std::string& key,
                                                std::string& upload_id,
                                                size_t part_number,
                                                std::shared_ptr<std::stringstream>& sstream) {
  Aws::S3Crt::Model::UploadPartRequest upload_request;
  upload_request.SetUploadId(upload_id);
  upload_request.SetBucket(bucket);
  upload_request.SetKey(key);
  upload_request.SetPartNumber(part_number);
  upload_request.SetBody(sstream);

  auto upload_outcome = s3_client.UploadPart(upload_request);
  return {upload_outcome.IsSuccess(), upload_outcome.GetResult().GetETag()};
}
// -------------------------------------------------------------------------------------
static void generate_and_upload_multipart(const Aws::S3Crt::S3CrtClient& s3_client,
                                          const std::string& bucket,
                                          long number_of_objects,
                                          long object_size) {
  tbb::parallel_for(long(0), number_of_objects, [&](long object_idx) {
    auto key = get_key(object_size, object_idx);

    /* Create Upload */
    Aws::S3Crt::Model::CreateMultipartUploadRequest create_upload_request;
    create_upload_request.SetBucket(bucket);
    create_upload_request.SetKey(key);
    auto create_upload_outcome = s3_client.CreateMultipartUpload(create_upload_request);
    if (!create_upload_outcome.IsSuccess()) {
      throw std::logic_error("Failed to create Mulitpart Upload");
    }
    auto upload_id = create_upload_outcome.GetResult().GetUploadId();

    /* Upload parts */
    size_t num_parts = (object_size + part_size - 1) / part_size;
    std::vector<Aws::S3Crt::Model::CompletedPart> completed_parts(num_parts);
    tbb::parallel_for(size_t(1), num_parts + 1, [&](size_t part_number) {
      auto sstream = std::make_shared<std::stringstream>();
      generate_data(sstream, std::min(part_size, static_cast<std::size_t>(object_size)));
      auto [success, etag] = upload_part(s3_client, bucket, key, upload_id, part_number, sstream);
      if (!success) {
        bool cleanup_done = false;
        while (!cleanup_done) {
          std::cerr << "Uploading part " << part_number << " for id " << upload_id
                    << " failed. Attempting abort.." << std::endl;
          Aws::S3Crt::Model::AbortMultipartUploadRequest abort_upload_request;
          abort_upload_request.SetUploadId(upload_id);
          abort_upload_request.SetBucket(bucket);
          abort_upload_request.SetKey(key);

          auto abort_upload_outcome = s3_client.AbortMultipartUpload(abort_upload_request);
          cleanup_done = abort_upload_outcome.IsSuccess();

          // List parts to make sure there is nothing left. The docs says we have to do this to
          // ensure proper cleanup.
          Aws::S3Crt::Model::ListPartsRequest list_parts_request;
          list_parts_request.SetUploadId(upload_id);
          list_parts_request.SetBucket(bucket);
          list_parts_request.SetKey(key);
          auto list_parts_outcome = s3_client.ListParts(list_parts_request);
          if (list_parts_outcome.IsSuccess()) {
            auto parts_left = list_parts_outcome.GetResult().GetParts().size();
            cleanup_done &= parts_left == 0;
          } else {
            cleanup_done = false;
          }
        }

        throw std::runtime_error("uploading part failed");
      }
      completed_parts[part_number - 1].SetPartNumber(part_number);
      completed_parts[part_number - 1].SetETag(etag);
    });

    /* Complete Upload */
    Aws::S3Crt::Model::CompletedMultipartUpload completed_upload;
    completed_upload.SetParts(completed_parts);

    Aws::S3Crt::Model::CompleteMultipartUploadRequest complete_upload_request;
    complete_upload_request.SetUploadId(upload_id);
    complete_upload_request.SetBucket(bucket);
    complete_upload_request.SetKey(key);
    complete_upload_request.SetMultipartUpload(completed_upload);
    auto complete_upload_outcome = s3_client.CompleteMultipartUpload(complete_upload_request);
    if (!complete_upload_outcome.IsSuccess()) {
      throw std::logic_error("Failed to complete Mulitpart Upload");
    }
  });
}
// -------------------------------------------------------------------------------------
static void generate_and_upload(const Aws::S3Crt::S3CrtClient& s3_client,
                                const std::string& bucket,
                                long number_of_objects,
                                long object_size) {
  tbb::parallel_for(long(0), number_of_objects, [&](long object_idx) {
    auto sstream = std::make_shared<std::stringstream>();
    generate_data(sstream, object_size);

    if (!put_object(s3_client, bucket, sstream, object_idx, object_size)) {
      throw std::runtime_error("Upload to S3 failed");
    }
  });
}
// -------------------------------------------------------------------------------------
int main(int argc, char** argv) {
  if (argc != 4) {
    usage(argv[0]);
  }

  long number_of_objects = std::stol(argv[1]);
  long object_size = std::stol(argv[2]);
  auto region = argv[3];
  std::stringstream bucket;
  bucket << bucket_prefix << "-" << region;

  //  tbb::global_control c(tbb::global_control::max_allowed_parallelism, 1);

  Aws::SDKOptions options;
  Aws::InitAPI(options);

  {
    Aws::S3Crt::ClientConfiguration config;
    config.partSize = part_size;
    config.throughputTargetGbps = 100.0;
    config.region = region;

    Aws::S3Crt::S3CrtClient s3_client(config);

//#define MULTIPART
#ifdef MULTIPART
    generate_and_upload_multipart(s3_client, bucket.str(), number_of_objects, object_size);
#else
    generate_and_upload(s3_client, bucket.str(), number_of_objects, object_size);
#endif
  }

  Aws::ShutdownAPI(options);
}
// -------------------------------------------------------------------------------------
