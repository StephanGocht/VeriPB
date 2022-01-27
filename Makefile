ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

.PHONY: all test install cpp dist clean dev

CPP_FILES:=\
	./veripb/optimized/constraints.cpp \
	./veripb/optimized/pybindings.cpp \
	./veripb/optimized/parsing.cpp

CPP_FILES_COMPILED=$(CPP_FILES:.cpp=.o)

HPP_FILES:=\
	./veripb/optimized/BigInt.hpp \
	./veripb/optimized/constraints.hpp

PYTHON:=$(shell find veripb -name "*.py")

PYTHON_COMPILED_C=$(PYTHON:.py=.c)

# if you want to use asan you need prepend the following
# when running veripb, if compiled with gcc
# LD_PRELOAD=$(gcc -print-file-name=libasan.so)
CXX_FLAGS?=-O3
CXX_FLAGS:=${CXX_FLAGS} -g
CXX_FLAGS:=${CXX_FLAGS} -DNDEBUG
# CXX_FLAGS:=${CXX_FLAGS} -fsanitize=address


# use full for getting better profiling without too much overhead
# required for perf profiling
# CXX_FLAGS:=${CXX_FLAGS} -fno-omit-frame-pointer
# no inlining can give more details but might lead to slower
# performance and hence skew the profiling information
# CXX_FLAGS:=${CXX_FLAGS} -fno-inline



PYBINDINCLUDE:=`python3 -m pybind11 --includes`

.PHONY: install test all

#all: install
all: cpp

dev: testparser

testparser: ${CPP_FILES} ${HPP_FILES}
	clang++ --std=c++17 ${CPP_FILES} -o testparser -lgmp -O3 -DNDEBUG -fno-omit-frame-pointer -g

test: cpp
	python3 -m pytest ${ROOT_DIR}

install:
	pip3 install --user -e ${ROOT_DIR}

%.o: %.cpp ${HPP_FILES} Makefile
	$(CXX) -c -Wall -std=c++17 -fPIC ${CXX_FLAGS} -fmax-errors=3\
			-o $@ \
			${PYBINDINCLUDE} \
			$< \
			-DPY_BINDINGS


cpp: ${CPP_FILES_COMPILED} ${HPP_FILES}
	$(CXX) -Wall -shared -std=c++17 -fPIC ${CXX_FLAGS} \
		${CPP_FILES_COMPILED} \
		-o veripb/optimized/pybindings`python3-config --extension-suffix` \
		-lgmp -lgmpxx -DPY_BINDINGS



dist:
	echo "creating distribution requires pyinstaller and staticx"
	echo "pip3 install pyinstaller staticx"
	echo "see also"
	echo "https://www.pyinstaller.org/"
	echo "https://github.com/JonathonReinhart/staticx/"

	pyinstaller --onefile veripb_bin.py
	staticx dist/veripb_bin dist/veripb_bin_static

clean:
	find . -name "*.so" -delete
	find . -name "*.o" -delete
	rm $(PYTHON_COMPILED_C) -f
