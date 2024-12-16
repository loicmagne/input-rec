# Targets
all: configure build install

# Pull both main and submodules
pull:
	git submodule update --init --recursive
	git submodule foreach git pull origin

# Run CMake configuration
configure:
	cmake --preset linux-x86_64 -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu

build:
	cmake --build --preset linux-x86_64

# Build the project
install:
	sudo cmake --install build_x86_64

# Clean the build directory
clean:
	rm -rf $(BUILD_DIR)

# Rebuild the project
rebuild: clean all

.PHONY: all configure build clean rebuild
