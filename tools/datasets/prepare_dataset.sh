#!/usr/bin/env bash
set -e

function processYaml() {
    sed -i -E 's/"(.+)" (varchar.*|text.*|string.*)/- name: \1 \n    type: string/g' $1
    sed -i -E 's/"(.+)" (decimal.*|float.*|double.*)/- name: \1 \n    type: double/g' $1
    sed -i -E 's/"(.+)" (integer.*|smallint.*)/- name: \1 \n    type: integer/g' $1
    sed -i -E 's/"(.+)" (bigint.*)/- name: \1 \n    type: bigint/g' $1
    # remove all other types of columns
    sed -i -E 's/"(.+)" (time.*|date.*|timestamp.*|bool.*)/- name: \1 \n    type: skip/g' $1
    sed -i -E 's/CREATE TABLE.*/columns:/g' $1
    sed -i -E 's/\);//g' $1
    # replace / in column names with _
    sed -i -E 's/\//_/g' $1
    sed -i -E 's/name: ([^:]*): +(.*)/name: \1_\2/g' $1
}

if [[ ! -d public_bi_benchmark ]]; then
   git clone --depth=1 https://github.com/cwida/public_bi_benchmark.git
   rm -rf public_bi_benchmark/.git*
fi

cd public_bi_benchmark/benchmark
base_dir=$(pwd)
for file in *; do
   if [[ -d $file ]]; then
      for variant in $file/tables/*; do
         variant_name=$(echo $variant | sed -E 's/[^_]*tables\/(\w+_[0-9]+).*/\1/g')
         variant_no=$(echo $variant_name | sed -E 's/\w+_([0-9]+).*/\1/g')
         mkdir -p ./../../formatted/$file/$variant_no
         cp $variant ./../../formatted/$file/$variant_no/$variant_name.yaml
         processYaml ./../../formatted/$file/$variant_no/$variant_name.yaml

#         csv_bzip=$(sed "${variant_no}q;d" $file/data-urls.txt) , lines are not correctly sorted
         csv_bzip="http://www.cwi.nl/~boncz/PublicBIbenchmark/${file}/${variant_name}.csv.bz2"

         # first argument must be given to download
         tagret_dir="./../../formatted/$file/$variant_no/"

         cd $tagret_dir
         if [[ $DOWNLOAD ]] && [[ ! -e ${tagret_dir}/${variant_name}.csv ]]; then
            # ${variant_name}.csv
            (wget $csv_bzip; bzip2 -d *.bz2*; rm *.bz2* ; sed -n '$=' *.csv > tuple_count)
         fi

         if [[ $PARSE ]]; then
            if [[ -f parser_ignore ]] || ( [[ ${variant_name} < $START ]] ) || ( [[ ${variant_name} > $END ]] ); then
               echo "ignoring "${variant_name}
               cd $base_dir
               continue
            fi

            if [[ $PARSER ]]; then PARSER_EXEC=$PARSER; else PARSER_EXEC=harbook; fi
            echo "parsing "${variant_name}

            if [[ $DELETE ]] && [[ -d $variant_name ]]; then
               echo "delete old parsed files in directiory "${variant_name}
               rm -rf $variant_name
            fi

            if [[ $MOVE_CSV_BEFORE ]]; then
               mv $MOVE_CSV/$variant_name".csv" . || echo "could not (before) move csv"
            fi

            $PARSER_EXEC --only_parse=1 --parse=1 --yaml=$variant_name

            if [[ $MOVE_CSV ]]; then
               mv $variant_name".csv" $MOVE_CSV
            fi
         fi
         cd $base_dir
      done

   fi
done