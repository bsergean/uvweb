all: build

full_build:
	(cd build && \
		cmake -GNinja -DCMAKE_BUILD_TYPE=Debug \
			-DCXXOPTS_BUILD_EXAMPLES=OFF \
			-DBUILD_TESTING=OFF \
			-DLIBUV_BUILD_TESTS=OFF .. && ninja)

xcode:
	(cd xcode_build && \
		cmake -GXcode -DCMAKE_BUILD_TYPE=Debug \
			-DCXXOPTS_BUILD_EXAMPLES=OFF \
			-DBUILD_TESTING=OFF \
			-DLIBUV_BUILD_TESTS=OFF .. && open uvweb.xcodeproj)


build:
	(cd build && ninja)

setup_dir:
	rm -rf build
	mkdir -p build

test: build
	sh tools/test.sh

clean_build: setup_dir build

format:
	clang-format -i uvweb/*.cpp uvweb/*.cpp cli/*.cpp cli/*.h

test_compressed_upload:
	ws curl --compress_request -F foo=@test/data/MAINTAINERS.md http://jeanserge.com:8080/

#
# Docker
#
NAME        := ${DOCKER_REPO}/uvweb
TAG         := $(shell python tools/compute_version_from_git.py)
IMG         := ${NAME}:${TAG}
BUILD       := ${NAME}:build
PROD        := ${NAME}:production

docker_tag:
	docker tag ${IMG} ${PROD}
	docker push ${PROD}
	docker push ${IMG}

docker: update_version
	docker build -t ${IMG} .
	docker tag ${IMG} ${BUILD}
	docker tag ${IMG} ${PROD}

docker_push: docker_tag

deploy: docker docker_push

dummy: docker


.PHONY: build
