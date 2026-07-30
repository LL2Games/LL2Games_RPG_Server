#pragma once
#include "common.h"
#include "CommonEnum.h"
class channelSession;

class PortalPacketSender
{
public:
    static int SendMoveSuccess(ChannelSession* session, int destinationMapId,const Vec2& spawnPosition);
   
};