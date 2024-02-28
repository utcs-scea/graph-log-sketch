# SPDX-License-Identifier: MIT
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

SHELL := /bin/bash

UNAME ?= $(shell whoami)
UID ?= $(shell id -u)
GID ?= $(shell id -g)

BASE_IMAGE_NAME ?= galois
IMAGE_NAME ?= ${UNAME}-${BASE_IMAGE_NAME}
SRC_DIR ?= $(shell pwd)
VERSION ?= $(shell git log --pretty="%h" -1 Dockerfile.dev)

CONTAINER_SRC_DIR ?= /galois
CONTAINER_BUILD_DIR ?= /galois/build
CONTAINER_WORKDIR ?= ${CONTAINER_SRC_DIR}
CONTAINER_CONTEXT ?= default
CONTAINER_OPTS ?=
CONTAINER_CMD ?= setarch `uname -m` -R bash -l
INTERACTIVE ?= i

BUILD_TYPE ?= Release

# CMake variables
GALOIS_TEST_DISCOVERY_TIMEOUT ?= 150
GALOIS_EXTRA_CXX_FLAGS ?= ""

# Developer variables that should be set as env vars in startup files like .profile
GALOIS_CONTAINER_MOUNTS ?=
GALOIS_CONTAINER_ENV ?=

dependencies: dependencies-asdf

dependencies-asdf:
	@echo "Updating asdf plugins..."
	@asdf plugin update --all >/dev/null 2>&1 || true
	@echo "Adding new asdf plugins..."
	@cut -d" " -f1 ./.tool-versions | xargs -I % asdf plugin-add % >/dev/null 2>&1 || true
	@echo "Installing asdf tools..."
	@cat ./.tool-versions | xargs -I{} bash -c 'asdf install {}'
	@echo "Updating local environment to use proper tool versions..."
	@cat ./.tool-versions | xargs -I{} bash -c 'asdf local {}'
	@asdf reshim
	@echo "Done!"

hooks:
	@pre-commit install --hook-type pre-commit
	@pre-commit install-hooks

pre-commit:
	@pre-commit run -a

git-submodules:
	@git submodule update --init --recursive

ci-image:
	@${MAKE} docker-image-dependencies
	@docker --context ${CONTAINER_CONTEXT} build \
	--build-arg SRC_DIR=${CONTAINER_SRC_DIR} \
	--build-arg BUILD_DIR=${CONTAINER_BUILD_DIR} \
	--build-arg UNAME=runner \
  --build-arg UID=1078 \
  --build-arg GID=504 \
	-t galois:${VERSION} \
	--file Dockerfile.dev \
	--target dev .

docker-image:
	@${MAKE} docker-image-dependencies
	@docker --context ${CONTAINER_CONTEXT} build \
	--build-arg SRC_DIR=${CONTAINER_SRC_DIR} \
	--build-arg BUILD_DIR=${CONTAINER_BUILD_DIR} \
	--build-arg UNAME=${UNAME} \
	--build-arg IS_CI=false \
  --build-arg UID=${UID} \
  --build-arg GID=${GID} \
	-t ${IMAGE_NAME}:${VERSION} \
	--file Dockerfile.dev \
	--target dev .

docker-image-dependencies:
	@mkdir -p build
	@mkdir -p data

.PHONY: docker
docker:
	@docker --context ${CONTAINER_CONTEXT} run --rm \
	-v ${SRC_DIR}/:${CONTAINER_SRC_DIR} \
	${GALOIS_CONTAINER_MOUNTS} \
	${GALOIS_CONTAINER_ENV} \
	--privileged \
	--workdir=${CONTAINER_WORKDIR} \
	${CONTAINER_OPTS} \
	-${INTERACTIVE}t \
	${IMAGE_NAME}:${VERSION} \
	${CONTAINER_CMD}

.PHONY: cmake
cmake:
	@echo "Must be run from inside the dev Docker container"
	@cmake \
  -S ${SRC_DIR} \
  -B ${BUILD_DIR} \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
	-DCMAKE_CXX_FLAGS=${GALOIS_EXTRA_CXX_FLAGS} \
  -DCMAKE_INSTALL_PREFIX=/opt/galois \
  -DBUILD_TESTING=ON \
  -DBUILD_EXAMPLES=ON \
  -DBUILD_DOCS=${PANDO_BUILD_DOCS} \
	-DGALOIS_TEST_DISCOVERY_TIMEOUT=${GALOIS_TEST_DISCOVERY_TIMEOUT} \
	-DCMAKE_CXX_COMPILER=g++-12 \
  -DCMAKE_C_COMPILER=gcc-12

setup: cmake

.PHONY: tests
tests:
	@ctest --test-dir ${BUILD_DIR} --output-on-failure

run-tests: tests

# this command is slow since hooks are not stored in the container image
# this is mostly for CI use
docker-pre-commit:
	@docker --context ${CONTAINER_CONTEXT} run --rm \
	-v ${SRC_DIR}/:${CONTAINER_SRC_DIR} --privileged \
	--workdir=${CONTAINER_WORKDIR} -t \
	${IMAGE_NAME}:${VERSION} bash -lc "git config --global --add safe.directory /galois && make hooks && make pre-commit"
