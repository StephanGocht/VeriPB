ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

CPP_FILES:=\
	./veripb/optimized/constraints.cpp \
	./veripb/optimized/pybindings.cpp \
	./veripb/optimized/parsing.cpp

HPP_FILES:=\
	./veripb/optimized/BigInt.hpp \
	./veripb/optimized/constraints.hpp

.PHONY: install test

test:
	python3 -m pytest ${ROOT_DIR}

install:
	pip3 install ${ROOT_DIR}

cpp ${CPP_FILES} ${HPP_FILES}:
	$(CXX) -Wall -shared -std=c++17 -fPIC \
		`python3 -m pybind11 --includes` \
		${CPP_FILES} \
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