#include "QuickSlotManager.h"

void QuickSlotManager::Init()
{
    std::lock_guard<std::mutex> lock(m_quickSlotMutex);
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
    std::lock_guard<std::mutex> lock(m_quickSlotMutex);

    std::vector<QuickSlotData> changedSlots;
    if (data.slot_index < 0)
    {
       
        return changedSlots;
    }

    auto slot = m_quickSlots.find(data.slot_index);
    if (slot == m_quickSlots.end())
    {
     
        return changedSlots;
    }
        
    
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
    
    slot->second = data;
    slot->second.slot_index = data.slot_index;

    changedSlots.push_back(slot->second);

    K_LOG_DEBUG( "SetSlot end\n");

    return changedSlots;
}
    
void QuickSlotManager::RemoveSlot(int slotIndex)
{
    std::lock_guard<std::mutex> lock(m_quickSlotMutex);

    auto slot = m_quickSlots.find(slotIndex);

    if(slot != m_quickSlots.end())
    {
        slot->second =  QuickSlotData{}; 
    }
}

std::optional<QuickSlotData> QuickSlotManager::GetSlot(int slotIndex) const
 {
    std::lock_guard<std::mutex> lock(m_quickSlotMutex);
    auto slot = m_quickSlots.find(slotIndex);

    if(slot == m_quickSlots.end())
    {
        return std::nullopt;
    }

    return slot->second;
 }

std::vector<QuickSlotData> QuickSlotManager::GetSlotList() const
 {
    std::lock_guard<std::mutex> lock(m_quickSlotMutex);
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

    if (a.type == QuickSlotType::Skill || a.type == QuickSlotType::Item)
    {
        return a.ref_id == b.ref_id;
    }

    return false;
 }

 void QuickSlotManager::ClearSlot(int slotIndex)
 {
    std::lock_guard<std::mutex> lock(m_quickSlotMutex);
    
    auto it = m_quickSlots.find(slotIndex);
    if (it == m_quickSlots.end())
        return;

    it->second = QuickSlotData{};
    it->second.slot_index = slotIndex;
 }