###
### Connects to the sever given by -s via ssh, clones the btrblocks repo and builds and executes the benchmark.
### Gets the resulting json file and computes average compression ratio and ratio weighted by column size.
###

HELP="Specify server via -s flag"

TMP_FOLDER="tmp"
TMP_FILE="./${TMP_FOLDER}/result.json"

while getopts "hs:" flag; do
 case $flag in
	h) 
	echo "${HELP}"
	;;
	s) 
	SERVER=$OPTARG
	;;
	\?)
	echo "Unknown flag"
	;;
 esac
done

if ! [ -z "$SERVER"  ]
then
ssh $SERVER << EOF
	
	# Prep directories

	mkdir -p btr_bench
	cd btr_bench

	mkdir -p bench_results

	# Clone git repo if necessary
	
	if ! [ -d "btrblocks" ] 
	then	
		git clone https://github.com/seb711/btrblocks.git
	fi

	cd btrblocks
	
	# Get newest version and build 

	git pull

	mkdir -p build_release
	cd build_release
	cmake -DCMAKE_BUILD_TYPE=Release ..

	make bench_dataset_downloader

	./bench_dataset_downloader

	make benchmarks

	# Execute benchmarks

	./benchmarks --benchmark_out="./../../bench_results/result.json" --benchmark_out_format=json
	
	# Download result and exit	

	exit
EOF

if ! [ -d "${TMP_FOLDER}" ] 
then
	mkdir ${TMP_FOLDER}
fi

scp "${SERVER}:~/btr_bench/bench_results/result.json" ${TMP_FILE}

COMP_RATIO_AVG=$(jq '.benchmarks | map( .comp_ratio_avg ) | add/length' ${TMP_FILE})

UNCOMPRESSED_SIZE=$(jq '.benchmarks | map( .uncompressed_data_size ) | add' ${TMP_FILE})
COMPRESSED_SIZE=$(jq '.benchmarks | map ( .compressed_data_size ) | add' ${TMP_FILE})

WEIGHTED_COMP_RATIO_AVG=$(bc <<< "${UNCOMPRESSED_SIZE} * 1.00/ ${COMPRESSED_SIZE}")

touch bench_result.json

jq '{ benchmarks: .benchmarks, weighted_comp_ratio_avg: "'$WEIGHTED_COMP_RATIO_AVG'", compression_ratio_average: "'$COMP_RATIO_AVG'" }' ${TMP_FILE} > bench_result.json

rm ${TMP_FILE}

echo "${HELP}"
fi
