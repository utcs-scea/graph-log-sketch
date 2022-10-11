#!/usr/bin/awk -f
BEGIN {print "---------------------------------- Useful Stats -----------------------------------------"}
$1 !~ /^[0-9]/ {if(NUM) print cycles/NUM "cycles"}
$1 !~ /^[0-9]/ {if(NUM) print instructions/NUM "instructions"}
$1 !~ /^[0-9]/ {if(NUM) print l2_rd_miss/NUM "l2_rqsts.demand_data_rd_miss"}
$1 !~ /^[0-9]/ {if(NUM) print l2_rd_hit/NUM "l2_rqsts.demand_data_rd_hit"}
$1 !~ /^[0-9]/ {if(NUM) print l1d_ld_miss/NUM "L1-dcache-load-misses"}
$1 !~ /^[0-9]/ {if(NUM) print l1d_ld/NUM "L1-dcache-loads"}
$1 !~ /^[0-9]/ { NUM=$3; print $1 $3}
$2 == "cycles" {cycles+=$1}
$2 == "instructions" {instructions+=$1}
$2 == "ref-cycles" {ref_cycles+=$1}
$2 == "l2_rqsts.demand_data_rd_miss" {l2_rd_miss+=$1}
$2 == "l2_rqsts.demand_data_rd_hit" {l2_rd_hit+=$1}
$2 == "L1-dcache-load-misses" {l1d_ld_miss+=$1}
$2 == "L1-dcache-loads" {l1d_ld+=$1}
