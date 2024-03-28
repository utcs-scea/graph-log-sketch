#!/bin/bash

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

CPU=$(nproc | awk '{print ($1 - 1)}')

for i in $(seq 0 $CPU); do
	cpufreq-set -c $i -g userspace
	cpufreq-set -c $i -d 2.90GHz
	cpufreq-set -c $i -u 2.90GHz
	cpufreq-set -c $i -f 2.90GHz
done
