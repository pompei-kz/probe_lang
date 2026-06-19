.PHONY: run configure build test clean rebuild

PRESET := debug-clang
BUILD_DIR := cmake-build-debug-clang
# Run tests in parallel — each test uses its own DB schema, so they are isolated.
# Override with `make test JOBS=1` to run serially.
JOBS := $(shell nproc 2>/dev/null || echo 4)

run: build
	$(BUILD_DIR)/probe_lang

configure:
	cmake --preset $(PRESET)

build: configure
	cmake --build --preset $(PRESET)

test: build
	ctest --test-dir $(BUILD_DIR) -j$(JOBS) --output-on-failure

rebuild:
	rm -rf $(BUILD_DIR)
	cmake --preset $(PRESET)
	cmake --build --preset $(PRESET)
	ctest --test-dir $(BUILD_DIR) -j$(JOBS) --output-on-failure

clean:
	rm -rf $(BUILD_DIR)
