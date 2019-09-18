#!/bin/bash

# this script requires google perf tools to be installed
# (https://github.com/gperftools/gperftools) or according package from
# package manager

REFPY_DIR=`dirname ${BASH_SOURCE[0]}`
# heap and performance
# LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libprofiler.so:/usr/lib/x86_64-linux-gnu/libtcmalloc.so CPUPROFILE=prof.out CPUPROFILE_FREQUENCY=1000 HEAPPROFILE=./heap.prof refpy "$@"
# performance only
LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libprofiler.so CPUPROFILE=prof.out CPUPROFILE_FREQUENCY=1000 refpy "$@"
# heap only
# LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libtcmalloc.so HEAPPROFILE=./heap.prof refpy "$@"
google-pprof --addresses --callgrind $REFPY_DIR/refpy/optimized/constraints.cpython-36m-x86_64-linux-gnu.so prof.out > prof.callgrind