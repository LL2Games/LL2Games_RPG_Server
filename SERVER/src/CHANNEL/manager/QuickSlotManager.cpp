#include "QuickSlotManager.h"

void QuickSlotManager::Init()
{
   m_quickSlots.clear();

    for (int i = 0; i < m_maxSlotCount; ++i)
    {
        QuickSlotData data{};
        data.slot_index = i;
        m_quickSlots.emplace(i, data);
    }
}


std::vector<QuickSlotData> QuickSlotManager::SetSlot(const QuickSlotData& data)
{
    std::vector<QuickSlotData> changedSlots;
    K_LOG_DEBUG( "LHJ TEST0\n");
    if (data.slot_index < 0)
    {
        K_LOG_DEBUG( "LHJ TEST1\n");
        return changedSlots;
    }
    K_LOG_DEBUG( "LHJ TEST5\n");    
    auto slot = m_quickSlots.find(data.slot_index);
    if (slot == m_quickSlots.end())
    {
        K_LOG_DEBUG( "LHJ TEST2\n");
        return changedSlots;
    }
        
    K_LOG_DEBUG( "LHJ TEST4\n");
    for (auto& [slotIndex, slotData] : m_quickSlots)
    {
        if (slotIndex == data.slot_index)
            continue;

        if (IsSameTarget(slotData, data))
        {
            slotData = QuickSlotData{};
            slotData.slot_index = slotIndex;

            changedSlots.push_back(slotData);
        }
    }
    K_LOG_DEBUG( "LHJ TEST3\n");
    slot->second = data;
    slot->second.slot_index = data.slot_index;

    changedSlots.push_back(slot->second);

    K_LOG_DEBUG( "SetSlot end\n");

    return changedSlots;
}
    
void QuickSlotManager::RemoveSlot(int slotIndex)
{
    auto slot = m_quickSlots.find(slotIndex);

    if(slot != m_quickSlots.end())
    {
        slot->second =  QuickSlotData{}; 
    }
}

 const QuickSlotData* QuickSlotManager::GetSlot(int slotIndex) const
 {
    auto slot = m_quickSlots.find(slotIndex);

    if(slot == m_quickSlots.end())
    {
        return nullptr;
    }

    return &(slot->second);
 }

 const std::vector<QuickSlotData> QuickSlotManager::GetSlotList() const
 {
    std::vector<QuickSlotData> quickSlotDatas;

    quickSlotDatas.reserve(m_quickSlots.size());

    for(auto [id, slotData] : m_quickSlots)
    {
        quickSlotDatas.push_back(slotData);
    }
    return quickSlotDatas;
 }

 bool QuickSlotManager::IsSameTarget(const QuickSlotData& a, const QuickSlotData& b)
 {
    if (a.type != b.type)
        return false;

    if (a.type == QuickSlotType::Skill)
    {
        return a.ref_id == b.ref_id;
    }

    if (a.type == QuickSlotType::Item)
    {
        return a.ref_id == b.ref_id;
    }

    return false;
 }

 void QuickSlotManager::ClearSlot(int slotIndex)
 {
    auto it = m_quickSlots.find(slotIndex);
    if (it == m_quickSlots.end())
        return;

    it->second = QuickSlotData{};
    it->second.slot_index = slotIndex;
 }