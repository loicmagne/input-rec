BUILD_DIR = Build

# Targets
all: configure install

# Run CMake configuration
configure:
	cmake -S . -B $(BUILD_DIR)

# Build the project
install:
	cmake --build $(BUILD_DIR) --target install

# Clean the build directory
clean:
	rm -rf $(BUILD_DIR)

# Rebuild the project
rebuild: clean all

.PHONY: all configure build clean rebuild
