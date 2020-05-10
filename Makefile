UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
SHARED_EXT := so
endif
ifeq ($(UNAME_S),Darwin)
SHARED_EXT += dylib
endif

ifneq (, $(shell which getconf))
KNOWN_CACHE_LINE_SIZE := $(shell getconf LEVEL1_DCACHE_LINESIZE)
endif

ifdef KNOWN_CACHE_LINE_SIZE
CACHE_FLAGS := -DKNOWN_CACHE_LINE_SIZE=$(KNOWN_CACHE_LINE_SIZE)
else
CACHE_FLAGS :=
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
	$(CXX) -shared -fPIC $(LDFLAGS) -o dist/lib/stackalloc.$(SHARED_EXT) $(OBJS)
	$(AR) -rcs -o dist/lib/stackalloc.a $(OBJS)

test: dist $(TST_OBJS)
	mkdir -p dist/bin
	$(CXX) -o dist/bin/test $(LDFLAGS) $(TST_OBJS) dist/lib/stackalloc.a

clean:
	rm -rf build/

build:
	mkdir -p build
	mkdir -p build/src
	mkdir -p build/test

$(OBJS): build/src/%.o: src/%.cpp | build
	$(CXX) -Wall -fPIC -std=c++17 $(CACHE_FLAGS) $(CXXFLAGS) -o $@ -c $<

$(TST_OBJS): build/test/%.o: test/%.cpp | build
	$(CXX) -Wall -fPIC -std=c++17 $(CACHE_FLAGS) -Idist/include $(CXXFLAGS) -o $@ -c $<
