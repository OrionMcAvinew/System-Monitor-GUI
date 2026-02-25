.DEFAULT_GOAL := help

BUILD_DIR ?= build
CMAKE ?= cmake

.PHONY: help install configure build run test clean doctor

help:
	@echo "Targets:"
	@echo "  make install   - Install dependencies (apt/brew)"
	@echo "  make run       - Configure + build + run app"
	@echo "  make test      - Build and run tests"
	@echo "  make doctor    - Check whether GUI binary can be built"
	@echo "  make clean     - Remove build dir"

install:
	./scripts/install_deps.sh

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DREQUIRE_OPENGL=OFF

build: configure
	$(CMAKE) --build $(BUILD_DIR) -j

run: build
	@if [ -x "$(BUILD_DIR)/SystemMonitor" ]; then \
		"$(BUILD_DIR)/SystemMonitor"; \
	else \
		echo "SystemMonitor binary not found. GUI was likely skipped due missing OpenGL libs."; \
		echo "Run: make install"; \
		echo "Then verify GUI prerequisites with: make doctor"; \
		echo "You can still run core tests with: make test"; \
	fi

test:
	$(CMAKE) -S . -B $(BUILD_DIR) -DBUILD_TESTING=ON -DBUILD_GUI=OFF
	$(CMAKE) --build $(BUILD_DIR) -j
	ctest --test-dir $(BUILD_DIR) --output-on-failure

doctor:
	$(CMAKE) -S . -B $(BUILD_DIR)-doctor -DREQUIRE_OPENGL=ON || true
	@echo "If configure failed above, install OpenGL development libs and re-run make install."

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR) $(BUILD_DIR)-doctor
