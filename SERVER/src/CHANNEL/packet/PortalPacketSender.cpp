#include "PortalPacketSender.h"
#include "ChannelSession.h"
#include "Packet.h"

int PortalPacketSender::SendMoveSuccess(ChannelSession *session, int destinationMapId, const Vec2 &spawnPosition)
{
    if(session == nullptr)
        return EXIT_FAILURE;
  

    std::vector<std::string> payload;

    payload.push_back(std::to_string(destinationMapId));
    payload.push_back(std::to_string(spawnPosition.xPos));
    payload.push_back(std::to_string(spawnPosition.yPos));

    return session->SendOk(PKT_PORTAL_ENTER, payload);
}

