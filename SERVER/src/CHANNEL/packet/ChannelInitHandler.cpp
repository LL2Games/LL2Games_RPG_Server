#include "common.h"
#include "ChannelInitHandler.h"
#include "ChannelSession.h"
#include "PlayerService.h"
#include "Player.h"
#include "PlayerManager.h"
#include "PacketParser.h"
#include "PlayerPacketSender.h"
#include "InventoryPacketSender.h"
#include "QuickSlotPacketSender.h"
#include "LevelManager.h"
#include "K_slog.h"
#include "utility.h"
#include <sstream>



void ChannelInitHandler::Execute(PacketContext* ctx)
{
    // 임시로 인증 처리
    K_slog_trace(K_SLOG_TRACE, " [%s][%d] LJH TEST", __FUNCTION__ , __LINE__);   
    HandleChannelAuth(ctx);
}

void ChannelInitHandler::HandleChannelAuth(PacketContext *ctx)
{
    ChannelSession* session;
    std::unique_ptr<Player> player;
    int rc = EXIT_SUCCESS;
    std::string errMsg;
    size_t offset = 0;
    int characterId =0;
    std::string ch_id;
    
    if(ctx == nullptr)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s][%d] ctx is nullptr\n", __FILE__, __FUNCTION__, __LINE__);
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]ctx is nullptr";
        goto err;
    }

     // 받은 정보에서 playerID 추출 
    if(!PacketParser::ParseLengthPrefixedString(
        ctx->payload,
        ctx->payload_len,
        offset,
        ch_id,
        errMsg
    ))
    {
        rc = EXIT_FAILURE;
        K_slog_trace(K_SLOG_ERROR, "[%s : %s][%d] ParseLengthPrefixedString fail", __FILE__, __FUNCTION__, __LINE__);
        goto err;
    }

    session = ctx->channel_session;
    if(session == nullptr)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s][%d] session is nullptr\n", __FILE__, __FUNCTION__, __LINE__);
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]session is nullptr";
        goto err;
    }


    if(!utility::StringToInt(ch_id, characterId))
    {
        K_slog_trace(K_SLOG_ERROR, "[%d][%s] Error_MSG : [%s]", __LINE__, __FUNCTION__, "String to Int 실패");
        rc = EXIT_FAILURE;
        goto err;
    }

    if(!player) {
        errMsg = "[" + std::to_string(rc) + "]HandleChannelAuth: 플레이어 로드 실패 [" + std::to_string(characterId) + "]";
        K_slog_trace(K_SLOG_ERROR, "[%d][%s]%s", __LINE__, __FUNCTION__, errMsg.c_str());
        rc = EXIT_FAILURE;
        goto err;
    }

    K_slog_trace(K_SLOG_TRACE, "HandleChannelAuth: 플레이어 로드 성공 [%d]", characterId);
    PlayerService::LoadInventoryMeta(player.get());
    PlayerService::LoadInventory(player.get());
    PlayerService::LoadLearnedSkill(player.get());


    K_slog_trace(K_SLOG_TRACE, "HandleChannelAuth: 플레이어 인벤토리 로드 성공 [%d]", characterId);

    

    K_slog_trace(K_SLOG_DEBUG, " [%s][%d] player_manager [%p]", __FUNCTION__ , __LINE__, ctx->player_manager);  
    // PlayerManager에 등록 (안전 체크)
    if (ctx->player_manager) {
        Player* playerPtr = player.get();
        bool success = ctx->player_manager->AddPlayer(std::move(player));
        if (success) {

            // 세션에 플레이어 연결
            ctx->channel_session->SetPlayer(playerPtr);
            // 세션에 플레이어 매니저 연결
            ctx->channel_session->SetPlayerManager(ctx->player_manager);
            playerPtr->SetSession(ctx->channel_session);
            K_slog_trace(K_SLOG_TRACE, "HandleChannelAuth: PlayerManager 등록 성공");
            // 이때 클라이언트한테 정보를 보내줘야 한다.

            // 플레이어 정보 보내기
            PlayerPacketSender::SendPlayerInfo(playerPtr);
            PlayerPacketSender::SendPlayerStat(playerPtr);
            PlayerPacketSender::SendPlayerSkillList(playerPtr);

            InventoryPacketSender::SendInventoryMeta(playerPtr);
            InventoryPacketSender::SendInventoryItems(playerPtr);            // 인벤토리 정보도 보내야 한다.    

            QuickSlotPacketSender::SendQuickSlotList(playerPtr);
        } else {
            K_slog_trace(K_SLOG_ERROR, "HandleChannelAuth: PlayerManager 등록 실패, 이미 접속한 플레이어 입니다.");
            rc = EXIT_FAILURE;
            goto err;
        }
    } else {
        K_slog_trace(K_SLOG_ERROR, "HandleChannelAuth: PlayerManager가 null입니다");
        rc = EXIT_FAILURE;
        goto err;
    }
    // 성공 응답
    K_slog_trace(K_SLOG_TRACE, "HandleChannelAuth: 접속 성공");

err:
    if (rc != EXIT_SUCCESS) {
        ctx->channel_session->SendNok(PKT_CHANNEL_AUTH, errMsg);
    } else {
        std::string name = "default";
        Player* playerPtr = ctx->channel_session->GetPlayer();
        if(playerPtr)
        {
            name = playerPtr->GetName();
        }

         K_slog_trace(K_SLOG_TRACE, "[%s][%d] PKT_CHANNEL_AUTH name[%s]", __FUNCTION__, __LINE__, name.c_str());
        ctx->channel_session->SendOk(PKT_CHANNEL_AUTH, {name});
    }
}
