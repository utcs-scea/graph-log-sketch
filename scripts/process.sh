#!/bin/bash

BENCH_COUNT=$4
BENCH_NUM=$3
COUNTERS=$2
FILE_NAME=$1



for i in `seq 1 $3`;
do
  cat $1 | head -n $(( ( i - 1 ) * ( $BENCH_COUNT * $COUNTERS + 1 ) + 1 )) \
    |tail -n 1 | awk 'NF-=2'
  for j in "cycles" "instructions" "ref-cycles" "l2_rqsts.demand_data_rd_miss" "l2_rqsts.demand_data_rd_hit" "L1-dcache-load-misses" "L1-dcache-loads";
  do
    echo $j
    cat $1 | head -n $(( i * ( $BENCH_COUNT * $COUNTERS + 1 ) )) | tail -n $(( $BENCH_COUNT * $COUNTERS + 1 )) \
      | awk -v search="$j" '$2 == search {print $1}' |stats-exe | paste - - -
  done | paste - -
done
