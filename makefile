all: build

build:
	(cd build && \
		cmake -DCMAKE_BUILD_TYPE=Debug \
			-DCXXOPTS_BUILD_EXAMPLES=OFF \
			-DBUILD_TESTING=OFF \
			-DLIBUV_BUILD_TESTS=OFF .. && make)

setup_dir:
	rm -rf build
	mkdir -p build

clean_build: setup_dir build

format:
	clang-format -i options.cpp main.cpp

.PHONY: build
