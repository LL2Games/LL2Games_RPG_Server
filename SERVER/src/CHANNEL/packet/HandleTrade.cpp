#include "PlayerHandler.h"
#include "ChannelSession.h"
#include "Player.h"
#include "PacketParser.h"
#include "PlayerManager.h"
#include "TradeService.h"
#include "PlayerService.h"

void PlayerHandler::HandleTradeRequest(PacketContext* ctx)
{
    int rc = EXIT_SUCCESS;
    ChannelSession *session = nullptr;
    Player *player = nullptr;
    Player *target_player = nullptr;
    size_t offset = 0;
    std::string target_player_name;
    PlayerManager* player_manager = nullptr;
    TradeService* trade_service = nullptr;
    std::string errMsg;

    if (ctx == nullptr)
    {
        K_LOG_ERROR( "ctx is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]ctx is nullptr";
        goto err;
    }
    session = ctx->channel_session;
    if (session == nullptr)
    {
        K_LOG_ERROR( "session is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]session is nullptr";
        goto err;
    }

    player = session->GetPlayer();
    if (player == nullptr)
    {
        K_LOG_ERROR( "player is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]player is nullptr";
        goto err;
    }

    player_manager = ctx->player_manager;
    if (player_manager == nullptr)
    {
        K_LOG_ERROR( "player_manager is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]player_manager is nullptr";
        goto err;
    }

    trade_service = ctx->trade_service;
    if (trade_service == nullptr)
    {
        K_LOG_ERROR( "trade_service is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]trade_service is nullptr";
        goto err;
    }

    //교환신청 상대방 Player Name 추출
    if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            target_player_name,
            errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }

    //교환신청 상대방 Player 객체 추출
    target_player = player_manager->GetPlayer(target_player_name);
    if (target_player == nullptr)
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "target_player is nullptr\n");
        errMsg = "[" + std::to_string(rc) + "]target_player is nullptr";
        goto err;
    }

    rc = trade_service->Request(player, target_player, errMsg);

err:

if (rc != EXIT_SUCCESS)
    {
        session->SendNok(PKT_TRADE_REQUEST, errMsg);
    }
    else
    {
        session->SendOk(PKT_TRADE_REQUEST);
    }

}

void PlayerHandler::HandleTradeAccept(PacketContext* ctx)
{
    int rc = EXIT_SUCCESS;
    ChannelSession *session = nullptr;
    Player *player = nullptr;
    Player *request_player = nullptr;
    size_t offset = 0;
    std::string request_player_id;
    PlayerManager* player_manager = nullptr;
    TradeService* trade_service = nullptr;
    std::string errMsg;
    K_LOG_DEBUG( "gunoo22_TEST");
    if (ctx == nullptr)
    {
        K_LOG_ERROR( "ctx is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]ctx is nullptr";
        goto err;
    }
    session = ctx->channel_session;
    if (session == nullptr)
    {
        K_LOG_ERROR( "session is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]session is nullptr";
        goto err;
    }

    player = session->GetPlayer();
    if (player == nullptr)
    {
        K_LOG_ERROR( "player is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]player is nullptr";
        goto err;
    }

    player_manager = ctx->player_manager;
    if (player_manager == nullptr)
    {
        K_LOG_ERROR( "player_manager is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]player_manager is nullptr";
        goto err;
    }

    trade_service = ctx->trade_service;
    if (trade_service == nullptr)
    {
        K_LOG_ERROR( "trade_service is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]trade_service is nullptr";
        goto err;
    }
    K_LOG_DEBUG( "gunoo22_TEST");

    //교환신청 상대방 Player Name 추출
    if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            request_player_id,
            errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }
    K_LOG_DEBUG( "gunoo22_TEST request_player_id[%s]", request_player_id.c_str());

    //교환신청 요청 Player 객체 추출
    request_player = player_manager->GetPlayer(atoi(request_player_id.c_str()));
    if (request_player == nullptr)
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "request_player is nullptr\n");
        errMsg = "[" + std::to_string(rc) + "]request_player is nullptr";
        goto err;
    }
    K_LOG_DEBUG( "gunoo22_TEST");

    rc = trade_service->Start(request_player, player, errMsg);
    K_LOG_DEBUG( "gunoo22_TEST");

err:

if (rc != EXIT_SUCCESS)
    {
        session->SendNok(PKT_TRADE_ACCEPT, errMsg);
    }
    else
    {
        K_LOG_DEBUG( "gunoo22_TEST SUCCESS");

        //성공시에는 교환 실행 패킷이 trade_service->Start에서 갈 예정
        //session->SendOk(PKT_TRADE_ACCEPT);
    }

}


void PlayerHandler::HandleTradeReady(PacketContext* ctx)
{
    int rc = EXIT_SUCCESS;
    ChannelSession *session = nullptr;
    Player *player = nullptr;
    std::string target_player_id;
    Player *target_player = nullptr;
    PlayerManager* player_manager = nullptr;
    size_t offset = 0;
    std::vector<TradeItem> trade_items;
    TradeService* trade_service = nullptr;
    std::string errMsg;
    PlayerService* player_service = nullptr;

    if (ctx == nullptr)
    {
        K_LOG_ERROR( "ctx is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]ctx is nullptr";
        goto err;
    }
    session = ctx->channel_session;
    if (session == nullptr)
    {
        K_LOG_ERROR( "session is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]session is nullptr";
        goto err;
    }

    player = session->GetPlayer();
    if (player == nullptr)
    {
        K_LOG_ERROR( "player is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]player is nullptr";
        goto err;
    }

    player_manager = ctx->player_manager;
    if (player_manager == nullptr)
    {
        K_LOG_ERROR( "player_manager is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]player_manager is nullptr";
        goto err;
    }

    player_service = ctx->player_service;
    if (player_service == nullptr)
    {
        K_LOG_ERROR( "player_service is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]player_service is nullptr";
        goto err;
    }

    trade_service = ctx->trade_service;
    if (trade_service == nullptr)
    {
        K_LOG_ERROR( "trade_service is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]trade_service is nullptr";
        goto err;
    }

    //교환신청 상대방 Player id추출
    if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            target_player_id,
            errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }

    //교환신청 요청 Player 객체 추출
    target_player = player_manager->GetPlayer(atoi(target_player_id.c_str()));
    if (target_player == nullptr)
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "target_player is nullptr\n");
        errMsg = "[" + std::to_string(rc) + "]target_player is nullptr";
        goto err;
    }

    //Item추출
    //반복하여 ItemId, amount 추출
#if 0 //gunoo22 260528 AddItem에서 아이템 업로드로 변경
    while (1)
    {
        std::string item_id;
        std::string item_amount;

        K_LOG_DEBUG( "gunoo22_TEST");

        //item_id
        if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            item_id,
            errMsg))
        {
            //더이상 없으면 중단
                    K_LOG_DEBUG( "gunoo22_TEST");
            break;
        }

                K_LOG_DEBUG( "gunoo22_TEST");

        //item_amount
        if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            item_amount,
            errMsg))
        {
            //error
                    K_LOG_DEBUG( "gunoo22_TEST");

            break;
        }

        K_LOG_DEBUG( "gunoo22_TEST");

        TradeItem trade_item;
        trade_item.id = item_id;
        trade_item.type = item_id.substr(0, 1); //id의 첫글자로 타입 구분 ex) 2000000 -> type 2 
        trade_item.amount = atoi(item_amount.c_str());

        trade_items.push_back(trade_item);
    }
#endif //gunoo22 260528 AddItem에서 아이템 업로드로 변경

        K_LOG_DEBUG( "gunoo22_TEST");

    rc = trade_service->Ready(player, trade_items, errMsg);
        K_LOG_DEBUG( "gunoo22_TEST");

err:
    if (rc == EXIT_SUCCESS)
    {
        //DB업데이트에 따라 각 Player의 Inventory 업데이트
        player_service->LoadInventory(player);
        player_service->LoadInventory(target_player);
        K_LOG_DEBUG( "player inventories loaded");
        //TODO
        //UpdateInventory로 변경

        const std::vector<TradeItem>& myItems = trade_service->GetMyItems(player);
        std::vector<std::string> myItemPacket;
        for (const auto& item : myItems)
        {
            myItemPacket.push_back(item.id);
            myItemPacket.push_back(std::to_string(item.amount));
            myItemPacket.push_back(std::to_string(item.slot_index));
        }

        const std::vector<TradeItem>& targetItems = trade_service->GetTargetItems(player);
        std::vector<std::string> targetItemPacket;
        for (const auto& item : targetItems)
        {
            targetItemPacket.push_back(item.id);
            targetItemPacket.push_back(std::to_string(item.amount));
            targetItemPacket.push_back(std::to_string(item.slot_index));
        }

        //상대 player에게 교환 성사 패킷 전송
        std::vector<std::string> payload;
        payload.insert(payload.end(), targetItemPacket.begin(), targetItemPacket.end());
        payload.push_back("$");
        payload.insert(payload.end(), myItemPacket.begin(), myItemPacket.end());
        //상대는 (상대아이템, 내아이템) 순으로 전송
        target_player->GetSession()->SendOk(PKT_TRADE_CONFIRM, payload);

        payload.clear();
        payload.insert(payload.end(), myItemPacket.begin(), myItemPacket.end());
        payload.push_back("$");
        payload.insert(payload.end(), targetItemPacket.begin(), targetItemPacket.end());
        //나는 (상대아이템, 내아이템) 순으로 전송
        session->SendOk(PKT_TRADE_CONFIRM, payload);

        trade_service->DeleteTradeSession(trade_service->GetTradeSession(player));
        
    }
    else if (rc == 2) //상대 교환준비 대기
    {
        //상대 player에게 교환 준비 상태 패킷 전송
        target_player->GetSession()->Send(PKT_TRADE_READY, {std::to_string(player->GetId())});
        session->Send(PKT_TRADE_READY, {"wait"});
    }
    else 
    {
        session->SendNok(PKT_TRADE_READY, errMsg);
    }
}

void PlayerHandler::HandleTradeCancel(PacketContext* ctx)
{
    int rc = EXIT_SUCCESS;
    ChannelSession *session = nullptr;
    Player *player = nullptr;
    TradeService* trade_service = nullptr;
    std::string target_player_id;
    Player *target_player = nullptr;
    PlayerManager* player_manager = nullptr;
    size_t offset = 0;
    std::string errMsg;

    if (ctx == nullptr)
    {
        K_LOG_ERROR( "ctx is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]ctx is nullptr";
        goto err;
    }
    session = ctx->channel_session;
    if (session == nullptr)
    {
        K_LOG_ERROR( "session is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]session is nullptr";
        goto err;
    }

    player = session->GetPlayer();
    if (player == nullptr)
    {
        K_LOG_ERROR( "player is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]player is nullptr";
        goto err;
    }

    player_manager = ctx->player_manager;
    if (player_manager == nullptr)
    {
        K_LOG_ERROR( "player_manager is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]player_manager is nullptr";
        goto err;
    }

    trade_service = ctx->trade_service;
    if (trade_service == nullptr)
    {
        K_LOG_ERROR( "trade_service is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]trade_service is nullptr";
        goto err;
    }

    //교환신청 상대방 Player id추출
    if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            target_player_id,
            errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }

    //교환신청 상대방 Player 객체 추출
    target_player = player_manager->GetPlayer(atoi(target_player_id.c_str()));
    if (target_player == nullptr)
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "target_player is nullptr\n");
        errMsg = "[" + std::to_string(rc) + "]target_player is nullptr";
        goto err;
    }


    rc = trade_service->Cancel(player, errMsg);

err:

    if (rc != EXIT_SUCCESS)
    {
        session->SendNok(PKT_TRADE_CANCEL, errMsg);
    }
    else
    {
        //상대 player에게 교환 성사 패킷 전송
        target_player->GetSession()->SendOk(PKT_TRADE_CANCEL);
        session->SendOk(PKT_TRADE_CANCEL);
    }
}

void PlayerHandler::HandleTradeAddItem(PacketContext* ctx)
{
    int rc = EXIT_SUCCESS;
    ChannelSession *session = nullptr;
    Player *player = nullptr;
    Player *target_player = nullptr;
    PlayerManager* player_manager = nullptr;
    size_t offset = 0;
    TradeService* trade_service = nullptr;
    TradeItem trade_item;
    std::string errMsg;
    std::string item_trade_slot_index;

    if (ctx == nullptr)
    {
        K_LOG_ERROR( "ctx is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]ctx is nullptr";
        goto err;
    }
    session = ctx->channel_session;
    if (session == nullptr)
    {
        K_LOG_ERROR( "session is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]session is nullptr";
        goto err;
    }

    player = session->GetPlayer();
    if (player == nullptr)
    {
        K_LOG_ERROR( "player is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]player is nullptr";
        goto err;
    }

    player_manager = ctx->player_manager;
    if (player_manager == nullptr)
    {
        K_LOG_ERROR( "player_manager is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]player_manager is nullptr";
        goto err;
    }

    trade_service = ctx->trade_service;
    if (trade_service == nullptr)
    {
        K_LOG_ERROR( "trade_service is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]trade_service is nullptr";
        goto err;
    }

    //교환신청 요청 Player 객체 추출
    target_player = trade_service->GetTargetPlayer(player);
    if (target_player == nullptr)
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "target_player is nullptr\n");
        errMsg = "[" + std::to_string(rc) + "]target_player is nullptr";
        goto err;
    }

    //Item추출
    {
        std::string item_id, item_amount, item_inven_slot_index;

        //item_id
        if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            item_id,
            errMsg))
        {
            rc = EXIT_FAILURE;
            K_LOG_ERROR( "ParseLengthPrefixedString fail");
            goto err;
        }

        //item_amount
        if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            item_amount,
            errMsg))
        {
            rc = EXIT_FAILURE;
            K_LOG_ERROR( "ParseLengthPrefixedString fail");
            goto err;
        }

        //item_trade_slot_index
        if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            item_trade_slot_index,
            errMsg))
        {
            rc = EXIT_FAILURE;
            K_LOG_ERROR( "ParseLengthPrefixedString fail");
            goto err;
        }

        //item_inven_slot_index
        if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            item_inven_slot_index,
            errMsg))
        {
            rc = EXIT_FAILURE;
            K_LOG_ERROR( "ParseLengthPrefixedString fail");
            goto err;
        }

        trade_item.id = item_id;
        {
            std::string tmpS = item_id.substr(0, 1); //id의 첫글자로 타입 구분 ex) 2000000 -> type 2 
            int tmpI = std::stoi(tmpS) - 1;
            trade_item.type = std::to_string(tmpI);
        }
        trade_item.amount = std::stoi(item_amount);
        trade_item.slot_index = std::stoi(item_inven_slot_index);

        K_LOG_DEBUG( "player[%s] uploads item", player->GetName().c_str());
        K_LOG_DEBUG( "trade_item.id: %s", trade_item.id.c_str());
        K_LOG_DEBUG( "trade_item.type: %s", trade_item.type.c_str());
        K_LOG_DEBUG( "trade_item.amount: %d", trade_item.amount);
        K_LOG_DEBUG( "trade_item.slot_index: %d", trade_item.slot_index);

        rc = trade_service->UploadItem(player, trade_item, errMsg);
    }

err:
    if (rc == EXIT_SUCCESS)
    {
        //상대 player에게 아이템 업로드 패킷 전송
        std::vector<std::string> payload = {
            trade_item.id,
            std::to_string(trade_item.amount),
            item_trade_slot_index
        };

        target_player->GetSession()->Send(PKT_TRADE_ADD_ITEM, payload);
        session->SendOk(PKT_TRADE_ADD_ITEM);
    }
    else 
    {
        session->SendNok(PKT_TRADE_ADD_ITEM, errMsg);
    }
}
