#!/bin/bash

# this script requires google perf tools to be installed
# (https://github.com/gperftools/gperftools) or according package from
# package manager

VERIPB_DIR=`dirname $(realpath ${BASH_SOURCE[0]})`
# # heap and performance
# # LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libprofiler.so:/usr/lib/x86_64-linux-gnu/libtcmalloc.so CPUPROFILE=prof.out CPUPROFILE_FREQUENCY=1000 HEAPPROFILE=./heap.prof veripb "$@"
# # performance only
# LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libprofiler.so CPUPROFILE=prof.out CPUPROFILE_FREQUENCY=1000 python3 $VERIPB_DIR/veripb "$@"
# heap only
LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libtcmalloc.so HEAPPROFILE=./heap.prof veripb "$@"

py_suffix=`python3-config --extension-suffix`
## process profiles
# google-pprof --addresses --callgrind $VERIPB_DIR/veripb/optimized/pybindings${py_suffix} prof.out > callgrind.out.cpp.profile
## show heap profile
# google-pprof --gv $VERIPB_DIR/veripb/optimized/pybindings${py_suffix} heap.prof.0001.heap

# alternatively for perf:
# perf record --call-graph fp python3 $VERIPB_DIR/veripb "$@"
# perf report
