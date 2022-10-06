# syntax=docker/dockerfile:1

FROM ubuntu:14.04

# Build Requirenments
RUN apt-get update && apt-get install wget 

# Download anaconda
RUN wget -O anaconda.sh https://repo.anaconda.com/archive/Anaconda3-2022.05-Linux-x86_64.sh

# Install anaconda
RUN chmod +x anaconda.sh && ./anaconda.sh -b -p /anaconda

# install dependencies
# note: somehow sh seems to bug with anaconda so we use bash instead of just run
RUN ["/bin/bash", "-c", "source /anaconda/bin/activate && conda install -y gxx_linux-64 gmp make"]

# Copy From Working Directory
COPY . /app
WORKDIR /app

# note that "." is the same in sh as "source" in bash

RUN ["/bin/bash", "-c", "source /anaconda/bin/activate && pip3 install ."]
RUN ["/bin/bash", "-c", "source /anaconda/bin/activate && CPATH=/anaconda/include/ LD_LIBRARY_PATH=/anaconda/lib/ CXX_FLAGS=\"-L /anaconda/lib/ -O3\" make test -j"]


