#include <aws/core/Aws.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>
#include <aws/core/utils/threading/Executor.h>
#include <aws/s3/S3Client.h>
#include <aws/transfer/TransferManager.h>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace std;
using namespace Aws;
using namespace Aws::Utils;

const string out_dir_name = "bench-dataset";

vector<string> IntegerBenchmarkDatasets{"binary/Bimbo/1/Bimbo_1/10_Semana.integer",
                                        "binary/Bimbo/1/Bimbo_1/12_Venta_uni_hoy.integer",
                                        "binary/Bimbo/1/Bimbo_1/1_Agencia_ID.integer",
                                        "binary/Bimbo/1/Bimbo_1/2_Canal_ID.integer",
                                        "binary/Bimbo/1/Bimbo_1/3_Cliente_ID.integer",
                                        "binary/Bimbo/1/Bimbo_1/4_Demanda_uni_equil.integer",
                                        "binary/Bimbo/1/Bimbo_1/6_Dev_uni_proxima.integer",
                                        "binary/Bimbo/1/Bimbo_1/8_Producto_ID.integer",
                                        "binary/Bimbo/1/Bimbo_1/9_Ruta_SAK.integer"};

vector<string> DoubleBenchmarkDatasets{
    "binary/Arade/1/Arade_1/4_F4.double",         "binary/Arade/1/Arade_1/5_F5.double",
    "binary/Arade/1/Arade_1/8_F8.double",         "binary/Arade/1/Arade_1/9_F9.double",
    "binary/Bimbo/1/Bimbo_1/11_Venta_hoy.double", "binary/Bimbo/1/Bimbo_1/5_Dev_proxima.double"};

vector<string> StringBenchmarkDatasets{
    "binary/Arade/1/Arade_1/1_F1.string", "binary/Arade/1/Arade_1/2_F2.string",
    "binary/Arade/1/Arade_1/6_F6.string", "binary/Arade/1/Arade_1/7_F7.string"};

vector<vector<string>> Datasets = {IntegerBenchmarkDatasets, DoubleBenchmarkDatasets,
                                   StringBenchmarkDatasets};

void DownloadBenchmarkDataset(const Aws::String& bucket_name,
                              const Aws::String& objectKey,
                              const Aws::String& local_path,
                              const shared_ptr<Aws::Transfer::TransferManager>& transfer_manager) {
  auto downloadHandle = transfer_manager->DownloadFile(bucket_name, objectKey, [&local_path]() {
    auto* stream = Aws::New<Aws::FStream>("s3file", local_path, std::ios_base::out);
    stream->rdbuf()->pubsetbuf(nullptr, 0);

    return stream;
  });

  downloadHandle->WaitUntilFinished();
  auto downStat = downloadHandle->GetStatus();
  if (downStat != Transfer::TransferStatus::COMPLETED) {
    auto err = downloadHandle->GetLastError();
    std::cout << "\nFile download failed:  " << err.GetMessage() << '\n';
  }
  std::cout << "\nFile download to " << local_path << " finished." << '\n';
}

bool FileExists(string path) {
  ifstream f;
  try {
    f.open(path);
    return f.good();
  } catch (int _) {
    return false;
  }
}

int main(int argc, char** argv) {
  std::filesystem::create_directory(out_dir_name);

  Aws::SDKOptions options;
  Aws::InitAPI(options);
  {
    const Aws::String bucket_name = "public-bi-benchmark";
    const Aws::String region = "eu-central-1";

    Aws::Client::ClientConfiguration config;
    config.httpRequestTimeoutMs = 300000;
    config.requestTimeoutMs = 300000;
    config.connectTimeoutMs = 300000;

    if (!region.empty()) {
      config.region = region;
    }

    // snippet-start:[transfer-manager.cpp.transferOnStream.code]
    auto s3_client = std::make_shared<Aws::S3::S3Client>(config);
    auto executor = Aws::MakeShared<Aws::Utils::Threading::PooledThreadExecutor>("executor", 8);

    Aws::Transfer::TransferManagerConfiguration transfer_config(executor.get());
    transfer_config.s3Client = s3_client;

    transfer_config.downloadProgressCallback =
        [](const Aws::Transfer::TransferManager*,
           const std::shared_ptr<const Aws::Transfer::TransferHandle>& handle) {
          std::cout << "\r"
                    << "Benchmark Download Progress for " << handle->GetKey() << ": "
                    << handle->GetBytesTransferred() << " of " << handle->GetBytesTotalSize()
                    << " bytes" << std::flush;
        };

    auto transfer_manager = Aws::Transfer::TransferManager::Create(transfer_config);

    auto t1 = std::chrono::high_resolution_clock::now();

    for (auto& dataset : Datasets) {
      for (auto& objectKey : dataset) {
        auto bitMapKey = objectKey.substr(0, objectKey.find_last_of('.')) + ".bitmap";

        auto objectFileName = objectKey.substr(objectKey.find_last_of('/') + 1);

        auto localPath = string("./").append(out_dir_name).append("/").append(objectFileName);

        if (FileExists(localPath)) {
          cout << "File: " << localPath << " already exists \n";
          continue;
        }

        DownloadBenchmarkDataset(bucket_name, objectKey, localPath, transfer_manager);
        DownloadBenchmarkDataset(bucket_name, bitMapKey, localPath, transfer_manager);
      }
    }

    auto t2 = std::chrono::high_resolution_clock::now();

    auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
    std::cout << "Download took " << ms_int.count() << "\n";
  }
  Aws::ShutdownAPI(options);

  return 0;
}  // namespace btrbench