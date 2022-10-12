#!/bin/bash

BENCH_COUNT=$3
BENCH_NUM=$2
FILE_NAME=$1

COUNTER_LIST=$(awk '$1 !~ /^[a-zA-Z]/ {print $2}' $1 | sort | uniq)
COUNTERS=$(awk '$1 !~ /^[a-zA-Z]/ {print $2}' $1 | sort | uniq | wc -l)

for i in `seq 1 $BENCH_NUM`;
do
  cat $1 | head -n $(( ( i - 1 ) * ( $BENCH_COUNT * $COUNTERS + 1 ) + 1 )) \
    |tail -n 1 | awk 'NF-=2'
  for j in $COUNTER_LIST
  do
    echo $j
    cat $1 | head -n $(( i * ( $BENCH_COUNT * $COUNTERS + 1 ) )) | tail -n $(( $BENCH_COUNT * $COUNTERS + 1 )) \
      | awk -v search="$j" '$2 == search {print $1}' |stats-exe | paste - - -
  done | paste - -
done
