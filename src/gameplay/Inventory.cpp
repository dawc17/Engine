#include "Inventory.h"
#include "../core/MainGlobals.h"
#include "../rendering/ToolModelGenerator.h"
#include "../../libs/imgui/imgui.h"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <cstdio>
#include <utility>
#include <vector>

const std::vector<CreativeItem>& getCreativeItems()
{
    static const std::vector<CreativeItem> items = {
        {1, "Dirt"},
        {2, "Grass"},
        {3, "Stone"},
        {4, "Sand"},
        {5, "Oak Log"},
        {6, "Oak Leaves"},
        {7, "Glass"},
        {8, "Oak Planks"},
        {18, "Snow Grass"},
        {19, "Spruce Log"},
        {20, "Spruce Planks"},
        {21, "Spruce Leaves"},
        {22, "Cobblestone"},
        {TOOL_DIAMOND_PICKAXE, "Diamond Pickaxe"},
        {TOOL_WOOD_PICKAXE, "Wood Pickaxe"},
        {TOOL_STONE_PICKAXE, "Stone Pickaxe"},
        {TOOL_GOLD_PICKAXE, "Gold Pickaxe"},
        {TOOL_IRON_PICKAXE, "Iron Pickaxe"},
    };
    return items;
}

Inventory::Inventory()
    : selectedHotbar(0)
{
    for (auto& s : slots)
        s.clear();
    heldItem.clear();
}

ItemStack& Inventory::hotbar(int index)
{
    return slots[index];
}

const ItemStack& Inventory::hotbar(int index) const
{
    return slots[index];
}

ItemStack& Inventory::slot(int index)
{
    return slots[index];
}

const ItemStack& Inventory::slot(int index) const
{
    return slots[index];
}

ItemStack& Inventory::selectedItem()
{
    return slots[selectedHotbar];
}

const ItemStack& Inventory::selectedItem() const
{
    return slots[selectedHotbar];
}

bool Inventory::addItem(uint8_t blockId, uint8_t count)
{
    for (int i = 0; i < HOTBAR_SLOTS; i++)
    {
        if (slots[i].blockId == blockId && slots[i].count < MAX_STACK_SIZE)
        {
            uint8_t space = MAX_STACK_SIZE - slots[i].count;
            uint8_t toAdd = std::min(count,space);
            slots[i].count += toAdd;
            count -= toAdd;
            if (count == 0) return true;
        }
    }

    for (int i = HOTBAR_SLOTS; i < TOTAL_SLOTS; i++)
    {
        if (slots[i].blockId == blockId && slots[i].count < MAX_STACK_SIZE)
        {
            uint8_t space = MAX_STACK_SIZE - slots[i].count;
            uint8_t toAdd = std::min(count, space);
            slots[i].count += toAdd;
            count -= toAdd;
            if (count == 0) return true;
        }
    }

    for (int i = 0; i < HOTBAR_SLOTS; i++)
    {
        if (slots[i].isEmpty())
        {
            slots[i].blockId = blockId;
            slots[i].count = count;
            return true;
        }
    }

    for (int i = HOTBAR_SLOTS; i < TOTAL_SLOTS; i++)
    {
        if (slots[i].isEmpty())
        {
            slots[i].blockId = blockId;
            slots[i].count = count;
            return true;
        }
    }

    return false;
}

bool Inventory::removeFromSelected(uint8_t count)
{
    ItemStack& item = slots[selectedHotbar];
    if (item.isEmpty() || item.count < count)
        return false;
    item.count -= count;
    if (item.count == 0)
        item.clear();
    return true;
}

void Inventory::swapSlots(int a, int b)
{
    std::swap(slots[a], slots[b]);
}

bool Inventory::loadFromFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        return false;

    uint8_t sel = 0;
    file.read(reinterpret_cast<char*>(&sel), 1);
    selectedHotbar = sel;

    for (int i = 0; i < TOTAL_SLOTS; i++)
    {
        file.read(reinterpret_cast<char*>(&slots[i].blockId), 1);
        file.read(reinterpret_cast<char*>(&slots[i].count), 1);
    }

    heldItem.clear();
    return file.good();
}

void Inventory::saveToFile(const std::string& path) const
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
        return;

    uint8_t sel = static_cast<uint8_t>(selectedHotbar);
    file.write(reinterpret_cast<const char*>(&sel), 1);

    for (int i = 0; i < TOTAL_SLOTS; i++)
    {
        file.write(reinterpret_cast<const char*>(&slots[i].blockId), 1);
        file.write(reinterpret_cast<const char*>(&slots[i].count), 1);
    }
}

static void handleSlotClick(Inventory& inv, int slotIdx)
{
    ItemStack& slotItem = inv.slot(slotIdx);

    if (inv.heldItem.isEmpty() && !slotItem.isEmpty())
    {
        inv.heldItem = slotItem;
        slotItem.clear();
    }
    else if (!inv.heldItem.isEmpty() && slotItem.isEmpty())
    {
        slotItem = inv.heldItem;
        inv.heldItem.clear();
    }
    else if (!inv.heldItem.isEmpty() && !slotItem.isEmpty()) 
    {
        if (inv.heldItem.blockId == slotItem.blockId)
        {
            uint8_t space = MAX_STACK_SIZE - slotItem.count;
            uint8_t toAdd = std::min(inv.heldItem.count, space);
            slotItem.count += toAdd;
            inv.heldItem.count -= toAdd;
            if (inv.heldItem.count == 0)
                inv.heldItem.clear();
        }
        else 
        {
            std::swap(inv.heldItem, slotItem);
        }
    }
}

static void drawSlotWidget(Inventory& inv, int slotIdx, float slotSize, bool isSelected)
{
    ImGui::PushID(slotIdx);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImU32 bgColor = isSelected ? IM_COL32(80, 80, 80, 220) : IM_COL32(40, 40, 40, 220);
    ImU32 borderColor = isSelected ? IM_COL32(225, 225, 225, 220) : IM_COL32(100, 100, 100, 150);

    dl->AddRectFilled(pos, ImVec2(pos.x + slotSize, pos.y + slotSize), bgColor);
    dl->AddRect(pos, ImVec2(pos.x + slotSize, pos.y + slotSize), borderColor, 0.0f, 0, isSelected ? 2.0f : 1.0f);

    if (ImGui::InvisibleButton("##slot", ImVec2(slotSize, slotSize)))
        handleSlotClick(inv, slotIdx);

    if (ImGui::IsItemHovered())
        dl->AddRectFilled(pos, ImVec2(pos.x + slotSize, pos.y + slotSize), IM_COL32(255, 255, 255, 40));

    const ItemStack& item = inv.slot(slotIdx);
    if (!item.isEmpty())
    {
        GLuint iconTex = 0;
        if (isToolItem(item.blockId))
        {
            auto it = g_toolIcons.find(item.blockId);
            if (it != g_toolIcons.end()) iconTex = it->second;
        }
        else
        {
            auto it = g_blockIcons.find(item.blockId);
            if (it != g_blockIcons.end()) iconTex = it->second;
        }
        if (iconTex != 0)
        {
            float pad = 4.0f;
            dl->AddImage(
                (ImTextureID)(intptr_t)iconTex,
                ImVec2(pos.x + pad, pos.y + pad),
                ImVec2(pos.x + slotSize - pad, pos.y + slotSize - pad));
        }

        if (item.count > 1)
        {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", item.count);
            ImVec2 textSize = ImGui::CalcTextSize(buf);
            ImVec2 textPos(pos.x + slotSize - textSize.x - 4.0f, pos.y + slotSize - textSize.y - 2.0f);
            dl->AddText(ImVec2(textPos.x + 1.0f, textPos.y + 1.0f), IM_COL32(0, 0, 0, 200), buf);
            dl->AddText(textPos, IM_COL32(255, 255, 255, 255), buf);
        }
    }

    ImGui::PopID();
}

void drawHotbar(const Inventory &inv, int fbWidth, int fbHeight)
{
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    float slotSize = 48.0f;
    float slotSpacing = 4.0f;
    float totalWidth = HOTBAR_SLOTS * slotSize + (HOTBAR_SLOTS - 1) * slotSpacing;
    float startX = (fbWidth - totalWidth) * 0.5f;
    float startY = fbHeight - slotSize - 20.0f;

    float bgPad = 6.0f;
    dl->AddRectFilled(
        ImVec2(startX - bgPad, startY - bgPad),
        ImVec2(startX + totalWidth + bgPad, startY + slotSize + bgPad),
        IM_COL32(0, 0, 0, 140));

        for (int i = 0; i < HOTBAR_SLOTS; i++)
        {
            float x = startX + i * (slotSize + slotSpacing);
            float y = startY;
            bool selected = (i == inv.selectedHotbar);
    
            ImU32 slotBg = selected ? IM_COL32(80, 80, 80, 220) : IM_COL32(40, 40, 40, 200);
            ImU32 slotBorder = selected ? IM_COL32(255, 255, 255, 220) : IM_COL32(120, 120, 120, 150);
            float borderThick = selected ? 2.5f : 1.0f;
    
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + slotSize, y + slotSize), slotBg);
            dl->AddRect(ImVec2(x, y), ImVec2(x + slotSize, y + slotSize), slotBorder, 0.0f, 0, borderThick);
    
            const ItemStack& item = inv.hotbar(i);
            if (!item.isEmpty())
            {
                GLuint iconTex = 0;
                if (isToolItem(item.blockId))
                {
                    auto it = g_toolIcons.find(item.blockId);
                    if (it != g_toolIcons.end()) iconTex = it->second;
                }
                else
                {
                    auto it = g_blockIcons.find(item.blockId);
                    if (it != g_blockIcons.end()) iconTex = it->second;
                }
                if (iconTex != 0)
                {
                    float pad = 4.0f;
                    dl->AddImage(
                        (ImTextureID)(intptr_t)iconTex,
                        ImVec2(x + pad, y + pad),
                        ImVec2(x + slotSize - pad, y + slotSize - pad));
                }
    
                if (item.count > 1)
                {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "%d", item.count);
                    ImVec2 textSize = ImGui::CalcTextSize(buf);
                    ImVec2 textPos(x + slotSize - textSize.x - 4.0f, y + slotSize - textSize.y - 2.0f);
                    dl->AddText(ImVec2(textPos.x + 1.0f, textPos.y + 1.0f), IM_COL32(0, 0, 0, 200), buf);
                    dl->AddText(textPos, IM_COL32(255, 255, 255, 255), buf);
                }
            }
    
            char numBuf[4];
            std::snprintf(numBuf, sizeof(numBuf), "%d", i + 1);
            dl->AddText(ImVec2(x + 3.0f, y + 2.0f), IM_COL32(200, 200, 200, 150), numBuf);
        }
}

void drawInventoryScreen(Inventory &inv, int fbWidth, int fbHeight)
{
    float slotSize = 52.0f;
    float slotSpacing = 4.0f;
    float windowWidth = INVENTORY_COLS * (slotSize + slotSpacing) + 24.0f;
    float windowHeight = (INVENTORY_ROWS + 1) * (slotSize + slotSpacing) + 60.0f;

    ImGui::SetNextWindowPos(ImVec2(fbWidth * 0.5f, fbHeight * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGui::Begin("##Inventory", nullptr, flags);

    ImGui::Text("Inventory");
    ImGui::Spacing();

    for (int row = 0; row < INVENTORY_ROWS; row++)
    {
        for (int col = 0; col < INVENTORY_COLS; col++)
        {
            int slotIdx = HOTBAR_SLOTS + row * INVENTORY_COLS + col;
            drawSlotWidget(inv, slotIdx, slotSize, false);
            if (col < INVENTORY_COLS - 1)
                ImGui::SameLine(0.0f, slotSpacing);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    for (int col = 0; col < HOTBAR_SLOTS; col++)
    {
        drawSlotWidget(inv, col, slotSize, col == inv.selectedHotbar);
        if (col < HOTBAR_SLOTS - 1)
            ImGui::SameLine(0.0f, slotSpacing);
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    if (!inv.heldItem.isEmpty())
    {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 mousePos = ImGui::GetMousePos();
        float halfSlot = slotSize * 0.5f;

        GLuint heldTex = 0;
        if (isToolItem(inv.heldItem.blockId))
        {
            auto it = g_toolIcons.find(inv.heldItem.blockId);
            if (it != g_toolIcons.end()) heldTex = it->second;
        }
        else
        {
            auto it = g_blockIcons.find(inv.heldItem.blockId);
            if (it != g_blockIcons.end()) heldTex = it->second;
        }
        if (heldTex != 0)
        {
            dl->AddImage(
                (ImTextureID)(intptr_t)heldTex,
                ImVec2(mousePos.x - halfSlot + 4.0f, mousePos.y - halfSlot + 4.0f),
                ImVec2(mousePos.x + halfSlot - 4.0f, mousePos.y + halfSlot - 4.0f));
        }

        if (inv.heldItem.count > 1)
        {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", inv.heldItem.count);
            dl->AddText(ImVec2(mousePos.x + 4.0f, mousePos.y + 4.0f), IM_COL32(255, 255, 255, 255), buf);
        }
    }
}

static GLuint getItemIcon(uint8_t id)
{
    if (isToolItem(id))
    {
        auto it = g_toolIcons.find(id);
        if (it != g_toolIcons.end()) return it->second;
    }
    else
    {
        auto it = g_blockIcons.find(id);
        if (it != g_blockIcons.end()) return it->second;
    }
    return 0;
}

void drawCreativeInventoryScreen(Inventory& inv, int fbWidth, int fbHeight)
{
    const auto& items = getCreativeItems();
    int catalogRows = (static_cast<int>(items.size()) + HOTBAR_SLOTS - 1) / HOTBAR_SLOTS;

    float slotSize = 52.0f;
    float slotSpacing = 4.0f;
    float windowWidth = HOTBAR_SLOTS * (slotSize + slotSpacing) + 24.0f;
    float catalogHeight = catalogRows * (slotSize + slotSpacing);
    float hotbarHeight = slotSize + slotSpacing;
    float windowHeight = catalogHeight + hotbarHeight + 80.0f;

    ImGui::SetNextWindowPos(ImVec2(fbWidth * 0.5f, fbHeight * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGui::Begin("##CreativeInventory", nullptr, flags);

    ImGui::Text("Creative");
    ImGui::Spacing();

    for (int i = 0; i < static_cast<int>(items.size()); i++)
    {
        int col = i % HOTBAR_SLOTS;
        ImGui::PushID(10000 + i);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(pos, ImVec2(pos.x + slotSize, pos.y + slotSize), IM_COL32(40, 40, 40, 220));
        dl->AddRect(pos, ImVec2(pos.x + slotSize, pos.y + slotSize), IM_COL32(100, 100, 100, 150));

        if (ImGui::InvisibleButton("##cslot", ImVec2(slotSize, slotSize)))
        {
            uint8_t count = isToolItem(items[i].id) ? 1 : 64;
            inv.heldItem = {items[i].id, count};
        }

        if (ImGui::IsItemHovered())
        {
            dl->AddRectFilled(pos, ImVec2(pos.x + slotSize, pos.y + slotSize), IM_COL32(255, 255, 255, 40));
            ImGui::SetTooltip("%s", items[i].name);
        }

        GLuint iconTex = getItemIcon(items[i].id);
        if (iconTex != 0)
        {
            float pad = 4.0f;
            dl->AddImage(
                (ImTextureID)(intptr_t)iconTex,
                ImVec2(pos.x + pad, pos.y + pad),
                ImVec2(pos.x + slotSize - pad, pos.y + slotSize - pad));
        }

        ImGui::PopID();

        if (col < HOTBAR_SLOTS - 1 && i < static_cast<int>(items.size()) - 1)
            ImGui::SameLine(0.0f, slotSpacing);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    for (int col = 0; col < HOTBAR_SLOTS; col++)
    {
        drawSlotWidget(inv, col, slotSize, col == inv.selectedHotbar);
        if (col < HOTBAR_SLOTS - 1)
            ImGui::SameLine(0.0f, slotSpacing);
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    if (!inv.heldItem.isEmpty())
    {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 mousePos = ImGui::GetMousePos();
        float halfSlot = slotSize * 0.5f;

        GLuint heldTex = getItemIcon(inv.heldItem.blockId);
        if (heldTex != 0)
        {
            dl->AddImage(
                (ImTextureID)(intptr_t)heldTex,
                ImVec2(mousePos.x - halfSlot + 4.0f, mousePos.y - halfSlot + 4.0f),
                ImVec2(mousePos.x + halfSlot - 4.0f, mousePos.y + halfSlot - 4.0f));
        }

        if (inv.heldItem.count > 1)
        {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", inv.heldItem.count);
            dl->AddText(ImVec2(mousePos.x + 4.0f, mousePos.y + 4.0f), IM_COL32(255, 255, 255, 255), buf);
        }
    }
}