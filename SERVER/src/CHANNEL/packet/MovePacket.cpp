#include "PlayerHandler.h"
#include "PacketParser.h"
#include "ChannelSession.h"
#include "MapInstance.h"
#include "GameplayGate.h"

void PlayerHandler::MovePacket(PacketContext* ctx) {
    if (ctx && ctx->channel_session)
        ctx->channel_session->SendNok(PKT_PLAYER_MOVE, "use movement input protocol");
}

void PlayerHandler::MovementInputPacket(PacketContext* ctx) {
    Gameplay::Guard guard(Gameplay::gate);
    if (!ctx || !ctx->channel_session) return;
    auto* session = ctx->channel_session;
    auto* player = session->GetPlayer();
    if (!player || !player->GetCurrentMap()) return;
    size_t offset = 0;
    std::string error;
    int mapId = 0, epoch = 0, sequence = 0, horizontal = 0, vertical = 0, jump = 0;
    auto read = [&](int& out) {
        return PacketParser::ParseNextIntField(
            ctx->payload, ctx->payload_len, offset, out, error);
    };
    if (!read(mapId) || !read(epoch) || !read(sequence) || !read(horizontal) ||
        !read(vertical) || !read(jump) ||
        offset != static_cast<size_t>(ctx->payload_len) ||
        sequence <= 0 || (jump != 0 && jump != 1)) {
        session->SendNok(PKT_MOVEMENT_INPUT, "invalid movement input");
        return;
    }
    if (mapId != player->GetMapId() ||
        mapId != static_cast<int>(player->GetCurrentMap()->GetMapId()) ||
        !player->GetCurrentMap()->checkPlayer(player->GetId())) return;
    player->AcceptMovement(epoch, sequence, {horizontal, vertical, jump == 1});
}
