PROJECT_ROOT := ..

CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Werror

PACKET_TEST_TARGET := $(PROJECT_ROOT)/SERVER/bin/packet_parser_length_test
PACKET_TEST_SOURCE := PacketParserLengthTest.cpp
COMMON_LIB := $(PROJECT_ROOT)/SERVER/lib/common/libcommon.a
CHANNEL_DIR := $(PROJECT_ROOT)/SERVER/src/CHANNEL

INCLUDES := \
	-I$(PROJECT_ROOT)/SERVER/include \
	-I$(PROJECT_ROOT)/SERVER/include/COMMON \
	-I$(PROJECT_ROOT)/SERVER/include/COMMON/packet

.PHONY: all common run clean

all: $(PACKET_TEST_TARGET)

common:
	$(MAKE) -C $(PROJECT_ROOT)/SERVER/src/COMMON

$(PACKET_TEST_TARGET): $(PACKET_TEST_SOURCE) common
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		$(PACKET_TEST_SOURCE) $(COMMON_LIB) -pthread -o $@

run: $(PACKET_TEST_TARGET)
	$(PACKET_TEST_TARGET)
	$(MAKE) -C $(CHANNEL_DIR) test-auth-session

clean:
	rm -f $(PACKET_TEST_TARGET)
	$(MAKE) -C $(CHANNEL_DIR) fclean