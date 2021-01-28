ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

.PHONY: all test install cpp dist clean

CPP_FILES:=\
	./veripb/optimized/constraints.cpp \
	./veripb/optimized/pybindings.cpp \
	./veripb/optimized/parsing.cpp

CPP_FILES_COMPILED=$(CPP_FILES:.cpp=.o)

HPP_FILES:=\
	./veripb/optimized/BigInt.hpp \
	./veripb/optimized/constraints.hpp

CXX_FLAGS?=-O3 -g

PYBINDINCLUDE:=`python3 -m pybind11 --includes`

.PHONY: install test all

all: install

test:
	python3 -m pytest ${ROOT_DIR}

install:
	pip3 install --user -e ${ROOT_DIR}

%.o: %.cpp ${HPP_FILES}
	$(CXX) -c -Wall -std=c++17 -fPIC ${CXX_FLAGS}\
			-o $@ \
			${PYBINDINCLUDE} \
			$< \
			-DPY_BINDINGS


cpp: ${CPP_FILES_COMPILED} ${HPP_FILES}
	$(CXX) -Wall -shared -std=c++17 -fPIC \
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
