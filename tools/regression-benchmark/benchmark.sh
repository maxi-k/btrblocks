#!/bin/bash

# Function to read CSV file and sync URIs
sync_uris() {
  input_file="$1"
  output_file="$2"
  index=1

  while IFS=',' read -r name uri yaml; do
    schemaname=$(basename "$yaml")

    echo $uri

    if [[ ! -f "./csvtobtrdata/yaml/$name/$schemaname" ]]; then
      mkdir ./csvtobtrdata/yaml/$name -p
      aws s3 cp $yaml ./csvtobtrdata/yaml/$name/ --no-sign
    fi

    btr_dir="./csvtobtrdata/btrblocks/$name/"
    mkdir -p "$btr_dir" || rm -rf "${$btr_dir:?}"/*
    bin_dir="./csvtobtrdata/btrblocks_bin/$name/"
    echo "aws s3 sync $uri $bin_dir"
    if [[ ! -d $bin_dir ]]; then
      aws s3 sync $uri $bin_dir --no-sign
    fi

    yaml_file="./csvtobtrdata/yaml/$name/$schemaname"
    ./csvtobtr --btr $btr_dir --binary $bin_dir --create_btr true --yaml $yaml_file

    echo "$name, $(./decompression-speed --btr $btr_dir --reps 100 --binary $bin_dir --yaml $yaml_file --verify)" >> $output_file

    ((index++))

  done < "$input_file"
}

# install things
sudo apt-get install libssl-dev libcurl4-openssl-dev aws-cli -y

# build the benchmark thing
output_file="results.csv"
rm $output_file
mkdir tmpbuild
cd tmpbuild

dataset="../datasets.csv"
# Check if uris.csv exists
if [[ ! -f $dataset ]]; then
  echo "$dataset file not found."
  exit 1
fi

cmake ../../.. -DCMAKE_BUILD_TYPE=Release
make csvtobtr
make decompression-speed
sync_uris $dataset $output_file

