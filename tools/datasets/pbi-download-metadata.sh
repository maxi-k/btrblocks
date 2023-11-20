#!/usr/bin/env bash

set -e

if [ "$1" = "help" ]; then
    echo "Usage: $0 [bucket='s3://public-bi-benchmark'] [s3basepath='binary/']"
    echo ""
    echo "  Downloads the metadata (yaml schema files, tuple counts)"
    echo "  for the Public BI Benchmark data from the given bucket."
    exit 0;
fi

bucket=${1:-"s3://public-bi-benchmark"}
s3base=${2:-"binary/"}

filelist=/tmp/pbi-filelist
outdir=metadata

s3path="$bucket/$s3base"
test -f $filelist || aws s3 ls --recursive "$s3path" > "$filelist"
echo "using filelist $filelist"

# download tuple counts
## mkdir -p $outdir
## grep 'tuple_count' "$filelist" | awk '{print $4}' |  while read -r line; do
##   basedir=$( echo $line | sed 's/tuple_count//' )
##   localpath=$( echo "$basedir" | sed "s|$s3base||")
##   mkdir -p "$outdir/$basedir"
##   aws s3 cp "$bucket/$line" "$outdir/$localpath/tuple_count"
## done

# download yaml files
grep '.yaml$' "$filelist" | awk '{print $4}' | while read -r line; do
  localpath=$( echo $line | sed "s|$s3base||" )
  mkdir -p "$outdir/$basedir"
  aws s3 cp "$bucket/$line" "$outdir/$localpath"
done
