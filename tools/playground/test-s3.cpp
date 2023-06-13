// -------------------------------------------------------------------------------------
// Created by david on 21.04.22.
// -------------------------------------------------------------------------------------
#include <chrono>
#include <iostream>
// -------------------------------------------------------------------------------------
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
// -------------------------------------------------------------------------------------
bool GetObject(const Aws::String& objectKey,
               const Aws::String& fromBucket, const Aws::String& region) {
    Aws::Client::ClientConfiguration config;

    if (!region.empty())
    {
        config.region = region;
    }

    Aws::S3::S3Client s3_client(config);

    Aws::S3::Model::GetObjectRequest object_request;
    object_request.SetBucket(fromBucket);
    object_request.SetKey(objectKey);

    Aws::S3::Model::GetObjectOutcome get_object_outcome =
            s3_client.GetObject(object_request);

    if (get_object_outcome.IsSuccess())
    {
        auto& retrieved_file = get_object_outcome.GetResultWithOwnership().
                GetBody();

        // Print a beginning portion of the text file.
        std::cout << "Beginning of file contents:\n";
        char file_data[255] = { 0 };
        retrieved_file.getline(file_data, 254);
        std::cout << file_data << std::endl;

        return true;
    }
    else
    {
        auto err = get_object_outcome.GetError();
        std::cout << "Error: GetObject: " <<
                  err.GetExceptionName() << ": " << err.GetMessage() << std::endl;

        return false;
    }
}
// -------------------------------------------------------------------------------------
int main(int argc, char **argv) {
    Aws::SDKOptions options;
    Aws::InitAPI(options);
    {
        const Aws::String bucket_name = "bucketprefix-public-bi-benchmark-csv";

        //const Aws::String object_name = "aws-cpp-sdk-test.txt";
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
