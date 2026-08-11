# ==================================
# Project Root & Common
# ==================================
PROJECT_ROOT := ../..
include $(PROJECT_ROOT)/common.mk

# ==================================
# Build Config
# ==================================
INC_DIR				:= $(PROJECT_ROOT)/include
SRC_DIR				:= $(PROJECT_ROOT)/src
OBJ_DIR 			:= $(PROJECT_ROOT)/obj/world
BIN_DIR				:= $(PROJECT_ROOT)/bin
LIB_DIR				:= $(PROJECT_ROOT)/lib
LOG_DIR				:= $(PROJECT_ROOT)/logs/world

# ==================================
# Compiler / Linker
# ==================================
CXX          := $(COMMON_CXX)
CXXFLAGS     := $(COMMON_CXXFLAGS)
LFLAGS		 := $(COMMON_LFLAGS)
LDFLAGS      := -pthread \
                -Wl,-rpath=$(LIB_DIR)/mysql \
                -Wl,-rpath=$(LIB_DIR)/hiredis

# ==================================
# Includes / Libs
# ==================================
INCLUDES := \
    -I$(INC_DIR) \
    -I$(INC_DIR)/COMMON \
    -I$(INC_DIR)/COMMON/auth \
    -I$(INC_DIR)/COMMON/packet \
	-I$(INC_DIR)/COMMON/slog \
    -I$(INC_DIR)/COMMON/config \
    -I$(INC_DIR)/COMMON/db \
    \
    -I$(SRC_DIR)/WORLD/app \
    -I$(SRC_DIR)/WORLD/db \
    -I$(SRC_DIR)/WORLD/packet

LIB_PATH := \
    -L$(LIB_DIR)/common \
    -L$(LIB_DIR)/mysql \
    -L$(LIB_DIR)/hiredis

# ==================================
# Target
# ==================================
TARGET := $(BIN_DIR)/worldD

# ==================================
# Source Files
# ==================================
SRCS := \
    app/WorldServer.cpp \
    app/WorldSession.cpp \
    app/CharacterService.cpp \
    app/ChannelManager.cpp \
    \
    packet/WorldPacketFactory.cpp \
    packet/CharacterHandler.cpp \
    packet/WorldInitHandler.cpp \
    packet/ChannelSelectHandler.cpp \
    packet/CharacterNewHandler.cpp \
    packet/CheckDupNickHandler.cpp \
    packet/CharacterDelHandler.cpp \
    \
    main.cpp

# ==================================
# Objects / Deps (경로 미러링)
# ==================================
OBJS := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

# ==================================
# Build Rules
# ==================================
.PHONY: all clean fclean re

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR) $(LOG_DIR)
	@echo " [LD] $@"
	$(CXX) -o $@ $^ $(LIB_PATH) $(LFLAGS) $(LDFLAGS)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo " [CXX] $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@ $(LIB_PATH) $(LFLAGS) $(LDFLAGS)

# ==================================
# Directories
# ==================================
$(BIN_DIR):
	mkdir -p $@

$(LOG_DIR):
	mkdir -p $@

# ==================================
# Clean
# ==================================
clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(TARGET)

re: fclean all

# ==================================
# Dependency Include
# ==================================
-include $(DEPS)
