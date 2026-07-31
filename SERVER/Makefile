# ==================================
# Build Config
# ==================================
PROJECT_ROOT		:= .

LIB_DIR 			:= $(PROJECT_ROOT)/lib
BIN_DIR				:= $(PROJECT_ROOT)/bin

MAIND				:= $(BIN_DIR)/mainD
LOGIND				:= $(BIN_DIR)/loginD
CHATD				:= $(BIN_DIR)/chatD
WORLDD				:= $(BIN_DIR)/worldD
CHANNELD			:= $(BIN_DIR)/channelD
LIB_COMMON  		:= $(LIB_DIR)/libcommon.a

TARGETS				:= $(LIB_COMMON) $(MAIND) $(LOGIND) $(CHATD) $(WORLDD) $(CHANNELD) 

# ==================================
# Directory Structure
# ==================================
OBJ_DIR_COMMON		= obj/common
OBJ_DIR_MAIN		= obj/main
OBJ_DIR_LOGIN		= obj/login
OBJ_DIR_CHAT		= obj/chat
OBJ_DIR_WORLD		= obj/world
OBJ_DIR_CHANNEL		= obj/channel

# ==================================
# Build Rules
# ==================================

all: $(TARGETS)

$(TARGETS) : common main login chat world channel

common: 
	@echo "===== Build COMMON ====="
	@$(MAKE) -C src/COMMON

main :
	@echo "===== Build MAIN Server ====="
	@$(MAKE) -C src/MAIN

login :
	@echo "===== Build LOGIN Server ====="
	@$(MAKE) -C src/LOGIN

chat :
	@echo "===== Build CHAT Server ====="
	@$(MAKE) -C src/CHAT

world: 
	@echo "===== Build WORLD Server ====="
	@$(MAKE) -C src/WORLD

channel: 
	@echo "===== Build CHANNEL Server ====="
	@$(MAKE) -C src/CHANNEL

test:
	@echo "===== Run Regression Tests ====="
	@$(MAKE) -C ../Test run

clean:
	rm -rf $(OBJ_DIR_COMMON) $(OBJ_DIR_MAIN) $(OBJ_DIR_LOGIN) $(OBJ_DIR_CHAT) $(OBJ_DIR_WORLD) $(OBJ_DIR_CHANNEL)

fclean: clean
	rm -rf $(TARGETS)

re : fclean all

 .PHONY: all clean fclean re common main login chat world channel test
