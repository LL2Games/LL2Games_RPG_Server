PROJECT_ROOT := ..

CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Werror

TARGET := $(PROJECT_ROOT)/SERVER/bin/packet_parser_length_test
SOURCE := PacketParserLengthTest.cpp
COMMON_LIB := $(PROJECT_ROOT)/SERVER/lib/common/libcommon.a

INCLUDES := \
	-I$(PROJECT_ROOT)/SERVER/include \
	-I$(PROJECT_ROOT)/SERVER/include/COMMON \
	-I$(PROJECT_ROOT)/SERVER/include/COMMON/packet

.PHONY: all common run clean

all: $(TARGET)

common:
	$(MAKE) -C $(PROJECT_ROOT)/SERVER/src/COMMON

$(TARGET): $(SOURCE) common
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCE) $(COMMON_LIB) -pthread -o $@

run: $(TARGET)
	$(TARGET)

clean:
	rm -f $(TARGET)
