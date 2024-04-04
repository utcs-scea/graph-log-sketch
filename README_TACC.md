<!--
  ~ SPDX-License-Identifier: BSD-2-Clause
  ~ Copyright (c) 2023. University of Texas at Austin. All rights reserved.
  -->

# TACC and Galois

Running Galois on TACC has various difficulties from which modules to load
for MPI and to compile, to how to handle and compile dependencies, and to
the mechanisms for running programs on TACC themselves.

## Authors

Patrick Kenney

## General Info

TACC Login Node: ls6.tacc.utexas.edu

TACC Website and Lonestar6 Status: [tacc](https://www.tacc.utexas.edu/portal/system-status/lonestar6)

TACC Node Specifications: 128 cores total (2 Sockets with 64 cores) and 256GB RAM

It is recommended to have at least 3 scripts for TACC: `scripts/tacc_env.sh` to handle
module dependencies and environment variables (invocable via `. scripts/tacc_env.sh`),
`scripts/tacc_run.sh` to actually run your program on TACC, and finally
`scripts/tacc_run_all.sh` to run the whole benchmark suite and track configurations
(`scripts/tacc_run_all.sh` are recommended be written later after you have familiarized
yourself with TACC runs for your program).

## TACC Commands

```shell
# get currently scheduled/running jobs
squeue -u <username>
# cancel a scheduled/running job (job-id can be found from squeue)
scancel <job-id>
```

## Scratch Filesystem

Users on TACC have space quotas for their home directories, to avoid strange
errors users should do all work on the scratch filesystem that *IS* accessible
from all compute nodes.  *WARNING:* the scratch filesystem is a temporary
filesystem and all files untouched for 20 days will be automatically deleted.
This scratch filesystem can be accessed via:

```shell
# The SCRATCH env var is set automatically for you
cd $SCRATCH
```

## Modules

Users do not have root access on TACC so dependencies are typically handled
using `module`.

The following modules list will work for running Galois programs without GPUs:

```shell
module load intel/19.1.1
module load impi/19.0.9
module load python3/3.9.7
module load boost-mpi/1.72
```

If any of the above modules are no longer present you can get more info on a
module by running `module spider <module>` i.e. `module spider impi`.  This
command will tell you any dependencies required to load the module.  For example,
the `impi` module requires the `intel` module.

Note that sometimes modules will conflict and when this happens the new module
is loaded and the old module is unloaded automatically with only a warning
printed.  For example, the modules `gcc/11.2.0` and `intel/19.1.1` conflict and
in the words of Highlander: There can be only one.

## Example Environment Script

Here is an example for TACC's environment script `scripts/tacc_env.sh`, it
must be loaded before building (`. scripts/tacc_env.sh`) and should be loaded
as part of a TACC job.

```shell
module load intel/19.1.1
module load impi/19.0.9
module load python3/3.9.7
module load boost-mpi/1.72

export LLVM_DIR="$WORK/llvm-project/build/cmake/modules/CMakeFiles/"
export fmt_DIR="$WORK/fmt/build/"
```

## Building Dependencies

Ensure you load dependencies before building anything via the following or
equivalent:

```shell
. scripts/tacc_env.sh
```

If you are not using a package manager like `conda` or `conan` for your C++
files then you need to clone and download your dependencies manually.  For
Galois you need [llvm](https://github.com/llvm/llvm-project) and
[fmt](https://github.com/fmtlib/fmt).

The following will build `fmt` properly:

```shell
cd $WORK
git clone https://github.com/fmtlib/fmt
cd fmt
cmake -B build
cd build
make -j4
```

`llvm` needs to be built with certain `cmake` flags in order to work with
Galois, the following sequence will build `llvm` properly:

```shell
idev -m 120 # enter an interactive dev machine to use more threads
cd $WORK
git clone https://github.com/llvm/llvm-project
cd llvm-project
cmake -S llvm -B build -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_RTTI=ON \
  -DLLVM_ENABLE_ZSTD=OFF -DLLVM_ENABLE_ZLIB=OFF -DLLVM_ENABLE_TERMINFO=OFF
cd build
make -j
```

## Building Galois

After you have built Galois' dependencies the process is more familiar.

The important bits are to set env vars to tell `cmake` where to find your
prebuilt dependencies.  For example:
`export LLVM_DIR="$WORK/llvm-project/build/cmake/modules/CMakeFiles/"`.
It is recommended to define the env vars as part of your TACC environment
script for ease of use.

For those unfamiliar with `cmake`, if you call `cmake` which a wrong library
version or compiler, then `cmake` will cache this choice.  In order to rebuild
with a different library version or compiler you must remove the file
`build/CMakeCache.txt` and then rebuild with the proper settings.

<!-- note: meyer did not have this issue: -->
Another caviat is with `llvm`: my builds were unable to find the header files
for `llvm` despite `cmake` finding the dependency.  In order to resolve this
I used the following `cmake` hack to include them directly:
`target_include_directories(<exe> PRIVATE $WORK/llvm-project/llvm/include)`.
Note that this does not break builds for different machines since `cmake` will
simply ignore paths that do not exist on the current filesystem.

## Launching Jobs

All scheduling should be done via scripts in order to get closer to the
Configuration as Code paradigm.  This will help you run what you mean to run,
be able to tell what was run in the past, and make the whole process far
faster than manual running.

Here is a bare template for `scripts/tacc_run.sh` with comments explaining
each section (arguments inside `<brackets>` need to be replaced with application
specific arguments that are completely determined by the user):

```shell
#!/bin/bash

# required sbatch parameters

# note that TIME is in the format H:MM:SS

# note that the bash paradigm `HOSTS="${HOSTS:-<default>}`
# means: set the env var HOSTS equal to $HOSTS if HOSTS is
# already set, if it is not set then set `HOSTS=default`
HOSTS="${HOSTS:-1}"
PROCS="${PROCS:-1}"
TIME="${TIME:-0:15:00}"
QUEUE="${QUEUE:-normal}"
JOBS="${JOBN:-<project-name>-run}"
OUTS="${OUTS:-<project-name>}"

ENV=${SCRATCH}/<project>/scripts/tacc_env.sh

# not strictly required but useful for many HPC applications
THREADD=$((128 * ${HOSTS} / ${PROCS}))

# These variables are not necessary but recommended for ease of use
# The data directory is helpful for storing outputs and is recommended
# but not necessary
BUILD="${BUILD:-$SCRATCH/<project>/build}"
DATA="${DATA:-$SCRATCH/<project>/data}"
THREADS="${THREADS:-${THREADD}}"

# JOBN should be parameterized with application parameters as well
# possibly time as well time prevent conflicts and overwriting
JOBN=${DATA}/${JOBS}_${HOSTS}_${PROCS}_${THREADS}

# print statements to validate input, can and should
# be extended with application parameters
echo $HOSTS
echo $PROCS
echo $THREADS
echo $TIME
echo $DATA
echo $QUEUE
echo $JOBN

# start of job that runs on the supercomputer
sbatch << EOT
#!/bin/bash

# special arguments passed to sbatch here instead of by command line
# the mail arguments are optional and are just there to send you email
# notifications when your jobs are scheduled and complete
#SBATCH -J ${JOBN}
#SBATCH -o ${JOBN}.out
#SBATCH -e ${JOBN}.err
#SBATCH -t ${TIME}
#SBATCH -N ${HOSTS}
#SBATCH -n ${PROCS}
#SBATCH --mail-type=all
#SBATCH --mail-user=<your-email>@utexas.edu
#SBATCH -p ${QUEUE}

# ensure the proper runtime environment is set
module purge
. ${ENV}

# not strictly required but useful for many HPC applications
export OMP_NUM_THREADS=${THREADS}

# actually run the equivalent of `mpirun` the `--` ensures arguments
# are passed to your executable and not `ibrun`
ibrun -- ${BUILD}/<executable>

EOT
```

The first part and bulk of your run scripts should be configuration
variables. You will have to add variables for your application
specific arguments here.

The second part prints several of the configuration variables.  It
is recommended to extend this section with some application arguments
and to check this output after a job is launched.

The third and final part is the bash script that actually runs on
the supercomputer.  It is only necessary to modify the `mail` targets
for `sbatch` and the line for `ibrun` where you specify the program
arguments.  You can think of `ibrun` like `mpirun`.

Note that the bash paradigm `HOSTS="${HOSTS:-<default>}` means:
set the env var `HOSTS` equal to `$HOSTS` if `HOSTS` is already set,
if it is not set then set `HOSTS=default`.

## Scheduling

TACC is a very large and powerful distributed system with many hosts.
However there are many users that wish to run workloads on TACC and
the queues can be upwards of 1000 jobs.  In order to get your jobs
scheduled faster there are a few important toggles in the run script
that should be minimized.

Note that `PROCS` should always be at least the same value as `HOSTS`,
otherwise you will have `HOSTS - PROCS` number of physical machines
lying idle for no reason and everybody loses.

The primary arguments that determine how fast jobs are scheduled on
TACC are `TIME`, `HOSTS`, and `QUEUE`.

Setting `TIME` as small as possible will go a long way to getting your
jobs scheduled quickly, however if you are too aggressive then your
job will be terminated when it runs for `TIME`.
`TIME` is usually set in the format `H:MM:SS`.

The other argument `HOSTS` is arguably more important, in order for
your job to run there need to be `$HOSTS` idle nodes on TACC and for
large numbers of hosts the scheduling algorithm will severely penalize jobs.

It may be the case you need to run on a lot of virtual hosts because
your program is single-threaded, does not scale past some threshold
like 16 threads, or for some other reason it is okay for multiple versions
of your program to run on the same host.  In this case you can increase
`PROCS` to the number of virtual hosts you need and TACC will schedule
multiple virtual hosts from your program's perspective across the smaller
amount of physical hosts.

The final argument `QUEUE` also has a large impact, typically you will
want to use `normal`.  But for some use cases you may want to use more
"expensive" machines, like hosts with GPUs or more memory.
These more expensive queues would be `gpu-a100`, `gpu-a100-dev`, `gpu-h100`,
`large`, etc.

## Benchmark Suites

After you have run and tested a few singular jobs on TACC and gotten
an idea of the compute and memory it is a good idea to write a script
to run your while benchmark.  That is, a script to run jobs for different
number of hosts/compute power in general and for different inputs.

Putting your entire benchmark into a script makes it much easier and
faster to run benchmarks, reduces errors, and if you are using `git`
to track changes then you can be confident in what was actually run
in past benchmark suite runs.

Here is an example way to write a benchmark suite script varying
compute power and inputs:

```shell
#!/bin/bash

# Data dependent variables
GRAPH_SCALE_0="/scratch/09601/pkenney/pando-workflow4-galois/data/wf4_100_10000_200000_15000000_0.csv"
RRR_SETS_SCALE_0="152101000"
INFLUENTIAL_THRESHOLD_SCALE_0="10100"
GRAPH_NAME_SCALE_0="commercial-0"
HOSTS_SCALE_0="2"
TIME_SCALE_0="0:30:00"

GRAPH_SCALE_1="/scratch/09601/pkenney/pando-workflow4-galois/data/wf4_100_20000_400000_30000000_0.csv"
RRR_SETS_SCALE_1="304201000"
INFLUENTIAL_THRESHOLD_SCALE_1="20100"
GRAPH_NAME_SCALE_1="commercial-1"
HOSTS_SCALE_1="2"
TIME_SCALE_1="0:45:00"

# Compute dependent variables
PROCS_0="8"
PROCS_1="16"

# Handle graph scale 0
export HOSTS="${HOSTS_SCALE_0}"
export TIME="${TIME_SCALE_0}"
export GRAPH_FILE="${GRAPH_SCALE_0}"
export GRAPH_NAME="${GRAPH_NAME_SCALE_0}"
export RRR_SETS="${RRR_SETS_SCALE_0}"
export INFLUENTIAL_THRESHOLD="${INFLUENTIAL_THRESHOLD_SCALE_0}"

export PROCS="${PROCS_0}"
bash scripts/tacc_run.sh
export PROCS="${PROCS_1}"
bash scripts/tacc_run.sh

# Handle graph scale 1
export HOSTS="${HOSTS_SCALE_1}"
export TIME="${TIME_SCALE_1}"
export GRAPH_FILE="${GRAPH_SCALE_1}"
export GRAPH_NAME="${GRAPH_NAME_SCALE_1}"
export RRR_SETS="${RRR_SETS_SCALE_1}"
export INFLUENTIAL_THRESHOLD="${INFLUENTIAL_THRESHOLD_SCALE_1}"

export PROCS="${PROCS_0}"
bash scripts/tacc_run.sh
export PROCS="${PROCS_1}"
bash scripts/tacc_run.sh
```

In the above script we first define all of the required env vars
for `scripts/tacc_run.sh` scripts in general and our application
dependent env vars for each input scale.

Then we define the env vars to determine compute power, since the
above application was more dependent on available memory than
compute power the number of physical hosts was set alongside
the data dependent vars, this is not always the case.

To be clear there is no right or wrong way to setup a benchmark
suite necessary, but it should be clearly formatted with clear
intent so readers including you can easily understand what is going
on.

It is important that your benchmark suite script calls your run
script directly instead of reinventing the wheel.
