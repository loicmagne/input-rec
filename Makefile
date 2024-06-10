BUILD_DIR = build

# Targets
all: configure install

# Pull both main and submodules
pull:
	git submodule update --init --recursive
	git submodule foreach git pull origin

# Run CMake configuration
configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_CXX_FLAGS="-Wall"

# Build the project
install:
	sudo cmake --build $(BUILD_DIR) --target install

# Clean the build directory
clean:
	rm -rf $(BUILD_DIR)

# Rebuild the project
rebuild: clean all

.PHONY: all configure build clean rebuild
