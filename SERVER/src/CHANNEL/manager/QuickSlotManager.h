#pragma once
#include "common.h"
#include "Slot_Info.h"

#include <mutex>
#include <optional>

class QuickSlotManager
{
public:
    void Init();        
    std::vector<QuickSlotData> SetSlot(const QuickSlotData& data);  
    void RemoveSlot(int slotIndex);
    void ClearSlot(int slotIndex);
public:
    std::optional<QuickSlotData> GetSlot(int slotIndex) const;
    std::vector<QuickSlotData> GetSlotList() const;
private:
     static bool IsSameTarget(const QuickSlotData& a,const QuickSlotData& b);
private:
    mutable std::mutex m_quickSlotMutex;
    
    std::unordered_map<int, QuickSlotData> m_quickSlots;
    static constexpr int m_maxSlotCount = 32;
};