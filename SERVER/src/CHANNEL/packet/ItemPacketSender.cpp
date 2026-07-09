#include "ItemPacketSender.h"  
#include "ChannelSession.h"

void ItemPacketSender::SendSpawnItem(std::vector<DropSpawnInfo> spawnedInfos, std::unordered_map<int, Player*>& playerList)
{

    std::vector<std::string> payload;	
	payload.push_back(std::to_string(spawnedInfos.size()));

    for (auto& [id, p] : playerList)
    {
    	if (!p) continue;
        auto session = p->GetSession();
        if (!session) continue;
		for(size_t i =0; i < spawnedInfos.size(); i++)
		{
        	payload.push_back(std::to_string(spawnedInfos[i].dropId));
        	payload.push_back(std::to_string(spawnedInfos[i].itemId));
        	payload.push_back(std::to_string(spawnedInfos[i].count));
        	payload.push_back(std::to_string(spawnedInfos[i].xPos));
        	payload.push_back(std::to_string(spawnedInfos[i].yPos));
		}
		session->Send(PKT_DROPITEMS, payload);
    }	
}

void ItemPacketSender::SendAddItem(Player *player, std::vector<AddItemResult> &addItemResult)
{
	if (player == nullptr)
        return;

    auto session = player->GetSession();
    if (session == nullptr)
        return;

    if (addItemResult.empty())
	{
		return;
	}
        
	std::vector<std::string> payload;	
	payload.push_back(std::to_string(addItemResult.size()));
	
	for(size_t i =0; i < addItemResult.size(); i++)
	{
		K_LOG_TRACE( "inventoryType [%d]\n", addItemResult[i].inventoryType);
		K_LOG_TRACE( "slotPos [%d]\n", addItemResult[i].slotPos);
		K_LOG_TRACE( "itemId [%d]\n", addItemResult[i].itemId);
		K_LOG_TRACE( "itemCount [%d]\n", addItemResult[i].itemCount);

		payload.push_back(std::to_string(addItemResult[i].inventoryType));
    	payload.push_back(std::to_string(addItemResult[i].slotPos));
    	payload.push_back(std::to_string(addItemResult[i].itemId));
    	payload.push_back(std::to_string(addItemResult[i].itemCount));  
	}
	session->Send(PKT_PLAYER_PICKUP_ITEM, payload);
	K_LOG_TRACE( "Send PickUpItem Success\n");
}

void ItemPacketSender::SendRemoveDropItem(const std::vector<int>& removeItems, const std::unordered_map<int, Player*>& playerList)
{
    for (auto& [id, p] : playerList)
    {
        if (!p) continue;
        auto session = p->GetSession();
        if (!session) continue;
        std::vector<std::string> payload;
		
		payload.push_back(std::to_string(removeItems.size()));
		for(size_t i =0; i < removeItems.size(); i++)
		{
        	payload.push_back(std::to_string(removeItems[i]));
		}

		session->Send(PKT_REMOVEITEMS, payload);
		K_LOG_TRACE( "Send PKT_REMOVEITEMS Success\n");
    }	
}

