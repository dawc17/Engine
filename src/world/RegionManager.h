#pragma once
#include "Chunk.h"
#include "../utils/CoordUtils.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <mutex>
#include <fstream>
#include <vector>
#include <memory>

constexpr int REGION_SIZE = 32;
constexpr int REGION_SHIFT = 5;
constexpr int REGION_MASK = REGION_SIZE - 1;
constexpr int HEADER_ENTRIES = REGION_SIZE * REGION_SIZE;
constexpr int HEADER_SIZE = HEADER_ENTRIES * 8;
constexpr int SECTOR_SIZE = 4096;

using RegionCoord = glm::ivec2;
using RegionCoordHash = IVec2Hash;

struct ColumnEntry
{
    uint32_t offset;
    uint32_t size;
};

struct SectionData
{
    int8_t y;
    std::vector<uint8_t> compressedBlocks;
};

struct ColumnData
{
    std::vector<SectionData> sections;
};

class RegionFile
{
public:
    RegionFile(const std::string& path);
    ~RegionFile();

    bool loadColumn(int localX, int localZ, ColumnData& outData);
    void saveColumn(int localX, int localZ, const ColumnData& data);
    void flush();

private:
    std::string filePath;
    std::fstream file;
    ColumnEntry header[HEADER_ENTRIES];
    bool headerDirty;
    std::mutex mutex;

    int getEntryIndex(int localX, int localZ) const;
    void readHeader();
    void writeHeader();
    uint32_t allocateSectors(uint32_t numBytes);
};

struct PlayerData
{
    uint32_t version;
    float x, y, z;
    float yaw, pitch;
    float timeOfDay;
    float health;
    float hunger;
    int32_t gamemode;
};

class RegionManager
{
public:
    RegionManager(const std::string& worldPath = "saves/world");
    ~RegionManager();

    bool loadChunkData(int cx, int cy, int cz, BlockID* outBlocks);
    void saveChunkData(int cx, int cy, int cz, const BlockID* blocks);
    void flush();

    bool loadPlayerData(PlayerData& outData);
    void savePlayerData(const PlayerData& data);

private:
    std::string worldPath;
    std::unordered_map<RegionCoord, std::unique_ptr<RegionFile>, RegionCoordHash> regions;
    std::mutex regionsMutex;

    RegionFile* getOrOpenRegion(int regX, int regZ);
    std::string getRegionPath(int regX, int regZ) const;

    static void compressBlocks(const BlockID* blocks, std::vector<uint8_t>& outCompressed);
    static bool decompressBlocks(const std::vector<uint8_t>& compressed, BlockID* outBlocks);
};

