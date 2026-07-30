#include "PlayerHandler.h"
#include "common.h"
#include "PacketParser.h"
#include "ChannelSession.h"
#include "CombatService.h"
#include "ChannelServer.h"
#include "Player.h"
#include "utility.h"
#include "PlayerPacketSender.h"

 // 플레이어가 공격 했을 때 피격 당하는 몬스터

// 플레이어가 공격을 했을 때 클라이언트에서 피격 여부를 판정하고 누가 피격당했는지 어떤 스킬 및 공격을 수행했는지에 대해서 정보를 전달하는 것이 아니라
// 플레이어가 공격을 했다라는 정보만을 가지고 서버에서 피격 판정 여부와 공격 가능 여부를 판단해서 같은 맵의 플레이어들한테 정보 전달

//gunoo22 260729 스킬 사용부분 확인
void PlayerHandler::SkillAttackPacket(PacketContext * ctx)
{   
    CombatService *combat_service = nullptr;
    ChannelSession *session = nullptr;
    Player *player = nullptr;

    size_t offset = 0;
    int rc = EXIT_SUCCESS;

    std::string skill_id_str;
    std::string attack_dir_str;
    std::string errMsg;

    int skill_id = 0;
    int attack_dir = 0;
    int result = 0;

    if(ctx == nullptr)
    {
        K_LOG_ERROR( "ctx is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]ctx is nullptr";
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

    player = session->GetPlayer();
    if(player == nullptr)
    {
     K_LOG_ERROR( "player is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]player is nullptr";
        goto err;
    }

    combat_service = ctx->combat_service;
    if(combat_service == nullptr)
    {
     K_LOG_ERROR( "combat_service is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]combat_service is nullptr";
        goto err;
    }

    // skill_id를 추출
     if(!PacketParser::ParseLengthPrefixedString(
        ctx->payload,
        ctx->payload_len,
        offset,
        skill_id_str,
        errMsg
    ))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }

     // 공격 방향을 추출
     if(!PacketParser::ParseLengthPrefixedString(
        ctx->payload,
        ctx->payload_len,
        offset,
        attack_dir_str,
        errMsg
    ))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }

    if(!utility::StringToInt(skill_id_str, skill_id)) 
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "StringToInt fail");
        goto err;
    }

    if(!utility::StringToInt(attack_dir_str, attack_dir)) 
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "StringToInt fail");
        goto err;
    }
    result = combat_service->HandleSkillAttack(player, skill_id, attack_dir);

    if(result != 1)
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "HandleSkillAttack fail");
        goto err;
    }
    
    
err:
    if (rc != EXIT_SUCCESS) { 
        session->SendNok(PKT_PLAYER_SKILL_ATTACK, errMsg);
    } else {
        session->SendOk(PKT_PLAYER_SKILL_ATTACK, {std::to_string(skill_id)});

        K_LOG_TRACE( "Player Attack End");
        PlayerPacketSender::SendPlayerAttack(player,skill_id,attack_dir,player->GetCurrentMap()->GetPlayerList());
    }

}

void PlayerHandler::BasicAttackPacket(PacketContext * ctx)
{
    CombatService *combat_service = nullptr;
    ChannelSession *session = nullptr;
    Player *player = nullptr;

    size_t offset = 0;
    int result = 0;
    int rc = EXIT_SUCCESS;

    std::string attack_dir_str;
    std::string errMsg;

    int attack_dir = 0;
    
     K_LOG_DEBUG( "BasicAttackPacket Start\n");

    if(ctx == nullptr)
    {
        K_LOG_ERROR( "ctx is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]ctx is nullptr";
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

    player = session->GetPlayer();
    if(player == nullptr)
    {
     K_LOG_ERROR( "player is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]player is nullptr";
        goto err;
    }

    combat_service = ctx->combat_service;
    if(combat_service == nullptr)
    {
     K_LOG_ERROR( "combat_service is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]combat_service is nullptr";
        goto err;
    }

     // 공격 방향을 추출
     if(!PacketParser::ParseLengthPrefixedString(
        ctx->payload,
        ctx->payload_len,
        offset,
        attack_dir_str,
        errMsg
    ))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }

    if(!utility::StringToInt(attack_dir_str, attack_dir))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "attack_dir String to Int Error\n");
        goto err;
    }

    K_LOG_TRACE( "HandleBasicAttack \n");

    result = combat_service->HandleBasicAttack(player, attack_dir);
    if(result != 1)
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "HandleBasicAttack fail");
        goto err;
    }
err:
    if (rc != EXIT_SUCCESS) { 
        session->SendNok(PKT_PLAYER_BASIC_ATTACK, errMsg);
    } else {
        K_LOG_TRACE( "Player Attack End");
        PlayerPacketSender::SendPlayerAttack(player,static_cast<int>(player->GetWeaponType()),attack_dir,player->GetCurrentMap()->GetPlayerList());
    }
}



int CheckHit()
{
    return 0;
}

