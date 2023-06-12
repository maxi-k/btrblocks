// -------------------------------------------------------------------------------------
// Created by david on 24.04.22.
// -------------------------------------------------------------------------------------
#include <chrono>
#include <iostream>
#include <thread>
// -------------------------------------------------------------------------------------
#include <aws/core/Aws.h>
#include <aws/core/utils/StringUtils.h>
#include <aws/core/utils/memory/AWSMemory.h>
#include <aws/core/utils/memory/stl/AWSStreamFwd.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>
#include <aws/core/utils/threading/Executor.h>
#include <aws/s3/S3Client.h>
#include <aws/transfer/TransferHandle.h>
#include <aws/transfer/TransferManager.h>
// -------------------------------------------------------------------------------------
static const size_t BUFFER_SIZE = 512 * 1024 * 1024;  // 512MB Buffer
// -------------------------------------------------------------------------------------
bool GetObject(const Aws::String& objectKey,
               const Aws::String& fromBucket,
               const Aws::String& region) {
  auto s3_client = Aws::MakeShared<Aws::S3::S3Client>("S3Client");
  auto executor = Aws::MakeShared<Aws::Utils::Threading::PooledThreadExecutor>("executor", 25);
  Aws::Transfer::TransferManagerConfiguration transfer_config(executor.get());
  transfer_config.s3Client = s3_client;

  auto transfer_manager = Aws::Transfer::TransferManager::Create(transfer_config);

  Aws::Utils::Array<unsigned char> buffer(BUFFER_SIZE);
  auto stream =
      Aws::New<Aws::IOStream>("TestTag", Aws::New<Aws::Utils::Stream::PreallocatedStreamBuf>(
                                             "TestTag", buffer.GetUnderlyingData(), BUFFER_SIZE));
  auto downloadHandle =
      transfer_manager->DownloadFile(fromBucket, objectKey,
                                     [&]() {  // Define a lambda expression for the callback method
                                              // parameter to stream back the data.
                                       return stream;
                                     });

  // Try to read partial data from stream
  for (int i = 0; i < 100; i++) {
    std::cout << "Round " << i << std::endl;
    std::cout << downloadHandle->GetStatus() << std::endl;

    auto available_bytes = stream->tellp();
    std::cout << available_bytes << std::endl;
    Aws::Utils::Array<char> read_buffer(available_bytes);
    stream->read(read_buffer.GetUnderlyingData(), available_bytes);
    assert(stream->gcount() == available_bytes);

    std::string result(read_buffer.GetUnderlyingData(),
                       read_buffer.GetUnderlyingData() + stream->gcount());
    std::cout << result << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  std::cout << "Waiting until download finishes." << std::endl;
  downloadHandle->WaitUntilFinished();  // Block calling thread until download is complete.
  auto downStat = downloadHandle->GetStatus();
  if (downStat != Aws::Transfer::TransferStatus::COMPLETED) {
    auto err = downloadHandle->GetLastError();
    std::cout << "File download failed:  " << err.GetMessage() << std::endl;
    return false;
  }
  // std::cout << "File download to memory finished."  << std::endl;
  return true;
}
// -------------------------------------------------------------------------------------
int main(int argc, char** argv) {
  Aws::SDKOptions options;
  Aws::InitAPI(options);
  {
    const Aws::String bucket_name = "bucketprefix-public-bi-benchmark-csv";

    // const Aws::String object_name = "aws-cpp-sdk-test.txt";
    const Aws::String object_name = "generico1/Generico_1.csv";

    const Aws::String region = "us-east-1";

    auto t1 = std::chrono::high_resolution_clock::now();
    bool result = GetObject(object_name, bucket_name, region);
    auto t2 = std::chrono::high_resolution_clock::now();

    auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
    std::cout << "Execution took " << ms_int.count() << std::endl;

    if (!result) {
      return 1;
    }
  }
  Aws::ShutdownAPI(options);
}
// -------------------------------------------------------------------------------------
