.PHONY: all clean test run debug release asan tsan msan

BUILD_DIR := build

all:
	@echo Specify a target: test, run, or clean

clean:
	@echo Cleaning the build directory...
	rm -rf $(BUILD_DIR)/

# Compile all targets in Debug mode.
debug:
	@echo "Building in debug mode..."
	mkdir -p $(BUILD_DIR)/debug
	cd $(BUILD_DIR)/debug && cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug ../..
	cmake --build $(BUILD_DIR)/debug --parallel

# Compile all targets in Release mode.
release:
	@echo "Building in release mode..."
	mkdir -p $(BUILD_DIR)/release
	cd $(BUILD_DIR)/release && cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release ../..
	cmake --build $(BUILD_DIR)/release --parallel

# Compile and run the server target in Address and Undefined-Behavior sanitizer modes using Clang and -01.
asan:
	@echo "Building in asan mode (both ASAN and UBSAN)..."
	mkdir -p $(BUILD_DIR)/asan
	cd $(BUILD_DIR)/asan && cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_UBSAN=ON \
	-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
	-DCMAKE_CXX_FLAGS_DEBUG="-O1 -g" ../..
	cmake --build $(BUILD_DIR)/asan --parallel -t server
	./$(BUILD_DIR)/asan/server

# Compile and run the server target in Thread sanitizer mode using Clang and -01.
tsan:
	@echo "Building in tsan mode..."
	mkdir -p $(BUILD_DIR)/tsan
# TODO: I get an "unexpected memory mapping" error when compiling the server target under gcc. Sticking to clang for now.
	cd $(BUILD_DIR)/tsan && cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON \
	-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
	-DCMAKE_CXX_FLAGS_DEBUG="-O1 -g" ../..
	cmake --build $(BUILD_DIR)/tsan --parallel -t server
	./$(BUILD_DIR)/tsan/server

# Compile and run the server target in Memory sanitizer mode using Clang and -01.
msan:
	@echo "Building in msan mode (Clang only)..."
	@which clang > /dev/null || (echo "Error: clang not found"; exit 1)
	@which clang++ > /dev/null || (echo "Error: clang++ not found"; exit 1)
	mkdir -p $(BUILD_DIR)/msan
	cd $(BUILD_DIR)/msan && cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_MSAN=ON \
	-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
	-DCMAKE_CXX_FLAGS_DEBUG="-O1 -g" ../..
	cmake --build $(BUILD_DIR)/msan --parallel -t server
	./$(BUILD_DIR)/msan/server

# Run all tests in Debug mode.
test: debug
	@echo "Running tests..."
	cd $(BUILD_DIR)/debug && GTEST_COLOR=1 ctest --verbose --output-on-failure

# NOTE: you can pass arguments after "make run" like so:
# 	make run -- --arg1 --arg2
# However, you can't combine multiple rules and pass arguments.
run: debug
	@echo "Running the main executable with arguments: $(RUN_ARGS)"
# TODO just invoke it here so we simplify this arg forwarding nonsense
	./$(BUILD_DIR)/debug/server $(RUN_ARGS)

# Handle command-line arguments
RUN_ARGS := $(shell if [ "$(word 1,$(MAKECMDGOALS))" = "run" ]; then \
                     echo "$(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))"; \
                 fi)

# Remove the "--" if it's the only argument
ifeq ($(RUN_ARGS),--)
	RUN_ARGS :=
endif

# Prevent make from interpreting the arguments as targets
%:
	@: