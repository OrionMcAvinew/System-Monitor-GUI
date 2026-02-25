BUILD_DIR ?= build
CMAKE ?= cmake

.PHONY: install configure build run test clean

install:
	./scripts/install_deps.sh

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release

build: configure
	$(CMAKE) --build $(BUILD_DIR) -j

run: build
	@if [ -x "$(BUILD_DIR)/SystemMonitor" ]; then \
		"$(BUILD_DIR)/SystemMonitor"; \
	else \
		echo "SystemMonitor binary not found (likely OpenGL unavailable and GUI was skipped)."; \
		echo "Run: make test"; \
	fi

test:
	$(CMAKE) -S . -B $(BUILD_DIR) -DBUILD_TESTING=ON -DBUILD_GUI=OFF
	$(CMAKE) --build $(BUILD_DIR) -j
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR)
