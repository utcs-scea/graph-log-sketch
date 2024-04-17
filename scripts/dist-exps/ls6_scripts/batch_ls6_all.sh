# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#EXECS=( "louvain-clustering-cli-dist" )
#EXECS=( "maximal-independent-sets-dist" )
EXECS=("connected-components-cli-dist")
#EXECS=( "minimum-spanning-forest-cli-dist" )
#EXECS=( "maximal-independent-sets-dist" "minimum-spanning-forest-cli-dist" )

# Format: list("#-of-hosts,time")

#SET="1,00:15:00 2,00:15:00 4,00:15:00 8,00:15:00 16,00:15:00 32,00:15:00"
#SET="16,00:50:00 32,00:50:00"
#SET="16,01:30:00 32,01:30:00"
#SET="4,00:40:00 8,00:40:00"
#SET="128,01:00:00"
#SET="256,00:30:00"
#SETc="100,01:30:00 "
#SETc="64,02:00:00 128,02:00:00 "
#SET="16,00:50:00 32,00:50:00"
#SET="64,01:00:00 128,01:00:00"
#SET="36,01:30:00"
#SET="32,01:00:00 128,01:00:00"
#SET="32,00:40:00 128,00:40:00 64,00:40:00 "
#SET="256,00:45:00 "
#SET="64,01:30:00"
#SET="2,00:30:00"
#SET="128,00:40:00"
SET="4,03:00:00 8,03:00:00 16,03:00:00 32,03:00:00"
#SET="16,00:30:00"
#SET="4,01:30:00"
#SET="4,02:30:00 8,02:30:00 16,02:30:00"
#SET="4,00:40:00 8,00:40:00 16,00:40:00 32,00:40:00"
#SET="16,05:00:00"
#SET=" 16,02:00:00 32,02:00:00"
#SET="4,01:00:00 8,01:00:00"
#SET="4,01:00:00 8,01:00:00 16,01:00:00 32,01:00:00 64,01:00:00"
#SET="4,01:00:00 32,01:00:00"
#SET="8,01:00:00"
#SET="32,02:00:00"
#SET="32,01:00:00"
#SET="16,01:10:00 32,01:10:00"

# Format: (input-graph;\"${SET}\"")

#INPUTS=( "road-USA;\"${SET}\"" "wdc12;\"${SETc}\"" "clueweb12;\"${SETc}\"" )
#INPUTS=( "road-USA;\"${SET}\"" "friendster;\"${SET}\"" )
#INPUTS=( "road-USA;\"${SET}\"" )
#INPUTS=( "friendster;\"${SET}\"" "twitter40;\"${SET}\"" )
#INPUTS=( "friendster;\"${SET}\"" )
#INPUTS=( "twitter40;\"${SET}\"" )
#INPUTS=( "friendster;\"${SET}\"" "clueweb12;\"${SETc}\"" )
#INPUTS=( "clueweb12;\"${SET}\"" "wdc;\"${SET}\"" )#INPUTS=( "clueweb12;\"${SET}\"" )#INPUTS=( "clueweb12;\"${SET}\"" )
#INPUTS=( "clueweb12;\"${SET}\"" )
#INPUTS=( "wdc12;\"${SET}\"" )
#INPUTS=( "friendster;\"${SET}\"" "road-europe;\"${SET}\"" "twitter50;\"${SET}\"" "twitter40;\"${SET}\"" "kron30;\"${SET}\"" )
#INPUTS=( "friendster;\"${SET}\"" "road-europe;\"${SET}\"" "twitter50;\"${SET}\"" "twitter40;\"${SET}\"" )
#INPUTS=( "twitter40;\"${SET}\"" "kron30;\"${SET}\"" )#INPUTS=( "kron30;\"${SET}\"" )
#INPUTS=( "kron30;\"${SET}\"" )
#INPUTS=( "road-europe;\"${SET}\"" )
INPUTS=("twitter40;\"${SET}\"")
#INPUTS=( "road-europe;\"${SET}\""  "twitter40;\"${SET}\"" )
#INPUTS=( "road-europe;\"${SET}\""  "friendster;\"${SET}\"" "twitter40;\"${SET}\"")
#INPUTS=( "road-europe;\"${SET}\""  "friendster;\"${SET}\"" )
#INPUTS=( "road-europe;\"${SET}\""  "twitter40;\"${SET}\"" "twitter50;\"${SET}\"")
#INPUTS=( "road-europe;\"${SET}\""  "twitter50;\"${SET}\"" "kron30;\"${SET}\"")
#INPUTS=( "friendster;\"${SET}\"" )
#INPUTS=( "friendster;\"${SET}\"" )
#INPUTS=( "wdc;\"${SET}\"" )
#INPUTS=( "friendster;\"${SET}\"" "clueweb12;\"${SETc}\"")
#INPUTS=("reddit;\"${SET}\"" "ogbn-products;\"${SET}\"")
#INPUTS=("ppi;\"${SET}\"" )

QUEUE=skx-normal
#QUEUE=skx-large
#QUEUE=skx-dev
#QUEUE=normal
#QUEUE=development#PARTS=( "oec" "cvc" )
#PARTS=( "oec" "cvc" )
#PARTS=( "oec" "cvc" )
#PARTS=( "oec" )
#PARTS=( "cvc" )

#PARTS=( "oec" "cvc" )
#PARTS=( "blocked-oec" )
PARTS=("blocked-cvc")
#PARTS=( "random-oec" )
#PARTS=( "random-oec" )
#PARTS=( "random-cvc" )

#ALGO=( " " )
#ALGO=( " --algo=DistPush" )
#ALGO=( " --algo=DistAfforest" " --algo=DistShortcuttingKVStore" )
#ALGO=( " --algo=DistAfforest" )
#ALGO=( " --algo=DeterministicKVStore" )
#ALGO=( " --algo=Deterministic" )
#ALGO=( " --algo=DistPriorityUsingNodeKVStore")
#ALGO=(" --algo=DistMCLabelProp")
#ALGO=( "--algo=DistLabelPropKVStore" )
#ALGO=( " --algo=DistPush" " --algo=DistShortcuttingKVStore" "--algo=DistLabelPropKVStore" )
#ALGO=( " --algo=DistPointerJumpingNodeKVStore" )
#ALGO=( " --algo=DistSVNodeKVStore" " --algo=DistSVKVStore" "--algo=DistLabelPropKVStore" " --algo=DistShortcuttingNodeKVStore" " --algo=DistPointerJumpingNodeKVStore" )
#ALGO=( " --algo=DistSVKVStore" " --algo=DistShortcuttingNodeKVStore" " --algo=DistPush" " --algo=DistLabelPropKVStore" )
#ALGO=( " --algo=DistSVKVStore" "--algo=DistLabelPropKVStore" " --algo=DistPointerJumpingNodeKVStore" " --algo=DistShortcuttingNodeKVStore" )
#ALGO=( " --algo=DistSVKVStore" " --algo=DistPointerJumpingNodeKVStore" " --algo=DistShortcuttingNodeKVStore" )
#ALGO=( " --algo=DistShortcuttingNodeKVStore" " --algo=DistPointerJumpingNodeKVStore" )
#ALGO=( " --algo=DistShortcuttingNodeKVStore" )
#ALGO=( " --algo=DistPointerJumpingNodeKVStore" )
#ALGO=( "--algo=DeterministicMC" )
#ALGO=( "--algo=DeterministicMCNoGAR" )
#ALGO=( "--algo=DistLabelPropKVStore" )
#ALGO=( " --algo=DistSVKVStore"  )
ALGO=(" --algo=DistMCSV")
#ALGO=( " --algo=DistShortcuttingNodeKVStore" )
#ALGO=( " --algo=DistShortcuttingKVStore" "--algo=DistLabelPropKVStore" )
#ALGO=( " --algo=DistShortcuttingNodeKVStore" )
#ALGO=( "--algo=DistLabelPropKVStore" )
#ALGO=( "--algo=DistKVStorePointerJumpingDirect" )
#ALGO=( " --algo=DistKVStoreShortcuttingDirect" "--algo=DistKVStorePointerJumpingDirect" )
#ALGO=( " --algo=DistPush" " --algo=DistKVStoreShortcuttingDirect" )
#ALGO=(" --algo=DistPush" " --algo=DistLabelPropKVStore" )
#ALGO=(" --algo=DistKVStoreShortcuttingDirect")
#ALGO=( " --use_kvstore=true" " --use_kvstore=false" )
#ALGO=( " --use_kvstore=false" )
#ALGO=( " --use_kvstore=true" )
#ALGO=( " --algo=DistBoruvka" )

#POSTFIX=( "_variant1" "_variant2" "_variant3" "_variant4" "_variant5" "_variant6" "_variant7" )
#POSTFIXFLAGS=( " --variant_type=1 --pin=true " " --variant_type=2 --pin=true "  \
#               " --variant_type=3 --pin=true " " --variant_type=4 --pin=true "  \
#               " --variant_type=5 --pin=true " " --variant_type=6 --pin=false "  \
#               " --variant_type=7 --pin=false " )
#POSTFIX=( "_variant1" "_variant2" "_variant3" "_variant4" "_variant5" )
#POSTFIXFLAGS=( " --variant_type=1 --pin=false " " --variant_type=2 --pin=false "  \
#               " --variant_type=3 --pin=false " " --variant_type=4 --pin=false "  \
#               " --variant_type=5 --pin=false " )
#POSTFIX=( "_variant5" )

#POSTFIXFLAGS=( " --variant_type=1 " " --variant_type=2 "  \
#               " --variant_type=3 " " --variant_type=4 " )
#POSTFIX=( "_variant1" "_variant2" "_variant3" "_variant4" )

#POSTFIXFLAGS=( " --variant_type=1 " " --variant_type=2 "  \
#               " --variant_type=3 " )
#POSTFIX=( "_variant1" "_variant2" "_variant3" )

#POSTFIX=( "_variant6" "_variant7" )
#POSTFIXFLAGS=( " --variant_type=1 --pin=false " " --variant_type=5 --pin=false " )
#POSTFIX=( "_variant8_8hosts" )
#POSTFIX=( "_test" )
#POSTFIXFLAGS=( " --variant_type=2 --pin=false " )
#POSTFIX=( "" )
#POSTFIXFLAGS=( " --variant_type=4 " )

POSTFIX=("")
POSTFIXFLAGS=("")

for j in "${INPUTS[@]}"; do
	IFS=";"
	set $j
	for i in "${EXECS[@]}"; do
		for a in "${ALGO[@]}"; do
			for p in "${PARTS[@]}"; do
				for post in "${!POSTFIX[@]}"; do
					psf=${POSTFIX[$post]}
					psf_flag=${POSTFIXFLAGS[$post]}
					echo "./run_stampede_all.sh ${i} ${1} ${2} $QUEUE $p $a $psf $psf_flag"
					./run_stampede_all.sh ${i} ${1} ${2} $QUEUE $p $a "$psf" "$psf_flag" |& tee -a jobs
				done
			done
		done
	done
done
