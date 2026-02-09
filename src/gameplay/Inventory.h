#pragma once
#include <cstdint>
#include <array>
#include <string>

constexpr int HOTBAR_SLOTS = 9;
constexpr int INVENTORY_ROWS = 3;
constexpr int INVENTORY_COLS = 9;
constexpr int STORAGE_SLOTS = INVENTORY_ROWS * INVENTORY_COLS;
constexpr int TOTAL_SLOTS = HOTBAR_SLOTS + STORAGE_SLOTS;
constexpr int MAX_STACK_SIZE = 64;

struct ItemStack
{
    uint8_t blockId;
    uint8_t count;

    bool isEmpty() const { return blockId == 0 || count == 0; }
    void clear() { blockId = 0; count = 0; }
};

struct Inventory
{
    std::array<ItemStack, TOTAL_SLOTS> slots;
    ItemStack heldItem;
    int selectedHotbar;

    Inventory();

    ItemStack& hotbar(int index);
    const ItemStack& hotbar(int index) const;

    ItemStack& slot(int index);
    const ItemStack& slot(int index) const;

    ItemStack& selectedItem();
    const ItemStack& selectedItem() const;

    bool addItem(uint8_t blockId, uint8_t count = 1);
    bool removeFromSelected(uint8_t count = 1);
    void swapSlots(int a, int b);

    bool loadFromFile(const std::string& path);
    void saveToFile(const std::string& path) const;
};

void drawHotbar(const Inventory& inv, int fbWidth, int fbHeight);
void drawInventoryScreen(Inventory& inv, int fbWidth, int fbHeight);