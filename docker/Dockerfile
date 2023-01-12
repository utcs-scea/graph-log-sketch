#FROM ubuntu:22.04
FROM nvidia/cuda:12.0.0-devel-ubuntu22.04
#RUN apt update -y && apt-get -y install add-apt-key ca-certificates
#RUN apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 15CF4D18AF4F7421
#COPY ./ubuntu-toolchain-llvm-jammy.list /etc/apt/sources.list.d/ubuntu-toolchain-llvm-jammy.list
RUN apt update -y &&\
  apt install -y \
  cmake \
  gcc \
  g++ \
  build-essential \
  make \
  libboost-all-dev \
  libfmt-dev \
  lsb-release \
  wget \
  software-properties-common \
  gnupg \
  gdb \
  vim \
  git


RUN bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"

ENV SRC_DIR=/LS_CSR
ENV BUILD_DIR=/LS_CSR/docker-build

COPY prep.sh /prep.sh
COPY build.sh /build.sh
