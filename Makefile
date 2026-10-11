JOBS := $(patsubst -j%,%,$(filter -j%,$(MAKEFLAGS)))
ifeq ($(JOBS),)
JOBS := 1
endif

LIB_DIR        := nexilis
LIB_BUILD      := $(LIB_DIR)/build
WINDOWS_BUILD  := $(LIB_DIR)/build-windows
TEST_CPP_DIR   := tests/nexilis
TEST_C_DIR     := tests/nexilisc
TEST_CS_DIR    := tests/CSharpBindings.Tests
PREMAKE_DIR    := tests/premake
TEST_LUA_DIR   := tests/lua
EXAMPLES_DIR   := examples
PRE_COMMIT_DIR := scripts/pre_commit

CSHARP_OUTPUT ?= $(CURDIR)/dist

.PHONY: all install build-windows install-csharp test unit-test test-cpp test-c test-csharp test-premake test-lua test-examples lint-python format cppcheck best_practises check_editorconfig pre-commit pre-commit-all clean clean-tests help

all: install

install:
	@echo "Building and installing Nexilis..."
	rm -rf $(LIB_BUILD)
	cd $(LIB_DIR) && cmake -B build -DCMAKE_INSTALL_PREFIX=$(CURDIR)/$(LIB_BUILD)/install
	MAKEFLAGS= cmake --build $(LIB_BUILD) --target install --parallel $(JOBS)

build-windows:
	@echo "Cross-building Nexilis for Windows..."
	cmake -S $(LIB_DIR) -B $(WINDOWS_BUILD) -DCMAKE_TOOLCHAIN_FILE=$(CURDIR)/$(LIB_DIR)/cmake/mingw_toolchain.cmake
	MAKEFLAGS= cmake --build $(WINDOWS_BUILD) --parallel $(JOBS)

install-csharp:
	@echo "Building and installing C# bindings..."
	python3 scripts/install_csharp.py --input $(CURDIR)/$(LIB_DIR) --output $(CSHARP_OUTPUT)

test: test-cpp test-c test-csharp test-premake test-lua test-examples

unit-test: test-cpp test-c test-csharp

test-cpp:
	@echo "Building C++ tests..."
	cmake -B $(TEST_CPP_DIR)/build $(TEST_CPP_DIR)
	MAKEFLAGS= cmake --build $(TEST_CPP_DIR)/build --parallel $(JOBS)
	@echo "Running C++ tests..."
	$(TEST_CPP_DIR)/build/nexilis_tests

test-c:
	@echo "Building C tests..."
	cmake -B $(TEST_C_DIR)/build $(TEST_C_DIR)
	MAKEFLAGS= cmake --build $(TEST_C_DIR)/build --parallel $(JOBS)
	@echo "Running C tests..."
	$(TEST_C_DIR)/build/nexilis_c_tests

test-csharp:
	@echo "Building C# bindings..."
	cd bindings/csharp && dotnet build
	@echo "Running C# tests..."
	cd $(TEST_CS_DIR) && dotnet test

test-premake:
	@echo "Building Nexilis with premake..."
	cd $(LIB_DIR) && premake5 gmake && MAKEFLAGS= make config=debug --silent -j$(JOBS)
	@echo "Building premake test..."
	cd $(PREMAKE_DIR) && premake5 gmake && MAKEFLAGS= make config=debug --silent -j$(JOBS)
	@echo "Running premake test..."
	$(PREMAKE_DIR)/bin/Debug/premake_test

test-lua:
	@echo "Building native Lua test dependencies..."
	MAKEFLAGS= cmake --build $(LIB_BUILD) --parallel $(JOBS)
	@echo "Running Lua client tests..."
	bash $(TEST_LUA_DIR)/run.sh

test-examples:
	@echo "Building and running examples..."
	cd $(PRE_COMMIT_DIR) && python3 run_examples.py

pre-commit:
	cd $(PRE_COMMIT_DIR) && python3 pre_commit.py --minimal

pre-commit-all:
	cd $(PRE_COMMIT_DIR) && python3 pre_commit.py --all

lint-python:
	cd $(PRE_COMMIT_DIR) && python3 flake8_check.py

format:
	cd $(PRE_COMMIT_DIR) && python3 format.py

cppcheck:
	cd $(PRE_COMMIT_DIR) && python3 cppcheck.py

best_practises:
	cd $(PRE_COMMIT_DIR) && python3 best_practises.py

check_editorconfig:
	cd $(PRE_COMMIT_DIR) && python3 check_editorconfig.py

clean: clean-tests
	rm -rf $(LIB_BUILD)
	rm -rf $(WINDOWS_BUILD)

clean-tests:
	rm -rf $(TEST_CPP_DIR)/build
	rm -rf $(TEST_C_DIR)/build
	rm -rf $(PREMAKE_DIR)/bin $(PREMAKE_DIR)/obj
	rm -rf $(TEST_LUA_DIR)/build
	rm -rf $(EXAMPLES_DIR)/*/build

help:
	@echo "Targets:"
	@echo "  install          Build and install the library (default)"
	@echo "  build-windows    Cross-build Windows DLLs with MinGW-w64"
	@echo "  install-csharp   Build and deploy C# bindings (CSHARP_OUTPUT=./dist)"
	@echo "  test             Build and run all tests"
	@echo "  unit-test        Build and run C++/C/C# unit tests only"
	@echo "  test-cpp         Build and run C++ tests only"
	@echo "  test-c           Build and run C tests only"
	@echo "  test-csharp      Build and run C# tests only"
	@echo "  test-premake     Build with premake5 and run premake test"
	@echo "  test-lua         Run Lua client smoke and integration tests"
	@echo "  test-examples    Build and run all examples via the checker script"
	@echo "  lint-python      Run flake8 over the Python scripts"
	@echo "  format           Run clang-format check on C++/C sources"
	@echo "  cppcheck         Run cppcheck on C++/C sources"
	@echo "  best_practises   Check correct type alias usage"
	@echo "  check_editorconfig Check files against .editorconfig rules"
	@echo "  pre-commit       Run minimal pre-commit checks (format, best practices, cppcheck, flake8)"
	@echo "  pre-commit-all   Run all pre-commit checks including tests and Python linting"
	@echo "  clean            Remove all build artifacts"
	@echo "  clean-tests      Remove only test and example build artifacts"
