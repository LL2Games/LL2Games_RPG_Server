#include "Packet.h"
#include "LoginPacketFactory.h"
#include "LoginHandler.h"
#include "RegisterHandler.h"

std::unique_ptr<IPacketHandler> LoginPacketFactory::Create(uint16_t type)
{
    switch(type)
    {
        case PKT_LOGIN: return std::make_unique<LoginHandler>();
        case PKT_REGISTER: return std::make_unique<RegisterHandler>();
    }

    return nullptr;
}