ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

.PHONY: install test

test:
	python3 -m pytest ${ROOT_DIR}

install:
	pip3 install ${ROOT_DIR}

dist:
	echo "creating distribution requires pyinstaller and staticx"
	echo "pip3 install pyinstaller staticx"
	echo "see also"
	echo "https://www.pyinstaller.org/"
	echo "https://github.com/JonathonReinhart/staticx/"

	pyinstaller --onefile veripb_bin.py
	staticx dist/veripb_bin dist/veripb_bin_static