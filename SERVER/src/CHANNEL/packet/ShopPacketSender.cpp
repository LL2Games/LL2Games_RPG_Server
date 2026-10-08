#include "ShopPacketSender.h"

#include "ChannelSession.h"
#include "Packet.h"
#include <string>
#include <vector>

void ShopPacketSender::SendOpen(ChannelSession* session,const NPCInteractionResult& npc,const ShopDefinition& shop)
{
    if (session == nullptr)
        return;

    std::vector<std::string> payload;
    payload.reserve(7 + shop.products.size() * 4);

    payload.push_back(std::to_string(npc.mapId));
    payload.push_back(std::to_string(npc.spawnId));
    payload.push_back(std::to_string(npc.npcId));

    payload.push_back(std::to_string(shop.shopId));
    payload.push_back(shop.name);
    payload.push_back(shop.allowSell ? "1" : "0");
    payload.push_back(std::to_string(shop.products.size()));

    for (const ShopProduct& product : shop.products)
    {
        payload.push_back(std::to_string(product.productId));
        payload.push_back(std::to_string(product.itemId));
        payload.push_back(std::to_string(product.price));
        payload.push_back(std::to_string(product.maxPerPurchase));
    }
    session->SendOk(PKT_SHOP_OPEN, payload);
}

void ShopPacketSender::SendBuySuccess(ChannelSession* session, const ShopBuyResult& result)
{
    if (session == nullptr)
        return;

    std::vector<std::string> payload;
    payload.reserve(9 + result.updatedSlots.size() * 4);

    payload.push_back(std::to_string(result.mapId));
    payload.push_back(std::to_string(result.spawnId));
    payload.push_back(std::to_string(result.shopId));
    payload.push_back(std::to_string(result.productId));
    payload.push_back(std::to_string(result.itemId));
    payload.push_back(std::to_string(result.count));
    payload.push_back(std::to_string(result.totalPrice));
    payload.push_back(std::to_string(result.gold));

    payload.push_back(std::to_string(result.updatedSlots.size()));

    for (const AddItemResult& slot : result.updatedSlots)
    {
        payload.push_back(std::to_string(slot.inventoryType));
        payload.push_back(std::to_string(slot.slotPos));
        payload.push_back(std::to_string(slot.itemId));
        payload.push_back(std::to_string(slot.itemCount));
    }

    session->SendOk(PKT_SHOP_BUY, payload);
}

void ShopPacketSender::SendSellSuccess(ChannelSession* session, const ShopSellResult& result)
{
    if (session == nullptr)
        return;

    const AddItemResult& slot = result.updatedSlot;

    std::vector<std::string> fields;
    fields.reserve(11);

    fields.push_back(std::to_string(result.mapId));
    fields.push_back(std::to_string(result.spawnId));
    fields.push_back(std::to_string(result.shopId));
    fields.push_back(std::to_string(result.itemId));
    fields.push_back(std::to_string(result.count));
    fields.push_back(std::to_string(result.totalPrice));
    fields.push_back(std::to_string(result.gold));

    fields.push_back(std::to_string(slot.inventoryType));
    fields.push_back(std::to_string(slot.slotPos));
    fields.push_back(std::to_string(slot.itemId));
    fields.push_back(std::to_string(slot.itemCount));

    session->SendOk(PKT_SHOP_SELL, fields);
}