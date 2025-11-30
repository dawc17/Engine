#include "Meshing.h"
#include "BlockTypes.h"
#include "ChunkManager.h"
#include "WaterSimulator.h"
#include <glad/glad.h>
#include <cstddef>
#include <queue>

static const Vertex FACE_POS_X[4] = { {{1, 0, 0}, {1, 0}, 0, 1.0f, 1.0f}, {{1, 1, 0}, {1, 1}, 0, 1.0f, 1.0f}, {{1, 1, 1}, {0, 1}, 0, 1.0f, 1.0f}, {{1, 0, 1}, {0, 0}, 0, 1.0f, 1.0f} };
static const Vertex FACE_NEG_X[4] = { {{0, 0, 1}, {1, 0}, 0, 1.0f, 1.0f}, {{0, 1, 1}, {1, 1}, 0, 1.0f, 1.0f}, {{0, 1, 0}, {0, 1}, 0, 1.0f, 1.0f}, {{0, 0, 0}, {0, 0}, 0, 1.0f, 1.0f} };
static const Vertex FACE_POS_Y[4] = { {{0, 1, 0}, {1, 0}, 0, 1.0f, 1.0f}, {{0, 1, 1}, {1, 1}, 0, 1.0f, 1.0f}, {{1, 1, 1}, {0, 1}, 0, 1.0f, 1.0f}, {{1, 1, 0}, {0, 0}, 0, 1.0f, 1.0f} };
static const Vertex FACE_NEG_Y[4] = { {{0, 0, 1}, {1, 0}, 0, 1.0f, 1.0f}, {{0, 0, 0}, {1, 1}, 0, 1.0f, 1.0f}, {{1, 0, 0}, {0, 1}, 0, 1.0f, 1.0f}, {{1, 0, 1}, {0, 0}, 0, 1.0f, 1.0f} };
static const Vertex FACE_POS_Z[4] = { {{1, 0, 1}, {1, 0}, 0, 1.0f, 1.0f}, {{1, 1, 1}, {1, 1}, 0, 1.0f, 1.0f}, {{0, 1, 1}, {0, 1}, 0, 1.0f, 1.0f}, {{0, 0, 1}, {0, 0}, 0, 1.0f, 1.0f} };
static const Vertex FACE_NEG_Z[4] = { {{0, 0, 0}, {1, 0}, 0, 1.0f, 1.0f}, {{0, 1, 0}, {1, 1}, 0, 1.0f, 1.0f}, {{1, 1, 0}, {0, 1}, 0, 1.0f, 1.0f}, {{1, 0, 0}, {0, 0}, 0, 1.0f, 1.0f} };

static const Vertex *FACE_TABLE[6] = {
    FACE_POS_X, FACE_NEG_X,
    FACE_POS_Y, FACE_NEG_Y,
    FACE_POS_Z, FACE_NEG_Z
};

static const uint32_t FACE_INDICES[6] = {
    0, 1, 2,
    0, 2, 3};

static float getWaterHeightForMesh(BlockID block)
{
    if (!isWater(block)) return 0.0f;
    return getWaterHeight(block);
}

static void buildGreedyMesh(
    const BlockID* blocks,
    BlockGetter getBlock,
    LightGetter getSkyLight,
    std::vector<Vertex>& outVertices,
    std::vector<uint32_t>& outIndices,
    bool liquidsOnly = false)
{
  outVertices.clear();
  outIndices.clear();
  outVertices.reserve(CHUNK_VOLUME * 6 * 4);
  outIndices.reserve(CHUNK_VOLUME * 6 * 6);

  for (int dir = 0; dir < 6; dir++)
  {
    glm::ivec3 n = DIRS[dir];
    int axis = 0;
    if (n.y != 0) axis = 1;
    if (n.z != 0) axis = 2;

    int u = (axis + 1) % 3;
    int v = (axis + 2) % 3;

    BlockID mask[CHUNK_SIZE][CHUNK_SIZE];
    uint8_t lightMask[CHUNK_SIZE][CHUNK_SIZE];
    float heightMask[CHUNK_SIZE][CHUNK_SIZE];

    for (int i = 0; i < CHUNK_SIZE; i++)
    {
      for (int j = 0; j < CHUNK_SIZE; j++)
      {
        for (int k = 0; k < CHUNK_SIZE; k++)
        {
          glm::ivec3 pos;
          pos[axis] = i;
          pos[u] = k;
          pos[v] = j;

          BlockID current = blocks[blockIndex(pos.x, pos.y, pos.z)];
          bool isCurrentLiquid = g_blockTypes[current].isLiquid;

          if (liquidsOnly != isCurrentLiquid)
          {
            mask[j][k] = 0;
            lightMask[j][k] = 0;
            heightMask[j][k] = 1.0f;
            continue;
          }

          glm::ivec3 npos = pos + n;
          BlockID neighbor = getBlock(npos.x, npos.y, npos.z);
          bool isNeighborLiquid = g_blockTypes[neighbor].isLiquid;

          bool showFace = false;
          float waterHeight = 1.0f;

          if (current != 0)
          {
            if (liquidsOnly)
            {
              if (dir == 2 && !isNeighborLiquid)
              {
                showFace = true;
                waterHeight = getWaterHeightForMesh(current);
                
                BlockID above = getBlock(pos.x, pos.y + 1, pos.z);
                if (isWater(above))
                {
                    showFace = false;
                }
              }
              else if (dir != 2 && dir != 3)
              {
                if (!isNeighborLiquid && neighbor == 0)
                {
                  showFace = true;
                  waterHeight = getWaterHeightForMesh(current);
                }
                else if (isNeighborLiquid)
                {
                  uint8_t currentLevel = getWaterLevel(current);
                  uint8_t neighborLevel = getWaterLevel(neighbor);
                  if (neighborLevel < currentLevel)
                  {
                    showFace = true;
                    waterHeight = getWaterHeightForMesh(current);
                  }
                }
              }
              else if (dir == 3)
              {
                if (!isNeighborLiquid && !g_blockTypes[neighbor].solid)
                {
                  showFace = true;
                }
              }
            }
            else
            {
              if (neighbor == 0)
              {
                showFace = true;
              }
              else if (isNeighborLiquid)
              {
                showFace = true;
              }
              else if (g_blockTypes[neighbor].transparent)
              {
                if (current == neighbor && g_blockTypes[current].connectsToSame)
                {
                  showFace = false;
                }
                else if (current != neighbor)
                {
                  showFace = true;
                }
              }
            }
          }

          mask[j][k] = showFace ? current : 0;
          lightMask[j][k] = showFace ? getSkyLight(npos.x, npos.y, npos.z) : 0;
          heightMask[j][k] = waterHeight;
        }
      }

      for (int j = 0; j < CHUNK_SIZE; j++)
      {
        for (int k = 0; k < CHUNK_SIZE; k++)
        {
          if (mask[j][k] != 0)
          {
            BlockID type = mask[j][k];
            uint8_t light = lightMask[j][k];
            float height = heightMask[j][k];
            int w = 1;
            int h = 1;

            if (!liquidsOnly)
            {
              while (k + w < CHUNK_SIZE && mask[j][k + w] == type && lightMask[j][k + w] == light)
                w++;

              bool done = false;
              while (j + h < CHUNK_SIZE)
              {
                for (int dx = 0; dx < w; dx++)
                {
                  if (mask[j + h][k + dx] != type || lightMask[j + h][k + dx] != light)
                  {
                    done = true;
                    break;
                  }
                }
                if (done) break;
                h++;
              }
            }

            const Vertex *face = FACE_TABLE[dir];
            uint32_t baseIndex = static_cast<uint32_t>(outVertices.size());

            int tileIndex = g_blockTypes[type].faceTexture[dir];
            int rotation = g_blockTypes[type].faceRotation[dir];

            float faceShade = FACE_SHADE[dir];
            float skyLightNormalized = static_cast<float>(light) / static_cast<float>(MAX_SKY_LIGHT);

            int axisOffset = (n[axis] > 0) ? 1 : 0;

            for (int vIdx = 0; vIdx < 4; vIdx++)
            {
              Vertex vtx = face[vIdx];
              glm::vec3 originalPos = vtx.pos;
              glm::vec3 finalPos;

              finalPos[axis] = static_cast<float>(i + axisOffset);

              if (vtx.uv.x > 0.5f) finalPos[u] = static_cast<float>(k + w);
              else finalPos[u] = static_cast<float>(k);

              if (vtx.uv.y > 0.5f) finalPos[v] = static_cast<float>(j + h);
              else finalPos[v] = static_cast<float>(j);

              bool isTopVertex = originalPos.y > 0.5f;
              
              if (liquidsOnly && dir == 2)
              {
                finalPos.y = static_cast<float>(i) + height - 0.1f;
              }
              else if (liquidsOnly && (dir == 0 || dir == 1 || dir == 4 || dir == 5))
              {
                float baseY;
                if (dir == 0 || dir == 1)
                {
                  baseY = static_cast<float>(k);
                }
                else
                {
                  baseY = static_cast<float>(j);
                }
                
                if (isTopVertex)
                {
                  finalPos.y = baseY + height - 0.1f;
                }
                else
                {
                  finalPos.y = baseY;
                }
              }

              vtx.pos = finalPos;

              float localU = (vtx.uv.x > 0.5f) ? static_cast<float>(w) : 0.0f;
              float localV = (vtx.uv.y > 0.5f) ? static_cast<float>(h) : 0.0f;

              if (liquidsOnly && (dir == 0 || dir == 1 || dir == 4 || dir == 5))
              {
                localV = isTopVertex ? height : 0.0f;
              }

              switch (rotation)
              {
                case 1:
                  {
                    float tmp = localU;
                    localU = localV;
                    localV = static_cast<float>(w) - tmp;
                  }
                  break;
                case 2:
                  localV = static_cast<float>(h) - localV;
                  break;
                case 3:
                  {
                    float tmp = localU;
                    localU = static_cast<float>(h) - localV;
                    localV = tmp;
                  }
                  break;
                default:
                  break;
              }

              vtx.uv = glm::vec2(localU, localV);
              vtx.tileIndex = static_cast<float>(tileIndex);
              vtx.skyLight = skyLightNormalized;
              vtx.faceShade = faceShade;

              outVertices.push_back(vtx);
            }

            for (int idx = 0; idx < 6; idx++)
              outIndices.push_back(baseIndex + FACE_INDICES[idx]);

            for (int dy = 0; dy < h; dy++)
            {
              for (int dx = 0; dx < w; dx++)
              {
                mask[j + dy][k + dx] = 0;
                lightMask[j + dy][k + dx] = 0;
              }
            }
          }
        }
      }
    }
  }
}

void calculateSkyLight(Chunk &c, ChunkManager &chunkManager)
{
  auto getBlockWorld = [&](int x, int y, int z) -> BlockID
  {
    if (x >= 0 && x < CHUNK_SIZE &&
        y >= 0 && y < CHUNK_SIZE &&
        z >= 0 && z < CHUNK_SIZE)
    {
      return c.blocks[blockIndex(x, y, z)];
    }
    
    int neighborCX = c.position.x;
    int neighborCY = c.position.y;
    int neighborCZ = c.position.z;
    int localX = x, localY = y, localZ = z;
    
    if (x < 0) { neighborCX--; localX = CHUNK_SIZE + x; }
    else if (x >= CHUNK_SIZE) { neighborCX++; localX = x - CHUNK_SIZE; }
    if (y < 0) { neighborCY--; localY = CHUNK_SIZE + y; }
    else if (y >= CHUNK_SIZE) { neighborCY++; localY = y - CHUNK_SIZE; }
    if (z < 0) { neighborCZ--; localZ = CHUNK_SIZE + z; }
    else if (z >= CHUNK_SIZE) { neighborCZ++; localZ = z - CHUNK_SIZE; }
    
    Chunk *neighbor = chunkManager.getChunk(neighborCX, neighborCY, neighborCZ);
    if (!neighbor) return 0;
    return neighbor->blocks[blockIndex(localX, localY, localZ)];
  };

  std::fill(std::begin(c.skyLight), std::end(c.skyLight), 0);

  std::queue<glm::ivec3> lightQueue;

  Chunk *chunkAbove = chunkManager.getChunk(c.position.x, c.position.y + 1, c.position.z);
  
  for (int x = 0; x < CHUNK_SIZE; x++)
  {
    for (int z = 0; z < CHUNK_SIZE; z++)
    {
      uint8_t incomingLight = MAX_SKY_LIGHT;
      if (chunkAbove)
      {
        BlockID blockAbove = chunkAbove->blocks[blockIndex(x, 0, z)];
        if (blockAbove != 0 && !g_blockTypes[blockAbove].transparent)
        {
          incomingLight = chunkAbove->skyLight[blockIndex(x, 0, z)];
        }
        else
        {
          incomingLight = chunkAbove->skyLight[blockIndex(x, 0, z)];
        }
      }
      
      uint8_t currentLight = incomingLight;
      for (int y = CHUNK_SIZE - 1; y >= 0; y--)
      {
        int idx = blockIndex(x, y, z);
        BlockID block = c.blocks[idx];
        
        if (block == 0)
        {
          c.skyLight[idx] = currentLight;
          if (currentLight > 1)
            lightQueue.push({x, y, z});
        }
        else if (g_blockTypes[block].transparent)
        {
          if (currentLight > 0 && (y % 2 == 0))
            currentLight = currentLight > 1 ? currentLight - 1 : currentLight;
          c.skyLight[idx] = currentLight;
          if (currentLight > 1)
            lightQueue.push({x, y, z});
        }
        else
        {
          currentLight = 0;
          c.skyLight[idx] = 0;
        }
      }
    }
  }

  while (!lightQueue.empty())
  {
    glm::ivec3 pos = lightQueue.front();
    lightQueue.pop();
    
    int idx = blockIndex(pos.x, pos.y, pos.z);
    uint8_t currentLight = c.skyLight[idx];
    
    if (currentLight <= 1) continue;
    
    const glm::ivec3 dirs[6] = {{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}};
    
    for (const auto& dir : dirs)
    {
      int nx = pos.x + dir.x;
      int ny = pos.y + dir.y;
      int nz = pos.z + dir.z;
      
      if (nx < 0 || nx >= CHUNK_SIZE ||
          ny < 0 || ny >= CHUNK_SIZE ||
          nz < 0 || nz >= CHUNK_SIZE)
        continue;
      
      int nidx = blockIndex(nx, ny, nz);
      BlockID neighborBlock = c.blocks[nidx];
      
      if (neighborBlock != 0 && !g_blockTypes[neighborBlock].transparent)
        continue;
      
      uint8_t attenuation = 1;
      uint8_t newLight = (currentLight > attenuation) ? currentLight - attenuation : 0;
      
      if (newLight > c.skyLight[nidx])
      {
        c.skyLight[nidx] = newLight;
        if (newLight > 1)
          lightQueue.push({nx, ny, nz});
      }
    }
  }
  
  c.dirtyLight = false;
}

void buildChunkMesh(Chunk &c, ChunkManager &chunkManager)
{
  if (c.dirtyLight)
  {
    calculateSkyLight(c, chunkManager);
  }

  auto getBlock = [&](int x, int y, int z) -> BlockID
  {
    if (x >= 0 && x < CHUNK_SIZE &&
        y >= 0 && y < CHUNK_SIZE &&
        z >= 0 && z < CHUNK_SIZE)
    {
      return c.blocks[blockIndex(x, y, z)];
    }
    
    int neighborCX = c.position.x;
    int neighborCY = c.position.y;
    int neighborCZ = c.position.z;
    int localX = x;
    int localY = y;
    int localZ = z;
    
    if (x < 0) { neighborCX--; localX = CHUNK_SIZE + x; }
    else if (x >= CHUNK_SIZE) { neighborCX++; localX = x - CHUNK_SIZE; }
    
    if (y < 0) { neighborCY--; localY = CHUNK_SIZE + y; }
    else if (y >= CHUNK_SIZE) { neighborCY++; localY = y - CHUNK_SIZE; }
    
    if (z < 0) { neighborCZ--; localZ = CHUNK_SIZE + z; }
    else if (z >= CHUNK_SIZE) { neighborCZ++; localZ = z - CHUNK_SIZE; }
    
    Chunk *neighbor = chunkManager.getChunk(neighborCX, neighborCY, neighborCZ);
    if (neighbor == nullptr)
    {
      return 0;
    }
    
    return neighbor->blocks[blockIndex(localX, localY, localZ)];
  };

  auto getSkyLight = [&](int x, int y, int z) -> uint8_t
  {
    if (x >= 0 && x < CHUNK_SIZE &&
        y >= 0 && y < CHUNK_SIZE &&
        z >= 0 && z < CHUNK_SIZE)
    {
      return c.skyLight[blockIndex(x, y, z)];
    }
    
    int neighborCX = c.position.x;
    int neighborCY = c.position.y;
    int neighborCZ = c.position.z;
    int localX = x, localY = y, localZ = z;
    
    if (x < 0) { neighborCX--; localX = CHUNK_SIZE + x; }
    else if (x >= CHUNK_SIZE) { neighborCX++; localX = x - CHUNK_SIZE; }
    if (y < 0) { neighborCY--; localY = CHUNK_SIZE + y; }
    else if (y >= CHUNK_SIZE) { neighborCY++; localY = y - CHUNK_SIZE; }
    if (z < 0) { neighborCZ--; localZ = CHUNK_SIZE + z; }
    else if (z >= CHUNK_SIZE) { neighborCZ++; localZ = z - CHUNK_SIZE; }
    
    Chunk *neighbor = chunkManager.getChunk(neighborCX, neighborCY, neighborCZ);
    if (!neighbor) return MAX_SKY_LIGHT;
    return neighbor->skyLight[blockIndex(localX, localY, localZ)];
  };

  std::vector<Vertex> verts;
  std::vector<uint32_t> inds;
  std::vector<Vertex> waterVerts;
  std::vector<uint32_t> waterInds;
  
  buildGreedyMesh(c.blocks, getBlock, getSkyLight, verts, inds, false);
  buildGreedyMesh(c.blocks, getBlock, getSkyLight, waterVerts, waterInds, true);
  
  uploadToGPU(c, verts, inds);
  uploadWaterToGPU(c, waterVerts, waterInds);
}

void uploadToGPU(Chunk &c, const std::vector<Vertex> &verts, const std::vector<uint32_t> &inds)
{
  glBindVertexArray(c.vao);

  glBindBuffer(GL_ARRAY_BUFFER, c.vbo);
  glBufferData(GL_ARRAY_BUFFER,
               verts.size() * sizeof(Vertex),
               verts.data(),
               GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, c.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               inds.size() * sizeof(uint32_t),
               inds.data(),
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, pos));
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, uv));
  glEnableVertexAttribArray(1);

  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, tileIndex));
  glEnableVertexAttribArray(2);

  glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, skyLight));
  glEnableVertexAttribArray(3);

  glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, faceShade));
  glEnableVertexAttribArray(4);

  c.indexCount = static_cast<uint32_t>(inds.size());
  c.vertexCount = static_cast<uint32_t>(verts.size());
}

void uploadWaterToGPU(Chunk &c, const std::vector<Vertex> &verts, const std::vector<uint32_t> &inds)
{
  if (verts.empty())
  {
    c.waterIndexCount = 0;
    c.waterVertexCount = 0;
    return;
  }

  if (c.waterVao == 0)
  {
    glGenVertexArrays(1, &c.waterVao);
    glGenBuffers(1, &c.waterVbo);
    glGenBuffers(1, &c.waterEbo);
  }

  glBindVertexArray(c.waterVao);

  glBindBuffer(GL_ARRAY_BUFFER, c.waterVbo);
  glBufferData(GL_ARRAY_BUFFER,
               verts.size() * sizeof(Vertex),
               verts.data(),
               GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, c.waterEbo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               inds.size() * sizeof(uint32_t),
               inds.data(),
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, pos));
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, uv));
  glEnableVertexAttribArray(1);

  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, tileIndex));
  glEnableVertexAttribArray(2);

  glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, skyLight));
  glEnableVertexAttribArray(3);

  glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, faceShade));
  glEnableVertexAttribArray(4);

  c.waterIndexCount = static_cast<uint32_t>(inds.size());
  c.waterVertexCount = static_cast<uint32_t>(verts.size());
}

void buildChunkMeshOffThread(
    const BlockID* blocks,
    const uint8_t* skyLight,
    BlockGetter getBlock,
    LightGetter getSkyLight,
    std::vector<Vertex>& outVertices,
    std::vector<uint32_t>& outIndices,
    std::vector<Vertex>& outWaterVertices,
    std::vector<uint32_t>& outWaterIndices)
{
  buildGreedyMesh(blocks, getBlock, getSkyLight, outVertices, outIndices, false);
  buildGreedyMesh(blocks, getBlock, getSkyLight, outWaterVertices, outWaterIndices, true);
}
