.PHONY: run configure build test clean rebuild

PRESET := debug-clang
BUILD_DIR := cmake-build-debug-clang

run: build
	$(BUILD_DIR)/probe_lang

configure:
	cmake --preset $(PRESET)

build: configure
	cmake --build --preset $(PRESET)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

rebuild:
	rm -rf $(BUILD_DIR)
	cmake --preset $(PRESET)
	cmake --build --preset $(PRESET)
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	rm -rf $(BUILD_DIR)
