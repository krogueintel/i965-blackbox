# Usage:
# LD_PRELOAD=/full/path/i965_batchbuffer_log_all.so execute command

CXX ?= g++
BATCHBUFFER_LOGGER_INSTALL_PATH ?= /opt/mesa.instrumentation

LOGGER_INC = $(BATCHBUFFER_LOGGER_INSTALL_PATH)/include
LOGGER_LIB_DIR = $(BATCHBUFFER_LOGGER_INSTALL_PATH)/lib

CXXFLAGS = -g -Wall -I$(LOGGER_INC) -std=c++11
LIBS = -L$(LOGGER_LIB_DIR) -li965_batchbuffer_logger -Wl,-z,defs

SRCS = i965-blackbox.cpp
OBJS = $(patsubst %.cpp, build/%.o, $(SRCS))

GEN_SRCS = generate_stuff.cpp

i965-blackbox.cpp.so: $(OBJS)
	$(CXX) -shared -Wl,-soname,i965-blackbox -o i965-blackbox.so $(OBJS) $(LIBS)

generate_stuff: build/generate_stuff.o
	$(CXX) $(CXXFLAGS) -o generate_stuff build/generate_stuff.o -ltinyxml

build/function_macros.inc: generate_stuff
	./generate_stuff gl.xml > $@

build/i965-blackbox.o: build/function_macros.inc

build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

clean:
	rm -fr build i965-blackbox.so generate_stuff

