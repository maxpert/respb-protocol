# Protocol Benchmark Makefile

CC = gcc
CFLAGS = -I./include -Wall -Wextra -std=c11
LDFLAGS = -lm -lpthread

# Build modes
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O3 -march=native -DNDEBUG
PGO_GEN_FLAGS = -O3 -fprofile-generate
PGO_USE_FLAGS = -O3 -fprofile-use

# Default to release
BUILD ?= release

ifeq ($(BUILD),debug)
    CFLAGS += $(DEBUG_FLAGS)
else ifeq ($(BUILD),pgo-gen)
    CFLAGS += $(PGO_GEN_FLAGS)
    LDFLAGS += -fprofile-generate
else ifeq ($(BUILD),pgo-use)
    CFLAGS += $(PGO_USE_FLAGS)
    LDFLAGS += -fprofile-use
else
    CFLAGS += $(RELEASE_FLAGS)
endif

# Platform-specific flags
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lrt
    CFLAGS += -D_GNU_SOURCE
endif

# Directories
SRCDIR = src
INCDIR = include
TESTDIR = tests
BINDIR = bin
DATADIR = data
RESULTSDIR = results

# Source files
CORE_SOURCES = $(SRCDIR)/respb_parser.c \
               $(SRCDIR)/respb_serializer.c \
               $(SRCDIR)/valkey_resp_parser.c \
               $(SRCDIR)/benchmark.c \
               $(SRCDIR)/metrics.c \
               $(SRCDIR)/workload.c

BENCH_SOURCES = $(CORE_SOURCES) $(SRCDIR)/main.c
TEST_SOURCES = $(CORE_SOURCES) $(TESTDIR)/test_main.c

# Object files
CORE_OBJECTS = $(CORE_SOURCES:.c=.o)
BENCH_OBJECTS = $(BENCH_SOURCES:.c=.o)
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)

# Targets
BENCHMARK = $(BINDIR)/benchmark
TEST_BINARY = $(BINDIR)/test
WORKLOAD_GEN = scripts/generate_workloads.py

# Default target
all: $(BENCHMARK)

# Create directories
$(BINDIR) $(DATADIR) $(RESULTSDIR):
	mkdir -p $@

# Compile object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build benchmark binary
$(BENCHMARK): $(BENCH_OBJECTS) | $(BINDIR)
	$(CC) $(BENCH_OBJECTS) $(LDFLAGS) -o $@
	@echo "Built benchmark: $@"

# Build test binary
$(TEST_BINARY): $(TEST_OBJECTS) | $(BINDIR)
	$(CC) $(TEST_OBJECTS) $(LDFLAGS) -o $@
	@echo "Built tests: $@"

# Generate test workloads
workloads: $(WORKLOAD_GEN) | $(DATADIR)
	@echo "Generating workloads..."
	python3 $(WORKLOAD_GEN) --output $(DATADIR)
	@echo "Workloads generated in $(DATADIR)/"

# Run tests
test: $(TEST_BINARY)
	@echo "Running tests..."
	./$(TEST_BINARY)

# Run benchmarks
bench: $(BENCHMARK) workloads | $(RESULTSDIR)
	@echo "Running benchmarks..."
	./scripts/run_benchmarks.sh

# Run quick benchmark (smaller dataset)
quick-bench: $(BENCHMARK) | $(RESULTSDIR)
	@echo "Running quick benchmark..."
	./$(BENCHMARK) --config configs/quick.conf

# Profile-guided optimization build
pgo: clean
	@echo "=== PGO Step 1: Build with instrumentation ==="
	$(MAKE) BUILD=pgo-gen
	@echo "=== PGO Step 2: Generate profile data ==="
	./$(BENCHMARK) --config configs/pgo_train.conf
	@echo "=== PGO Step 3: Rebuild with profile data ==="
	$(MAKE) BUILD=pgo-use
	@echo "=== PGO build complete ==="

# Analyze results
analyze: | $(RESULTSDIR)
	@echo "Analyzing results..."
	python3 scripts/analyze_results.py $(RESULTSDIR)/*.json

# Compare protocols
compare: | $(RESULTSDIR)
	@echo "Comparing RESP vs RESPB..."
	python3 scripts/compare_protocols.py \
		$(RESULTSDIR)/resp_*.json \
		$(RESULTSDIR)/respb_*.json

# Clean build artifacts
clean:
	rm -f $(SRCDIR)/*.o $(TESTDIR)/*.o
	rm -f $(BENCHMARK) $(TEST_BINARY)
	rm -rf *.gcda *.gcno

# Clean everything including data
distclean: clean
	rm -rf $(BINDIR)/* $(DATADIR)/* $(RESULTSDIR)/*

# Debug build
debug:
	$(MAKE) BUILD=debug

# Release build
release:
	$(MAKE) BUILD=release

# Install dependencies (Python packages)
deps:
	pip3 install -r requirements.txt

# Help target
help:
	@echo "Protocol Benchmark Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build benchmark (default, release mode)"
	@echo "  test         - Build and run tests"
	@echo "  workloads    - Generate test workloads"
	@echo "  bench        - Run full benchmark suite"
	@echo "  quick-bench  - Run quick benchmark"
	@echo "  pgo          - Profile-guided optimization build"
	@echo "  analyze      - Analyze benchmark results"
	@echo "  compare      - Compare RESP vs RESPB results"
	@echo "  clean        - Remove build artifacts"
	@echo "  distclean    - Remove all generated files"
	@echo "  debug        - Build with debug symbols"
	@echo "  release      - Build optimized (default)"
	@echo "  deps         - Install Python dependencies"
	@echo ""
	@echo "Build modes (use BUILD=mode):"
	@echo "  release      - Optimized build (-O3 -march=native)"
	@echo "  debug        - Debug build (-g -O0)"
	@echo "  pgo-gen      - PGO instrumentation"
	@echo "  pgo-use      - PGO optimized"
	@echo ""
	@echo "Examples:"
	@echo "  make                    # Build release"
	@echo "  make BUILD=debug        # Build debug"
	@echo "  make bench              # Run benchmarks"
	@echo "  make test               # Run tests"

.PHONY: all test bench quick-bench workloads analyze compare clean distclean \
        debug release pgo deps help

