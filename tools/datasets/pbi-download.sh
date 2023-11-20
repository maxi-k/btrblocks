#!/usr/bin/env sh

set -e

defaulturl="https://homepages.cwi.nl/~boncz/PublicBIbenchmark/"

if [ "$1" = "help" ]; then
    echo "Usage: $0 [url='$defaulturl']"
    echo "  url: url to download pbi data from (include trailing slash!)"
    echo ""
    echo "  Downloads the raw (csv) Public BI Benchmark data and decompresses it."
    exit 0
fi

url=${1:-"$defaulturl"}
dldir="bz2"
csvdir="csv"
btrdir="btr"

echo "Warning: This will use ~400GB of disk space and take a long time to decompress."
read -p "Continue? [y/N] " -r user_confirmation
test "$user_confirmation" = "y" || exit 1

if [ -d "$dldir" ]; then
  echo "Directory $dldir already exists, skipping download"
else
  depth=$(echo $url | awk -F/ '{print NF-4}')
  echo "Downloading pbi data to $dldir from $url with directory depth $depth."
  mkdir -p pbi
  wget -r -nH -P"$dldir" --no-parent --cut-dirs=$depth --reject="index.html*" $url
fi

echo "Decompressing files."

compr="bunzip2"
command -v $compr >/dev/null 2>&1 || { echo >&2 "$compr not found, please install it"; exit 1; }

mkdir -p $csvdir

# decompress files
for dataset in $(ls $dldir); do
    echo "decompressing $dataset to $csvdir"
    mkdir -p $csvdir/$dataset
    for file in $(ls $dldir/$dataset); do
        outname=$(echo $file | sed 's/\.bz2$//')
        echo "|- $file > $csvdir/$dataset/$outname"
        $compr -k -c $dldir/$dataset/$file > $csvdir/$dataset/$outname
    done
done
