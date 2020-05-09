UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
SHARED_EXT := so
endif
ifeq ($(UNAME_S),Darwin)
SHARED_EXT += dylib
endif

.PHONY: dist test clean

SRCS = $(shell find src -type f -name "*.cpp")
TST_SRCS = $(shell find test -type f -name "*.cpp")
OBJS = $(addprefix build/,$(patsubst %.cpp,%.o,$(SRCS)))
TST_OBJS = $(addprefix build/,$(patsubst %.cpp,%.o,$(TST_SRCS)))

dist: $(OBJS)
	mkdir -p dist
	mkdir -p dist/include
	mkdir -p dist/include/stackalloc
	cp src/allocate.h dist/include/stackalloc
	mkdir -p dist/lib
	$(CXX) -shared -fPIC $(LD_FLAGS) -o dist/lib/stackalloc.$(SHARED_EXT) $(OBJS)
	$(AR) -rcs -o dist/lib/stackalloc.a $(OBJS)

test: dist $(TST_OBJS)
	mkdir -p dist/bin
	$(CXX) -o dist/bin/test $(TST_OBJS) dist/lib/stackalloc.$(SHARED_EXT)

clean:
	rm -rf build/

build:
	mkdir -p build
	mkdir -p build/src
	mkdir -p build/test

$(OBJS): build/src/%.o: src/%.cpp | build
	$(CXX) -Wall -fPIC -std=c++17 -DKNOWN_L1_CACHE_LINE_SIZE=64 $(CXX_FLAGS) -o $@ -c $<

$(TST_OBJS): build/test/%.o: test/%.cpp | build
	$(CXX) -Wall -fPIC -std=c++17 -Idist/include $(CXX_FLAGS) -o $@ -c $<
