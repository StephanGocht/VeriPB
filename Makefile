ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

.PHONY: install test

test:
	python3 -m pytest ${ROOT_DIR}

install:
	pip3 install ${ROOT_DIR}
