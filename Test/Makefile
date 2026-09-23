# ==================================
# Project
# ==================================

PROJECT_ROOT := ..

CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Werror

MYSQL_CFLAGS := $(shell mysql_config --cflags)
MYSQL_LIBS := $(shell mysql_config --libs)


# ==================================
# Paths
# ==================================

COMMON_LIB := $(PROJECT_ROOT)/SERVER/lib/common/libcommon.a
CHANNEL_DIR := $(PROJECT_ROOT)/SERVER/src/CHANNEL
HIREDIS_LIB_DIR := $(PROJECT_ROOT)/SERVER/lib/hiredis


# ==================================
# Standalone Test Targets
# ==================================

PACKET_TEST_TARGET := $(PROJECT_ROOT)/SERVER/bin/packet_parser_length_test
PACKET_TEST_SOURCE := cpp/PacketParserLengthTest.cpp

AUTH_TOKEN_TEST_TARGET := $(PROJECT_ROOT)/SERVER/bin/auth_token_generator_test
AUTH_TOKEN_TEST_SOURCE := cpp/AuthTokenGeneratorTest.cpp

AUTH_TICKET_REPLAY_TEST_TARGET := $(PROJECT_ROOT)/SERVER/bin/auth_channel_ticket_replay_test
AUTH_TICKET_REPLAY_TEST_SOURCE := cpp/AuthChannelTicketReplayTest.cpp

PLAYER_STATE_REPOSITORY_TEST_TARGET := $(PROJECT_ROOT)/SERVER/bin/player_state_repository_test
PLAYER_STATE_REPOSITORY_TEST_SOURCE := cpp/PlayerStateRepositoryTest.cpp
PLAYER_STATE_REPOSITORY_IMPLEMENTATION := $(PROJECT_ROOT)/SERVER/src/CHANNEL/db/PlayerStateRepository.cpp


# ==================================
# Channel-linked Test Targets
# ==================================

AUTH_SESSION_TEST_TARGET := $(PROJECT_ROOT)/SERVER/bin/auth_session_regression_test

PLAYER_SAVE_FLOW_TEST_TARGET := $(PROJECT_ROOT)/SERVER/bin/player_save_flow_test

PLAYER_MOVEMENT_VALIDATION_TEST_TARGET := $(PROJECT_ROOT)/SERVER/bin/player_movement_validation_test

TRADE_SERVICE_VALIDATION_TEST_TARGET := $(PROJECT_ROOT)/SERVER/bin/trade_service_validation_test

CHANNEL_LIFECYCLE_TEST_TARGET := $(PROJECT_ROOT)/SERVER/bin/channel_server_lifecycle_test
# ==================================
# Test Tool Targets
# ==================================

CHANNEL_TICKET_MAKER_TARGET := $(PROJECT_ROOT)/SERVER/bin/channel_ticket_test_data_maker
CHANNEL_TICKET_MAKER_SOURCE := cpp/ChannelTicketTestDataMaker.cpp


# ==================================
# Target Groups
# ==================================

STANDALONE_TEST_TARGETS := \
	$(PACKET_TEST_TARGET) \
	$(AUTH_TOKEN_TEST_TARGET) \
	$(AUTH_TICKET_REPLAY_TEST_TARGET) \
	$(PLAYER_STATE_REPOSITORY_TEST_TARGET)

CHANNEL_TEST_TARGETS := \
	$(AUTH_SESSION_TEST_TARGET) \
	$(PLAYER_SAVE_FLOW_TEST_TARGET) \
	$(PLAYER_MOVEMENT_VALIDATION_TEST_TARGET) \
	$(CHANNEL_LIFECYCLE_TEST_TARGET) \
	$(TRADE_SERVICE_VALIDATION_TEST_TARGET)

TEST_TOOL_TARGETS := \
	$(CHANNEL_TICKET_MAKER_TARGET)


# ==================================
# Include Paths
# ==================================

INCLUDES := \
	-I$(PROJECT_ROOT)/SERVER/include \
	-I$(PROJECT_ROOT)/SERVER/include/COMMON \
	-I$(PROJECT_ROOT)/SERVER/include/COMMON/packet \
	-I$(PROJECT_ROOT)/SERVER/include/COMMON/auth \
	-I$(PROJECT_ROOT)/SERVER/include/COMMON/db \
	-I$(PROJECT_ROOT)/SERVER/include/COMMON/config \
	-I$(PROJECT_ROOT)/SERVER/src/CHANNEL/db \
	-I$(PROJECT_ROOT)/SERVER/src/CHANNEL/domain \
	-I$(PROJECT_ROOT)/SERVER/src/CHANNEL/stat \
	-I$(PROJECT_ROOT)/SERVER/src/CHANNEL/util \
	-I$(PROJECT_ROOT)/SERVER/src/CHANNEL/packet \
	-I$(PROJECT_ROOT)/SERVER/src/CHANNEL/map


# ==================================
# Main Targets
# ==================================

.PHONY: all common tools channel-tests packet-test run clean

all: $(STANDALONE_TEST_TARGETS) channel-tests

common:
	$(MAKE) -C $(PROJECT_ROOT)/SERVER/src/COMMON

tools: $(TEST_TOOL_TARGETS)

channel-tests:
	$(MAKE) -C $(CHANNEL_DIR) build-tests


# ==================================
# Standalone Test Build Rules
# ==================================

$(PACKET_TEST_TARGET): $(PACKET_TEST_SOURCE) common
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		$(PACKET_TEST_SOURCE) \
		$(COMMON_LIB) \
		-pthread \
		-o $@

$(AUTH_TOKEN_TEST_TARGET): $(AUTH_TOKEN_TEST_SOURCE) common
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		$(AUTH_TOKEN_TEST_SOURCE) \
		$(COMMON_LIB) \
		-pthread \
		-o $@

$(AUTH_TICKET_REPLAY_TEST_TARGET): \
	$(AUTH_TICKET_REPLAY_TEST_SOURCE) \
	common
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		$(AUTH_TICKET_REPLAY_TEST_SOURCE) \
		$(COMMON_LIB) \
		-L$(HIREDIS_LIB_DIR) \
		-lhiredis \
		-pthread \
		-Wl,-rpath,'$$ORIGIN/../lib/hiredis' \
		-o $@

$(PLAYER_STATE_REPOSITORY_TEST_TARGET): \
	$(PLAYER_STATE_REPOSITORY_TEST_SOURCE) \
	$(PLAYER_STATE_REPOSITORY_IMPLEMENTATION) \
	common
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(MYSQL_CFLAGS) \
		$(PLAYER_STATE_REPOSITORY_TEST_SOURCE) \
		$(PLAYER_STATE_REPOSITORY_IMPLEMENTATION) \
		$(COMMON_LIB) \
		$(MYSQL_LIBS) \
		-pthread \
		-o $@


# ==================================
# Test Tool Build Rules
# ==================================

$(CHANNEL_TICKET_MAKER_TARGET): \
	$(CHANNEL_TICKET_MAKER_SOURCE) \
	common
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		$(CHANNEL_TICKET_MAKER_SOURCE) \
		$(COMMON_LIB) \
		-L$(HIREDIS_LIB_DIR) \
		-lhiredis \
		-pthread \
		-Wl,-rpath,'$$ORIGIN/../lib/hiredis' \
		-o $@


# ==================================
# Run
# ==================================
packet-test: $(PACKET_TEST_TARGET)
	$(PACKET_TEST_TARGET)

run: all
	$(PACKET_TEST_TARGET)
	$(AUTH_TOKEN_TEST_TARGET)
	$(AUTH_TICKET_REPLAY_TEST_TARGET)
	$(PLAYER_STATE_REPOSITORY_TEST_TARGET)
	$(AUTH_SESSION_TEST_TARGET)
	$(PLAYER_SAVE_FLOW_TEST_TARGET)
	$(PLAYER_MOVEMENT_VALIDATION_TEST_TARGET)
	$(CHANNEL_LIFECYCLE_TEST_TARGET)
	$(TRADE_SERVICE_VALIDATION_TEST_TARGET)

# ==================================
# Clean
# ==================================

clean:
	rm -f \
		$(STANDALONE_TEST_TARGETS) \
		$(CHANNEL_TEST_TARGETS) \
		$(TEST_TOOL_TARGETS)
	$(MAKE) -C $(CHANNEL_DIR) fclean