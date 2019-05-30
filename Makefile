ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

.PHONY: install test

test:
	pytest-3 ${ROOT_DIR}

install:
	pip3 install ${ROOT_DIR}
