#!/usr/bin/env sh

set -e

usage() {
    echo "$0 [csvdir(input)='csv'] [metadatadir(input)='metadata'] [btrdir(compressed output)='btr'] [binarydir(uncompressed output)='binary']"
    echo ""
    echo " converts the csv files in csvdir to btr files in btrdir. requires metadata using the yaml file format."
    echo " you can use pbi-download.sh to download the data and pbi-download-metadata.sh to download metadata."
}

if [ "$1" = "help" ]; then
    usage
    exit 0;
fi

csvdir=${1:-"csv"}
metadir=${2:-"metadata"}
btrdir=${3:-"btr"}
binarydir=${3:-"binary"}
cpus=${4:-"$(lscpu | grep '^CPU(s):' | awk '{ print $2 }')"}

test -x "csvtobtr" || ( echo "binary csvtobtr not found, please compile it first" && exit 1 )
test -e "$metadir" || ( echo "metadata directory $metadir not found" && exit 1 )
test -e "$csvdir" ||  ( echo "csv directory $csvdir not found" && exit 1 )

mkdir -p $btrdir

CURRENT_JOB_COUNT=0
MAX_JOB_COUNT=$((cpus/2))
MAX_THREAD_COUNT=$((cpus/2))

echo "using max of $MAX_THREAD_COUNT threads per conversion and $MAX_JOB_COUNT parallel conversions"
for dataset in $(ls $csvdir); do
    for fullfile in $(ls $csvdir/$dataset/*.csv); do
        # parallelism
        CURRENT_JOB_COUNT=$(jobs | wc -l)
        while [ $CURRENT_JOB_COUNT -ge $MAX_JOB_COUNT ]; do
            echo "Pool is full. waiting for jobs to exit..."
            sleep 1

            # The above "echo" and "sleep" is for demo purposes only.
            # In a real usecase, remove those two and keep only the following line
            # It will drastically increase the performance of the script
            CURRENT_JOB_COUNT=$(jobs | wc -l)
        done

        file=$(basename $fullfile)
        echo "converting $fullfile to btr"
        basename=$(echo $file | sed 's/\.csv$//')
        dataset_nr=$(echo $basename | sed 's/.*_//')
        # only process datasets with nr 1, otherwise data is too skewed
        # see reasoning in paper
        if [ "$dataset_nr" != "1" ]; then
            continue
        fi
        yamlname="$basename.yaml"
        # try to find yaml file in e.g. Arade/Arade_1.yaml or Arade/1/Arade_1.yaml
        if [ -f "$metadir/$dataset/$dataset_nr/$yamlname" ]; then
            yamlfile="$metadir/$dataset/$dataset_nr/$yamlname"
        elif [ -f "$metadir/$dataset/$yamlname" ]; then
            yamlfile="$metadir/$dataset/$yamlname"
        else
            echo "yaml file $yamlname not found in $metadir/$dataset/$yamlname or $metadir/$dataset/$dataset_nr/$yamlname, skipping"
            continue
        fi
        # create dir and convert
        mkdir -p $btrdir/$basename
        mkdir -p $binarydir/$basename
        cp $yamlfile $binarydir/$basename
        # start in background
        ./csvtobtr \
            -create_binary -create_btr \
            -yaml "$yamlfile" \
            -csv "$csvdir/$dataset/$file" \
            -btr "$btrdir/$basename" \
            -binary "$binarydir/$basename/" \
            -threads $MAX_THREAD_COUNT &
    done
done

wait
