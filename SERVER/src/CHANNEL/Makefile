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
OBJ_DIR 			:= $(PROJECT_ROOT)/obj/channel
BIN_DIR				:= $(PROJECT_ROOT)/bin
LIB_DIR				:= $(PROJECT_ROOT)/lib
LOG_DIR				:= $(PROJECT_ROOT)/logs/channel

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
	-I$(INC_DIR)/COMMON/command \
	-I$(INC_DIR)/COMMON/config \
	-I$(INC_DIR)/COMMON/ipc \
	-I$(INC_DIR)/COMMON/thread \
	-I$(INC_DIR)/COMMON/util \
	-I$(INC_DIR)/COMMON/db \
	\
	-I$(SRC_DIR)/CHANNEL/app \
    -I$(SRC_DIR)/CHANNEL/db \
	-I$(SRC_DIR)/CHANNEL/domain \
	-I$(SRC_DIR)/CHANNEL/ipc \
	-I$(SRC_DIR)/CHANNEL/manager \
	-I$(SRC_DIR)/CHANNEL/map \
    -I$(SRC_DIR)/CHANNEL/packet \
	-I$(SRC_DIR)/CHANNEL/service \
	-I$(SRC_DIR)/CHANNEL/stat \
	-I$(SRC_DIR)/CHANNEL/util \

LIB_PATH := \
    -L$(LIB_DIR)/common \
    -L$(LIB_DIR)/mysql \
    -L$(LIB_DIR)/hiredis

# ==================================
# Target
# ==================================
TARGET := $(BIN_DIR)/channelD

# ==================================
# Source Files
# ==================================
SRCS := \
	app/ChannelServer.cpp \
	app/ChannelSession.cpp \
	app/ChannelAuthTask.cpp \
    \
	db/PlayerService.cpp \
	db/RedisUtility.cpp \
	\
	domain/Player.cpp \
	domain/Monster.cpp \
	domain/Item.cpp \
	domain/Inventory.cpp \
	domain/Projectile.cpp \
	\
	ipc/CommandReceiver.cpp \
	ipc/CommandMsgQueue.cpp \
	\
	manager/PlayerManager.cpp \
	manager/MonsterManager.cpp \
	manager/ItemManager.cpp \
	manager/InventoryManager.cpp\
	manager/QuickSlotManager.cpp\
	manager/SkillManager.cpp \
	manager/DropManager.cpp\
	manager/ProjectileManager.cpp \
	manager/LevelManager.cpp\
	\
	map/MapManager.cpp \
	map/MapInstance.cpp \
	\
	packet/ChannelPacketFactory.cpp \
	packet/ChannelInitHandler.cpp \
	packet/PlayerHandler.cpp \
	packet/MapHandler.cpp \
	packet/HandleStat.cpp \
	packet/StatPacketFactory.cpp \
	packet/MovePacket.cpp \
	packet/AttackPacket.cpp \
	packet/OnDamagedPacket.cpp \
	packet/UseItemPacket.cpp \
	packet/PlayerPacketSender.cpp\
	packet/InventoryPacketSender.cpp\
	packet/QuickSlotPacketSender.cpp\
	packet/QuickSlotPacketHandler.cpp\
	packet/MonsterPacketSender.cpp\
	packet/ItemPacketSender.cpp\
	packet/HandleTrade.cpp \
	packet/InventoryPacketHandler.cpp\
	packet/PacketProcessTask.cpp \
	packet/PortalHandler.cpp\
	packet/PortalPacketSender.cpp\
	\
	service/MapService.cpp \
	service/StatService.cpp \
	service/CombatService.cpp \
	service/ItemService.cpp \
	service/TradeService.cpp \
	\
	stat/PlayerStatRepository.cpp \
	stat/CharacterStat.cpp \
	\
	util/Math.cpp \
	\
    main.cpp

# ==================================
# Objects / Deps (경로 미러링)
# ==================================
OBJS := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

AUTH_TEST_TARGET := $(BIN_DIR)/auth_session_regression_test
AUTH_TEST_SRC := $(PROJECT_ROOT)/../Test/AuthSessionRegressionTest.cpp
AUTH_TEST_OBJ := $(OBJ_DIR)/test/AuthSessionRegressionTest.o
AUTH_TEST_OBJS := $(filter-out $(OBJ_DIR)/main.o,$(OBJS))
# ==================================
# Build Rules
# ==================================
.PHONY: all clean fclean re test

all: $(TARGET)

test-auth-session: $(AUTH_TEST_TARGET)
	$(AUTH_TEST_TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR) $(LOG_DIR)
	@echo " [LD] $@"
	$(CXX) -o $@ $^ $(LIB_PATH) $(LFLAGS) $(LDFLAGS)

$(AUTH_TEST_TARGET): $(AUTH_TEST_OBJS) $(AUTH_TEST_OBJ) | $(BIN_DIR) $(LOG_DIR)
	@echo " [LD] $@"
	$(CXX) -o $@ $^ $(LIB_PATH) $(LFLAGS) $(LDFLAGS)
	
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo " [CXX] $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@ $(LIB_PATH) $(LFLAGS) $(LDFLAGS) -DGLOBAL

$(AUTH_TEST_OBJ): $(AUTH_TEST_SRC)
	@mkdir -p $(dir $@)
	@echo " [CXX] $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -DGLOBAL -c $< -o $@

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
	rm -f $(TARGET) $(AUTH_TEST_TARGET)

re: fclean all

# ==================================
# Dependency Include
# ==================================
-include $(DEPS)

