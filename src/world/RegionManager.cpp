#include "RegionManager.h"
#include <filesystem>
#include <cstring>
#include <algorithm>
#include <ios>
#include "../../libs/zlib-1.3.1/zlib.h"

namespace fs = std::filesystem;

namespace {

constexpr uint16_t MORTON_SPREAD[16] = {
    0x000, 0x001, 0x008, 0x009,
    0x040, 0x041, 0x048, 0x049,
    0x200, 0x201, 0x208, 0x209,
    0x240, 0x241, 0x248, 0x249
};

inline uint16_t mortonEncode3D(uint8_t x, uint8_t y, uint8_t z)
{
    return MORTON_SPREAD[x & 0xF]
         | (MORTON_SPREAD[y & 0xF] << 1)
         | (MORTON_SPREAD[z & 0xF] << 2);
}

inline uint8_t mortonCompact1By2(uint16_t v)
{
    v &= 0x249;
    v = (v ^ (v >> 2)) & 0x0C3;
    v = (v ^ (v >> 4)) & 0x00F;
    return static_cast<uint8_t>(v);
}

struct TraversalOrders
{
    uint16_t yMajor[CHUNK_VOLUME];
    uint16_t morton[CHUNK_VOLUME];

    TraversalOrders()
    {
        int idx = 0;
        for (int y = 0; y < CHUNK_SIZE; y++)
            for (int z = 0; z < CHUNK_SIZE; z++)
                for (int x = 0; x < CHUNK_SIZE; x++)
                    yMajor[idx++] = static_cast<uint16_t>(blockIndex(x, y, z));

        for (int m = 0; m < CHUNK_VOLUME; m++)
        {
            uint8_t mx = mortonCompact1By2(static_cast<uint16_t>(m));
            uint8_t my = mortonCompact1By2(static_cast<uint16_t>(m) >> 1);
            uint8_t mz = mortonCompact1By2(static_cast<uint16_t>(m) >> 2);
            morton[m] = static_cast<uint16_t>(blockIndex(mx, my, mz));
        }
    }
};

const TraversalOrders& traversalOrders()
{
    static TraversalOrders t;
    return t;
}

void applyRLE(const BlockID* blocks, const uint16_t* order,
              std::vector<uint8_t>& rleOut)
{
    rleOut.clear();
    rleOut.reserve(CHUNK_VOLUME);

    int i = 0;
    while (i < CHUNK_VOLUME)
    {
        BlockID cur = blocks[order[i]];
        int run = 1;
        while (i + run < CHUNK_VOLUME &&
               blocks[order[i + run]] == cur &&
               run < 255)
        {
            run++;
        }
        rleOut.push_back(static_cast<uint8_t>(run));
        rleOut.push_back(cur);
        i += run;
    }
}

bool zlibCompressRLE(const std::vector<uint8_t>& rle,
                     std::vector<uint8_t>& out, uint8_t formatByte)
{
    uLongf bound = compressBound(static_cast<uLong>(rle.size()));
    out.resize(bound + 5);

    out[0] = formatByte;
    uint32_t rleSize = static_cast<uint32_t>(rle.size());
    std::memcpy(&out[1], &rleSize, 4);

    uLongf destLen = bound;
    int rc = compress2(out.data() + 5, &destLen,
                       rle.data(), static_cast<uLong>(rle.size()),
                       Z_BEST_COMPRESSION);
    if (rc != Z_OK) return false;
    out.resize(destLen + 5);
    return true;
}

bool compressPalette(const BlockID* blocks, std::vector<uint8_t>& out)
{
    bool seen[256] = {};
    uint8_t palette[256];
    int palSize = 0;

    for (int i = 0; i < CHUNK_VOLUME; i++)
    {
        if (!seen[blocks[i]])
        {
            seen[blocks[i]] = true;
            palette[palSize++] = blocks[i];
            if (palSize > 16) return false;
        }
    }
    std::sort(palette, palette + palSize);

    uint8_t lookup[256] = {};
    for (int i = 0; i < palSize; i++)
        lookup[palette[i]] = static_cast<uint8_t>(i);

    int bpe;
    if      (palSize <= 2)  bpe = 1;
    else if (palSize <= 4)  bpe = 2;
    else                    bpe = 4;

    const uint16_t* order = traversalOrders().yMajor;
    int totalBytes = (CHUNK_VOLUME * bpe + 7) / 8;
    std::vector<uint8_t> packed(totalBytes, 0);

    int bitPos = 0;
    for (int i = 0; i < CHUNK_VOLUME; i++)
    {
        uint8_t idx   = lookup[blocks[order[i]]];
        int bytePos   = bitPos >> 3;
        int bitOffset = bitPos & 7;
        packed[bytePos] |= static_cast<uint8_t>(idx << bitOffset);
        if (bitOffset + bpe > 8)
            packed[bytePos + 1] |= static_cast<uint8_t>(idx >> (8 - bitOffset));
        bitPos += bpe;
    }

    uLongf bound = compressBound(static_cast<uLong>(packed.size()));
    out.resize(2 + palSize + 4 + bound);

    out[0] = 0x04;
    out[1] = static_cast<uint8_t>(palSize);
    std::memcpy(&out[2], palette, palSize);
    uint32_t packedLen = static_cast<uint32_t>(packed.size());
    std::memcpy(&out[2 + palSize], &packedLen, 4);

    uLongf destLen = bound;
    int rc = compress2(out.data() + 2 + palSize + 4, &destLen,
                       packed.data(), static_cast<uLong>(packed.size()),
                       Z_BEST_COMPRESSION);
    if (rc != Z_OK) return false;
    out.resize(2 + palSize + 4 + destLen);
    return true;
}

bool decompressPalette(const std::vector<uint8_t>& compressed, BlockID* outBlocks)
{
    if (compressed.size() < 2) return false;
    uint8_t palSize = compressed[1];
    if (palSize == 0 || palSize > 16) return false;
    if (compressed.size() < static_cast<size_t>(2 + palSize + 4)) return false;

    uint8_t palette[16];
    std::memcpy(palette, &compressed[2], palSize);

    int bpe;
    if      (palSize <= 2)  bpe = 1;
    else if (palSize <= 4)  bpe = 2;
    else                    bpe = 4;

    uint32_t packedLen;
    std::memcpy(&packedLen, &compressed[2 + palSize], 4);

    std::vector<uint8_t> packed(packedLen);
    uLongf destLen = packedLen;
    int rc = uncompress(packed.data(), &destLen,
                        compressed.data() + 2 + palSize + 4,
                        static_cast<uLong>(compressed.size() - 2 - palSize - 4));
    if (rc != Z_OK || destLen != packedLen) return false;

    const uint16_t* order = traversalOrders().yMajor;
    uint8_t mask = static_cast<uint8_t>((1 << bpe) - 1);
    int bitPos = 0;
    for (int i = 0; i < CHUNK_VOLUME; i++)
    {
        int bytePos   = bitPos >> 3;
        int bitOffset = bitPos & 7;
        uint8_t idx   = (packed[bytePos] >> bitOffset) & mask;
        if (bitOffset + bpe > 8 && bytePos + 1 < static_cast<int>(packed.size()))
            idx |= static_cast<uint8_t>((packed[bytePos + 1] << (8 - bitOffset)) & mask);
        if (idx >= palSize) return false;
        outBlocks[order[i]] = palette[idx];
        bitPos += bpe;
    }
    return true;
}

}

RegionFile::RegionFile(const std::string& path)
    : filePath(path), headerDirty(false)
{
    std::memset(header, 0, sizeof(header));

    bool fileExists = fs::exists(path);
    
    if (fileExists)
    {
        file.open(path, std::ios::in | std::ios::out | std::ios::binary);
        if (file.is_open())
        {
            readHeader();
        }
    }
    else
    {
        fs::create_directories(fs::path(path).parent_path());
        file.open(path, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
        if (file.is_open())
        {
            writeHeader();
        }
    }
}

RegionFile::~RegionFile()
{
    flush();
    if (file.is_open())
    {
        file.close();
    }
}

int RegionFile::getEntryIndex(int localX, int localZ) const
{
    return (localZ << REGION_SHIFT) | localX;
}

void RegionFile::readHeader()
{
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(header), HEADER_SIZE);
}

void RegionFile::writeHeader()
{
    file.seekp(0, std::ios::beg);
    file.write(reinterpret_cast<const char*>(header), HEADER_SIZE);
    file.flush();
    headerDirty = false;
}

uint32_t RegionFile::allocateSectors(uint32_t numBytes)
{
    file.seekg(0, std::ios::end);
    uint32_t endPos = static_cast<uint32_t>(file.tellg());
    
    if (endPos < HEADER_SIZE)
    {
        endPos = HEADER_SIZE;
    }
    
    uint32_t alignedEnd = ((endPos + SECTOR_SIZE - 1) / SECTOR_SIZE) * SECTOR_SIZE;
    return alignedEnd;
}

bool RegionFile::loadColumn(int localX, int localZ, ColumnData& outData)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!file.is_open())
        return false;

    int idx = getEntryIndex(localX, localZ);
    ColumnEntry& entry = header[idx];

    if (entry.offset == 0 || entry.size == 0)
        return false;

    file.seekg(entry.offset, std::ios::beg);

    uint8_t numSections = 0;
    file.read(reinterpret_cast<char*>(&numSections), 1);

    outData.sections.clear();
    outData.sections.reserve(numSections);

    for (uint8_t i = 0; i < numSections; i++)
    {
        SectionData section;
        file.read(reinterpret_cast<char*>(&section.y), 1);

        uint32_t compressedSize = 0;
        file.read(reinterpret_cast<char*>(&compressedSize), 4);

        section.compressedBlocks.resize(compressedSize);
        file.read(reinterpret_cast<char*>(section.compressedBlocks.data()), compressedSize);

        outData.sections.push_back(std::move(section));
    }

    return true;
}

void RegionFile::saveColumn(int localX, int localZ, const ColumnData& data)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (!file.is_open())
        return;

    uint32_t totalSize = 1;
    for (const auto& section : data.sections)
    {
        totalSize += 1 + 4 + static_cast<uint32_t>(section.compressedBlocks.size());
    }

    int idx = getEntryIndex(localX, localZ);
    uint32_t offset = 0;
    if (header[idx].offset != 0 && header[idx].size >= totalSize)
    {
        offset = header[idx].offset;
    }
    else
    {
        offset = allocateSectors(totalSize);
    }

    file.seekp(offset, std::ios::beg);

    uint8_t numSections = static_cast<uint8_t>(data.sections.size());
    file.write(reinterpret_cast<const char*>(&numSections), 1);

    for (const auto& section : data.sections)
    {
        file.write(reinterpret_cast<const char*>(&section.y), 1);

        uint32_t compressedSize = static_cast<uint32_t>(section.compressedBlocks.size());
        file.write(reinterpret_cast<const char*>(&compressedSize), 4);

        file.write(reinterpret_cast<const char*>(section.compressedBlocks.data()), compressedSize);
    }

    header[idx].offset = offset;
    header[idx].size = totalSize;
    headerDirty = true;

    // Ensure written column data is visible to readers immediately
    file.flush();
}

void RegionFile::flush()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (headerDirty && file.is_open())
    {
        writeHeader();
    }
}

RegionManager::RegionManager(const std::string& worldPath)
    : worldPath(worldPath)
{
    fs::create_directories(worldPath);
}

RegionManager::~RegionManager()
{
    flush();
}

std::string RegionManager::getRegionPath(int regX, int regZ) const
{
    return worldPath + "/r." + std::to_string(regX) + "." + std::to_string(regZ) + ".vox";
}

RegionFile* RegionManager::getOrOpenRegion(int regX, int regZ)
{
    RegionCoord coord(regX, regZ);

    std::lock_guard<std::mutex> lock(regionsMutex);

    auto it = regions.find(coord);
    if (it != regions.end())
    {
        return it->second.get();
    }

    std::string path = getRegionPath(regX, regZ);
    auto region = std::make_unique<RegionFile>(path);
    RegionFile* ptr = region.get();
    regions.emplace(coord, std::move(region));
    return ptr;
}

void RegionManager::compressBlocks(const BlockID* blocks, std::vector<uint8_t>& outCompressed)
{
    bool allSame = true;
    BlockID firstBlock = blocks[0];
    for (int i = 1; i < CHUNK_VOLUME && allSame; i++)
    {
        if (blocks[i] != firstBlock)
            allSame = false;
    }
    if (allSame)
    {
        outCompressed.resize(2);
        outCompressed[0] = 0xFF;
        outCompressed[1] = firstBlock;
        return;
    }

    const auto& orders = traversalOrders();

    std::vector<uint8_t> best;

    auto tryCandidate = [&](std::vector<uint8_t>& candidate)
    {
        if (!candidate.empty() && (best.empty() || candidate.size() < best.size()))
            best.swap(candidate);
    };

    {

        static uint16_t linearOrder[CHUNK_VOLUME];
        static bool once = false;
        if (!once) { for (int j = 0; j < CHUNK_VOLUME; j++) linearOrder[j] = static_cast<uint16_t>(j); once = true; }

        std::vector<uint8_t> rle, comp;
        applyRLE(blocks, linearOrder, rle);
        if (zlibCompressRLE(rle, comp, 0x01))
            tryCandidate(comp);
    }

    {
        std::vector<uint8_t> rle, comp;
        applyRLE(blocks, orders.yMajor, rle);
        if (zlibCompressRLE(rle, comp, 0x02))
            tryCandidate(comp);
    }

    {
        std::vector<uint8_t> rle, comp;
        applyRLE(blocks, orders.morton, rle);
        if (zlibCompressRLE(rle, comp, 0x03))
            tryCandidate(comp);
    }

    {
        std::vector<uint8_t> comp;
        if (compressPalette(blocks, comp))
            tryCandidate(comp);
    }

    outCompressed = std::move(best);
}

bool RegionManager::decompressBlocks(const std::vector<uint8_t>& compressed, BlockID* outBlocks)
{
    if (compressed.size() < 2)
        return false;

    uint8_t format = compressed[0];

    if (format == 0xFF)
    {
        std::memset(outBlocks, compressed[1], CHUNK_VOLUME);
        return true;
    }

    if ((format == 0x01 || format == 0x02 || format == 0x03) && compressed.size() >= 5)
    {
        uint32_t rleSize;
        std::memcpy(&rleSize, &compressed[1], 4);

        std::vector<uint8_t> rleBuffer(rleSize);
        uLongf destLen = rleSize;

        int rc = uncompress(
            rleBuffer.data(), &destLen,
            compressed.data() + 5,
            static_cast<uLong>(compressed.size() - 5));

        if (rc != Z_OK || destLen != rleSize)
            return false;

        const uint16_t* order;
        static uint16_t linearOrder[CHUNK_VOLUME];
        static bool linearInit = false;

        if (format == 0x02)
        {
            order = traversalOrders().yMajor;
        }
        else if (format == 0x03)
        {
            order = traversalOrders().morton;
        }
        else
        {
            if (!linearInit) { for (int j = 0; j < CHUNK_VOLUME; j++) linearOrder[j] = static_cast<uint16_t>(j); linearInit = true; }
            order = linearOrder;
        }

        int outIdx = 0;
        size_t i = 0;
        while (i + 1 < rleBuffer.size() && outIdx < CHUNK_VOLUME)
        {
            uint8_t runLen = rleBuffer[i];
            BlockID block   = rleBuffer[i + 1];
            for (int j = 0; j < runLen && outIdx < CHUNK_VOLUME; j++)
                outBlocks[order[outIdx++]] = block;
            i += 2;
        }
        while (outIdx < CHUNK_VOLUME)
            outBlocks[order[outIdx++]] = 0;

        return true;
    }

    if (format == 0x04)
    {
        return decompressPalette(compressed, outBlocks);
    }

    uLongf destLen = CHUNK_VOLUME;
    int rc = uncompress(
        reinterpret_cast<Bytef*>(outBlocks), &destLen,
        compressed.data(), static_cast<uLong>(compressed.size()));

    return rc == Z_OK && destLen == CHUNK_VOLUME;
}

bool RegionManager::loadChunkData(int cx, int cy, int cz, BlockID* outBlocks)
{
    int regX = cx >> REGION_SHIFT;
    int regZ = cz >> REGION_SHIFT;
    int localX = cx & REGION_MASK;
    int localZ = cz & REGION_MASK;

    RegionFile* region = getOrOpenRegion(regX, regZ);
    if (!region)
        return false;

    ColumnData columnData;
    if (!region->loadColumn(localX, localZ, columnData))
        return false;

    for (const auto& section : columnData.sections)
    {
        if (section.y == static_cast<int8_t>(cy))
        {
            return decompressBlocks(section.compressedBlocks, outBlocks);
        }
    }

    return false;
}

void RegionManager::saveChunkData(int cx, int cy, int cz, const BlockID* blocks)
{
    int regX = cx >> REGION_SHIFT;
    int regZ = cz >> REGION_SHIFT;
    int localX = cx & REGION_MASK;
    int localZ = cz & REGION_MASK;

    RegionFile* region = getOrOpenRegion(regX, regZ);
    if (!region)
        return;

    ColumnData columnData;
    region->loadColumn(localX, localZ, columnData);

    std::vector<uint8_t> compressedBlocks;
    compressBlocks(blocks, compressedBlocks);
    if (compressedBlocks.empty())
        return;

    bool found = false;
    for (auto& section : columnData.sections)
    {
        if (section.y == static_cast<int8_t>(cy))
        {
            if (section.compressedBlocks == compressedBlocks)
                return;
            section.compressedBlocks = std::move(compressedBlocks);
            found = true;
            break;
        }
    }

    if (!found)
    {
        SectionData newSection;
        newSection.y = static_cast<int8_t>(cy);
        newSection.compressedBlocks = std::move(compressedBlocks);
        columnData.sections.push_back(std::move(newSection));

        std::sort(columnData.sections.begin(), columnData.sections.end(),
            [](const SectionData& a, const SectionData& b) { return a.y < b.y; });
    }

    region->saveColumn(localX, localZ, columnData);
}

void RegionManager::flush()
{
    std::lock_guard<std::mutex> lock(regionsMutex);
    for (auto& pair : regions)
    {
        pair.second->flush();
    }
}

bool RegionManager::loadPlayerData(PlayerData& outData)
{
    std::string path = worldPath + "/player.dat";
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        return false;

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size == static_cast<std::streamsize>(sizeof(PlayerData)))
    {
        file.read(reinterpret_cast<char*>(&outData), sizeof(PlayerData));
        return file.good();
    }

    struct PlayerDataV1
    {
        float x, y, z;
        float yaw, pitch;
        float timeOfDay;
    };

    if (size == static_cast<std::streamsize>(sizeof(PlayerDataV1)))
    {
        PlayerDataV1 v1;
        file.read(reinterpret_cast<char*>(&v1), sizeof(PlayerDataV1));
        if (!file.good())
            return false;

        outData.version = 2;
        outData.x = v1.x;
        outData.y = v1.y;
        outData.z = v1.z;
        outData.yaw = v1.yaw;
        outData.pitch = v1.pitch;
        outData.timeOfDay = v1.timeOfDay;
        outData.health = 20.0f;
        outData.hunger = 20.0f;
        outData.gamemode = 0;
        return true;
    }

    return false;
}

void RegionManager::savePlayerData(const PlayerData& data)
{
    fs::create_directories(worldPath);
    std::string path = worldPath + "/player.dat";
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (file.is_open())
    {
        file.write(reinterpret_cast<const char*>(&data), sizeof(PlayerData));
    }
}