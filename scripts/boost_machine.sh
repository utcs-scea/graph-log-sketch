#!/bin/bash

CPU=`nproc | awk '{print ($1 - 1)}'`

for i in `seq 0 $CPU`
do
  cpufreq-set -c $i -g performance
done
