PROJECT_ROOT := ..

CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Werror

PACKET_TEST_TARGET := $(PROJECT_ROOT)/SERVER/bin/packet_parser_length_test
PACKET_TEST_SOURCE := PacketParserLengthTest.cpp

AUTH_TOKEN_TEST_TARGET := $(PROJECT_ROOT)/SERVER/bin/auth_token_generator_test
AUTH_TOKEN_TEST_SOURCE := AuthTokenGeneratorTest.cpp

TEST_TARGETS := \
	$(PACKET_TEST_TARGET) \
	$(AUTH_TOKEN_TEST_TARGET)

COMMON_LIB := $(PROJECT_ROOT)/SERVER/lib/common/libcommon.a
CHANNEL_DIR := $(PROJECT_ROOT)/SERVER/src/CHANNEL

INCLUDES := \
	-I$(PROJECT_ROOT)/SERVER/include \
	-I$(PROJECT_ROOT)/SERVER/include/COMMON \
	-I$(PROJECT_ROOT)/SERVER/include/COMMON/packet \
	-I$(PROJECT_ROOT)/SERVER/include/COMMON/auth

.PHONY: all common run clean

all: $(TEST_TARGETS)

common:
	$(MAKE) -C $(PROJECT_ROOT)/SERVER/src/COMMON

$(PACKET_TEST_TARGET): $(PACKET_TEST_SOURCE) common
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		$(PACKET_TEST_SOURCE) $(COMMON_LIB) -pthread -o $@

$(AUTH_TOKEN_TEST_TARGET): $(AUTH_TOKEN_TEST_SOURCE) common
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		$(AUTH_TOKEN_TEST_SOURCE) $(COMMON_LIB) -pthread -o $@

run: $(TEST_TARGETS)
	$(PACKET_TEST_TARGET)
	$(AUTH_TOKEN_TEST_TARGET)
	$(MAKE) -C $(CHANNEL_DIR) test-auth-session

clean:
	rm -f $(TEST_TARGETS)
	$(MAKE) -C $(CHANNEL_DIR) fclean