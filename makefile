all: build

full_build:
	(cd build && \
		cmake -GNinja -DCMAKE_BUILD_TYPE=Debug \
			-DCXXOPTS_BUILD_EXAMPLES=OFF \
			-DBUILD_TESTING=OFF \
			-DLIBUV_BUILD_TESTS=OFF .. && ninja)

xcode:
	(cd build && \
		cmake -GXcode -DCMAKE_BUILD_TYPE=Debug \
			-DCXXOPTS_BUILD_EXAMPLES=OFF \
			-DBUILD_TESTING=OFF \
			-DLIBUV_BUILD_TESTS=OFF .. && open uvweb.xcodeproj)


build:
	(cd build && ninja)

setup_dir:
	rm -rf build
	mkdir -p build

clean_build: setup_dir build

format:
	clang-format -i uvweb/*.cpp uvweb/*.cpp cli/*.cpp cli/*.h

test_compressed_upload:
	ws curl --compress_request -F foo=@test/data/MAINTAINERS.md http://jeanserge.com:8080/

.PHONY: build
