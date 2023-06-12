#!/usr/bin/env sh

mkdir -p double-cols
bucket="s3://public-bi-benchmark/binary"

while read -r line; do
    aws s3 cp "$bucket/$line" "double-cols/$line"
done < ./pbi-double-columns.txt
