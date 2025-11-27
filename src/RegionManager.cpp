#include "RegionManager.h"
#include <filesystem>
#include <cstring>
#include <algorithm>
#include "../libs/zlib-1.3.1/zlib.h"

namespace fs = std::filesystem;

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

    uint32_t offset = allocateSectors(totalSize);

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

    int idx = getEntryIndex(localX, localZ);
    header[idx].offset = offset;
    header[idx].size = totalSize;
    headerDirty = true;

    writeHeader();
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
    RegionCoord coord{regX, regZ};

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
    uLongf compressedSize = compressBound(CHUNK_VOLUME);
    outCompressed.resize(compressedSize);

    int result = compress2(
        outCompressed.data(),
        &compressedSize,
        reinterpret_cast<const Bytef*>(blocks),
        CHUNK_VOLUME,
        Z_DEFAULT_COMPRESSION
    );

    if (result == Z_OK)
    {
        outCompressed.resize(compressedSize);
    }
    else
    {
        outCompressed.clear();
    }
}

bool RegionManager::decompressBlocks(const std::vector<uint8_t>& compressed, BlockID* outBlocks)
{
    uLongf destLen = CHUNK_VOLUME;

    int result = uncompress(
        reinterpret_cast<Bytef*>(outBlocks),
        &destLen,
        compressed.data(),
        static_cast<uLong>(compressed.size())
    );

    return result == Z_OK && destLen == CHUNK_VOLUME;
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

    bool found = false;
    for (auto& section : columnData.sections)
    {
        if (section.y == static_cast<int8_t>(cy))
        {
            compressBlocks(blocks, section.compressedBlocks);
            found = true;
            break;
        }
    }

    if (!found)
    {
        SectionData newSection;
        newSection.y = static_cast<int8_t>(cy);
        compressBlocks(blocks, newSection.compressedBlocks);
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

