PROJECT_ROOT := ..

CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Werror

PACKET_TEST_TARGET := $(PROJECT_ROOT)/SERVER/bin/packet_parser_length_test
PACKET_TEST_SOURCE := PacketParserLengthTest.cpp

AUTH_TOKEN_TEST_TARGET := $(PROJECT_ROOT)/SERVER/bin/auth_token_generator_test
AUTH_TOKEN_TEST_SOURCE := AuthTokenGeneratorTest.cpp

AUTH_TICKET_REPLAY_TEST_TARGET := $(PROJECT_ROOT)/SERVER/bin/auth_channel_ticket_replay_test
AUTH_TICKET_REPLAY_TEST_SOURCE := AuthChannelTicketReplayTest.cpp


TEST_TARGETS := \
	$(PACKET_TEST_TARGET) \
	$(AUTH_TOKEN_TEST_TARGET) \
	$(AUTH_TICKET_REPLAY_TEST_TARGET)

COMMON_LIB := $(PROJECT_ROOT)/SERVER/lib/common/libcommon.a
CHANNEL_DIR := $(PROJECT_ROOT)/SERVER/src/CHANNEL
HIREDIS_LIB_DIR := $(PROJECT_ROOT)/SERVER/lib/hiredis

INCLUDES := \
	-I$(PROJECT_ROOT)/SERVER/include \
	-I$(PROJECT_ROOT)/SERVER/include/COMMON \
	-I$(PROJECT_ROOT)/SERVER/include/COMMON/packet \
	-I$(PROJECT_ROOT)/SERVER/include/COMMON/auth \
	-I$(PROJECT_ROOT)/SERVER/include/COMMON/db \
	-I$(PROJECT_ROOT)/SERVER/include/COMMON/config

.PHONY: all common run run-redis-integration clean

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

$(AUTH_TICKET_REPLAY_TEST_TARGET): $(AUTH_TICKET_REPLAY_TEST_SOURCE) common
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		$(AUTH_TICKET_REPLAY_TEST_SOURCE) \
		$(COMMON_LIB) \
		-L$(HIREDIS_LIB_DIR) \
		-lhiredis \
		-pthread \
		-Wl,-rpath,'$$ORIGIN/../lib/hiredis' \
		-o $@

run: $(TEST_TARGETS)
	$(PACKET_TEST_TARGET)
	$(AUTH_TOKEN_TEST_TARGET)
	$(AUTH_TICKET_REPLAY_TEST_TARGET)
	$(MAKE) -C $(CHANNEL_DIR) test-auth-session

clean:
	rm -f $(TEST_TARGETS) $(AUTH_TICKET_REPLAY_TEST_TARGET)
	$(MAKE) -C $(CHANNEL_DIR) fclean