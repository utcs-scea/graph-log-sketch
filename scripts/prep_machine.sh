#!/bin/bash

CPU=`nproc | awk '{print ($1 - 1)}'`

for i in `seq 0 $CPU`
do
  cpufreq-set -c $i -g userspace
  cpufreq-set -c $i -d 2.90GHz
  cpufreq-set -c $i -u 2.90GHz
  cpufreq-set -c $i -f 2.90GHz
done
