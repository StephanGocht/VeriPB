#!/bin/bash

# this script requires google perf tools to be installed
# (https://github.com/gperftools/gperftools) or according package from
# package manager

VERIPB_DIR=`dirname ${BASH_SOURCE[0]}`
# heap and performance
# LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libprofiler.so:/usr/lib/x86_64-linux-gnu/libtcmalloc.so CPUPROFILE=prof.out CPUPROFILE_FREQUENCY=1000 HEAPPROFILE=./heap.prof veripb "$@"
# performance only
LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libprofiler.so CPUPROFILE=prof.out CPUPROFILE_FREQUENCY=1000 veripb "$@"
# heap only
# LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libtcmalloc.so HEAPPROFILE=./heap.prof veripb "$@"
google-pprof --addresses --callgrind $VERIPB_DIR/veripb/optimized/constraints.cpython-36m-x86_64-linux-gnu.so prof.out > prof.callgrind