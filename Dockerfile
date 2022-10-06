# syntax=docker/dockerfile:1

FROM ubuntu:18.04

# Build Requirenments
RUN apt-get update && apt-get install --no-install-recommends -y \
		python3 \
		python3-pip \
		python3-dev \
		g++ \
		libgmp-dev \
	&& pip3 install \
		setuptools

# Test Requirenments
RUN apt-get install make && pip3 install pytest

# Copy From Working Directory
COPY . /app
WORKDIR /app

RUN pip3 install .
RUN make test -j



