#include "Packet.h"
#include "WorldPacketFactory.h"
#include "WorldInitHandler.h"
#include "CharacterHandler.h"
#include "ChannelSelectHandler.h"
#include "CharacterNewHandler.h"
#include "CheckDupNickHandler.h"

std::unique_ptr<IPacketHandler> WorldPacketFactory::Create(uint16_t type)
{
    switch(type)
    {
        case PKT_INIT_WORLD: return std::make_unique<WorldInitHandler>();
        case PKT_SELECT_CHARACTER: return std::make_unique<CharacterHandler>();
        case PKT_SELECT_CHANNEL: return std::make_unique<ChannelSelectHandler>();
        case PKT_NEW_CHARACTER: return std::make_unique<CharacterNewHandler>();
        case PKT_CHECK_DUP_CHAR: return std::make_unique<CheckDupNickHandler>();
    }

    return nullptr;
}