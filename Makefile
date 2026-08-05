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
CMAKE_PRESET := $(shell echo $(CMAKE_BUILD_TYPE) | tr '[:upper:]' '[:lower:]')

.PHONY: setup
setup: setup/hooks setup/tools ## Install all dependencies and development tools

.PHONY: setup/hooks
setup/hooks: ## Install git hooks
	ln -sf "$(PWD)/git/hooks/pre-commit" .git/hooks/pre-commit
	echo "✅ Git hooks installed"

.PHONY: setup/tools
setup/tools: ## Install development tools
ifeq ($(UNAME_S),Linux)
	sudo apt-get update
	sudo apt-get install -y \
		build-essential \
		gdb \
		cmake \
		ninja-build
	curl -o /tmp/llvm.sh https://apt.llvm.org/llvm.sh
	chmod +x /tmp/llvm.sh
	sudo /tmp/llvm.sh 22
	sudo apt-get install -y clang-format-22
	sudo ln -sf /usr/bin/clang-format-22 /usr/bin/clang-format
else ifeq ($(UNAME_S),Darwin)
	xcode-select -p >/dev/null 2>&1 || xcode-select --install
	brew install \
		cmake \
		ninja \
		clang-format \
		gcc-13
	
else
	echo "Unsupported OS: $(UNAME_S)"
	exit 1
endif

.PHONY: build
build: ## Build all targets (library is header-only; builds tests and examples)
	cmake --build --preset $(CMAKE_PRESET)

.PHONY: check
check: check/c check/markdown ## Check formatting

.PHONY: check/c
check/c: ## Check formatting for C
	$(MAKE) format/c/common CLANG_FORMAT_OPTIONS="--dry-run --Werror"

.PHONY: check/markdown
check/markdown: ## Check formatting for markdown
	npm run lint:markdown

.PHONY: clean
clean: ## Remove build artifacts
	cmake --build build/$(CMAKE_PRESET) --target clean

.PHONY: configure
configure: ## Configure CMake cache
	cmake --preset $(CMAKE_PRESET)

.PHONY: format
format: format/c format/markdown ## Format all files

.PHONY: format/c
format/c: ## Format all source files
	$(MAKE) format/c/common CLANG_FORMAT_OPTIONS="-i"

.PHONY: format/c/common
format/c/common: ## Format common source files
	find . -type f \( -name "*.h" -o -name "*.c" \) \
		-not -path "./build/*" \
		-print0 | xargs -0 clang-format $(CLANG_FORMAT_OPTIONS)

.PHONY: format/markdown
format/markdown: ## Format all markdown files
	npm run format:markdown

.PHONY: test
test: ## Build and run all tests
	cmake --build --preset $(CMAKE_PRESET)
	ctest --preset $(CMAKE_PRESET)

env-%: ## Verify a required environment variable is set
	if [ -z "$($*)" ]; then \
		echo "Error: Environment variable '$*' is not set."; \
		exit 1; \
	fi

.PHONY: help
help: ## Display this help
	@awk 'BEGIN {FS = ":.*##"; printf "\nUsage:\n  make \033[36m\033[0m\n"} /^[a-zA-Z_\/-]+:.*?##/ { printf "  \033[36m%-20s\033[0m %s\n", $$1, $$2 } /^##@/ { printf "\n\033[1m%s\033[0m\n", substr($$0, 5) } ' $(MAKEFILE_LIST)
