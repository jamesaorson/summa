SHELL := /bin/bash
.SHELLFLAGS = -e -c
.DEFAULT_GOAL := help
.ONESHELL:
.SILENT:
MAKEFLAGS += --no-print-directory

ifneq (,$(wildcard ./.env))
    include .env
    export
endif

UNAME_S := $(shell uname -s)

CMAKE_BUILD_TYPE ?= Debug

.PHONY: clean
clean: ## Remove build artifacts
	cmake --build build/debug --target clean

.PHONY: setup
setup: ## Install development dependencies
ifeq ($(UNAME_S),Linux)
	sudo apt-get update
	sudo apt-get install -y \
		build-essential \
		cmake \
		ninja-build \
		clang-format
else ifeq ($(UNAME_S),Darwin)
	brew install \
		cmake \
		ninja \
		clang-format
else
	echo "Unsupported OS: $(UNAME_S)"
	exit 1
endif

.PHONY: configure
configure: ## Configure CMake cache
	cmake -S . -B build \
		-DJAMESAORSON_TEMPLATE_CPP_BUILD_EXAMPLES=ON \
		-DJAMESAORSON_TEMPLATE_CPP_BUILD_TESTS=ON \
		-DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)

.PHONY: build
build: ## Build the library
	cmake --build build \
		--target jamesaorson.template_cpp

.PHONY: build/all/examples
build/all/examples: ## Build all examples
	cmake --build build

.PHONY: test
test: ## Build and run all tests
	cmake --build build
	cd build && ctest

.PHONY: format
format: ## Format all source files
	find . -type f \( -name "*.h" -o -name "*.hpp" -o -name "*.cpp" \) \
		-not -path "./build/*" \
		-print0 | xargs -0 clang-format -i

env-%: ## Verify a required environment variable is set
	if [ -z "$($*)" ]; then \
		echo "Error: Environment variable '$*' is not set."; \
		exit 1; \
	fi

.PHONY: help
help: ## Display this help
	@awk 'BEGIN {FS = ":.*##"; printf "\nUsage:\n  make \033[36m\033[0m\n"} /^[a-zA-Z_\/-]+:.*?##/ { printf "  \033[36m%-20s\033[0m %s\n", $$1, $$2 } /^##@/ { printf "\n\033[1m%s\033[0m\n", substr($$0, 5) } ' $(MAKEFILE_LIST)
