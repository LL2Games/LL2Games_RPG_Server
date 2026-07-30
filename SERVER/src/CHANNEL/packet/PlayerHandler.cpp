#include "PlayerHandler.h"
#include "CommonEnum.h"
#include "PacketParser.h"
#include "ChannelSession.h"
#include "PlayerManager.h"
#include "MapInstance.h"
#include "K_slog.h"
#include "PacketParser.h"






PlayerHandler::PlayerHandler(uint16_t type) : m_type(type)
{
     
}


void PlayerHandler::Execute(PacketContext * ctx)
{
     switch(ctx->type)
     {
          case PKT_PLAYER_MOVE:
               MovePacket(ctx);
               break;
          case PKT_PLAYER_ATTACK:
               AttackPacket(ctx);
               break;
          case PKT_PLAYER_BASIC_ATTACK:
               K_LOG_TRACE( "PKT_PLAYER_BASIC_ATTACK START\n");
               BasicAttackPacket(ctx);
               break;
          case PKT_PLAYER_ONDAMAGED:
               K_LOG_TRACE( "PKT_PLAYER_ONDAMAGED START\n");
               OnDamagedPacket(ctx);
               break;
          case PKT_PLAYER_USE_ITEM:
               K_LOG_TRACE( "PKT_PLAYER_USE_ITEM START\n");
               UseItemPacket(ctx);
               break;
          // case PKT_STAT_VIEW:
          //      K_LOG_ERROR( "PKT_STAT_VIEW START\n");
          //      HandleStatView(ctx);
          //       break;
          case PKT_STAT_UP:
               K_LOG_TRACE("PKT_STAT_UP START\n");
               HandleStatUp(ctx);
               break;
          case PKT_TRADE_REQUEST:
               HandleTradeRequest(ctx);
               break;
          case PKT_TRADE_ACCEPT:
               HandleTradeAccept(ctx);
               break;
          case PKT_TRADE_READY:
               HandleTradeReady(ctx);
               break;
          case PKT_TRADE_CANCEL:
               HandleTradeCancel(ctx);
               break;
          case PKT_TRADE_ADD_ITEM:
               HandleTradeAddItem(ctx);
               break;
          case PKT_PLAYER_PICKUP_ITEM:
               PickUpItemPacket(ctx);
               break;
          default :
               K_LOG_ERROR( "default START\n");
               break;
     }
    
}

