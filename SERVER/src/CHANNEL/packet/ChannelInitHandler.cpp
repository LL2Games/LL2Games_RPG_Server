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
    K_LOG_TRACE( "LJH TEST");   
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
        K_LOG_ERROR( "ctx is nullptr\n");
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
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }

    session = ctx->channel_session;
    if(session == nullptr)
    {
        K_LOG_ERROR( "session is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]session is nullptr";
        goto err;
    }


    if(!utility::StringToInt(ch_id, characterId))
    {
        K_LOG_ERROR( "Error_MSG : [%s]", "String to Int 실패");
        rc = EXIT_FAILURE;
        goto err;
    }

    if(!player) {
        errMsg = "[" + std::to_string(rc) + "]HandleChannelAuth: 플레이어 로드 실패 [" + std::to_string(characterId) + "]";
        K_LOG_ERROR( "%s", errMsg.c_str());
        rc = EXIT_FAILURE;
        goto err;
    }

    K_LOG_TRACE( "HandleChannelAuth: 플레이어 로드 성공 [%d]", characterId);
    PlayerService::LoadInventoryMeta(player.get());
    PlayerService::LoadInventory(player.get());
    PlayerService::LoadLearnedSkill(player.get());


    K_LOG_TRACE( "HandleChannelAuth: 플레이어 인벤토리 로드 성공 [%d]", characterId);

    

    K_LOG_DEBUG( "player_manager [%p]", ctx->player_manager);  
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
            K_LOG_TRACE( "HandleChannelAuth: PlayerManager 등록 성공");
            // 이때 클라이언트한테 정보를 보내줘야 한다.

            // 플레이어 정보 보내기
            PlayerPacketSender::SendPlayerInfo(playerPtr);
            PlayerPacketSender::SendPlayerStat(playerPtr);
            PlayerPacketSender::SendPlayerSkillList(playerPtr);

            InventoryPacketSender::SendInventoryMeta(playerPtr);
            InventoryPacketSender::SendInventoryItems(playerPtr);            // 인벤토리 정보도 보내야 한다.    

            QuickSlotPacketSender::SendQuickSlotList(playerPtr);
        } else {
            K_LOG_ERROR( "HandleChannelAuth: PlayerManager 등록 실패, 이미 접속한 플레이어 입니다.");
            rc = EXIT_FAILURE;
            goto err;
        }
    } else {
        K_LOG_ERROR( "HandleChannelAuth: PlayerManager가 null입니다");
        rc = EXIT_FAILURE;
        goto err;
    }
    // 성공 응답
    K_LOG_TRACE( "HandleChannelAuth: 접속 성공");

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

         K_LOG_TRACE( "PKT_CHANNEL_AUTH name[%s]", name.c_str());
        ctx->channel_session->SendOk(PKT_CHANNEL_AUTH, {name});
    }
}
