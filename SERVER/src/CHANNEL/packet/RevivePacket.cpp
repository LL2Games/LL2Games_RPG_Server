#include "PlayerHandler.h"
#include "ChannelSession.h"
#include "MapInstance.h"
#include "GameplayGate.h"

void PlayerHandler::HandleRevivePacket(PacketContext* ctx)
{
    Gameplay::Guard guard(Gameplay::gate);
    if (!ctx || !ctx->channel_session) return;
    auto* session = ctx->channel_session;
    auto* player = session->GetPlayer();
    auto* map = player ? player->GetCurrentMap() : nullptr;
    if (ctx->payload_len != 0 || !map || !map->RevivePlayer(player))
    {
        session->SendNok(PKT_PLAYER_REVIVE, "cannot revive");
        return;
    }
    session->SendOk(PKT_PLAYER_REVIVE);
}
