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
OBJ_DIR 			:= $(PROJECT_ROOT)/obj/login
BIN_DIR				:= $(PROJECT_ROOT)/bin
LIB_DIR				:= $(PROJECT_ROOT)/lib
LOG_DIR				:= $(PROJECT_ROOT)/logs/login

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
    -I$(SRC_DIR)/LOGIN/app \
    -I$(SRC_DIR)/LOGIN/db \
    -I$(SRC_DIR)/LOGIN/packet \
    -I$(SRC_DIR)/LOGIN/util

LIB_PATH := \
    -L$(LIB_DIR)/common \
    -L$(LIB_DIR)/mysql \
    -L$(LIB_DIR)/hiredis


# ==================================
# Target
# ==================================
TARGET := $(BIN_DIR)/loginD

# ==================================
# Source Files
# ==================================
SRCS := \
    app/Server.cpp \
    app/Client.cpp \
    \
    packet/LoginPacketFactory.cpp \
    packet/LoginHandler.cpp \
    \
    db/MySQLManager.cpp \
    \
    util/StringUtil.cpp \
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
