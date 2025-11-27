#include "Meshing.h"
#include "BlockTypes.h"
#include "ChunkManager.h"
#include <glad/glad.h>
#include <cstddef> // for offsetof
#include <queue>

// Face templates now include default light value of 1.0
static const Vertex FACE_POS_X[4] = { {{1, 0, 0}, {1, 0}, 0, 1.0f}, {{1, 1, 0}, {1, 1}, 0, 1.0f}, {{1, 1, 1}, {0, 1}, 0, 1.0f}, {{1, 0, 1}, {0, 0}, 0, 1.0f} };
static const Vertex FACE_NEG_X[4] = { {{0, 0, 1}, {1, 0}, 0, 1.0f}, {{0, 1, 1}, {1, 1}, 0, 1.0f}, {{0, 1, 0}, {0, 1}, 0, 1.0f}, {{0, 0, 0}, {0, 0}, 0, 1.0f} };
static const Vertex FACE_POS_Y[4] = { {{0, 1, 0}, {1, 0}, 0, 1.0f}, {{0, 1, 1}, {1, 1}, 0, 1.0f}, {{1, 1, 1}, {0, 1}, 0, 1.0f}, {{1, 1, 0}, {0, 0}, 0, 1.0f} };
static const Vertex FACE_NEG_Y[4] = { {{0, 0, 1}, {1, 0}, 0, 1.0f}, {{0, 0, 0}, {1, 1}, 0, 1.0f}, {{1, 0, 0}, {0, 1}, 0, 1.0f}, {{1, 0, 1}, {0, 0}, 0, 1.0f} };
static const Vertex FACE_POS_Z[4] = { {{1, 0, 1}, {1, 0}, 0, 1.0f}, {{1, 1, 1}, {1, 1}, 0, 1.0f}, {{0, 1, 1}, {0, 1}, 0, 1.0f}, {{0, 0, 1}, {0, 0}, 0, 1.0f} };
static const Vertex FACE_NEG_Z[4] = { {{0, 0, 0}, {1, 0}, 0, 1.0f}, {{0, 1, 0}, {1, 1}, 0, 1.0f}, {{1, 1, 0}, {0, 1}, 0, 1.0f}, {{1, 0, 0}, {0, 0}, 0, 1.0f} };

static const Vertex *FACE_TABLE[6] = {
    FACE_POS_X, FACE_NEG_X,
    FACE_POS_Y, FACE_NEG_Y,
    FACE_POS_Z, FACE_NEG_Z
};

static const uint32_t FACE_INDICES[6] = {
    0, 1, 2,
    0, 2, 3};

// Calculate skylight for a chunk with proper propagation
void calculateSkyLight(Chunk &c, ChunkManager &chunkManager)
{
  // Helper to get block at position (handles neighbor chunks)
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
    if (!neighbor) return 0;  // Air outside loaded area
    return neighbor->blocks[blockIndex(localX, localY, localZ)];
  };

  // Helper to get skylight at position (handles neighbor chunks)
  auto getSkyLightWorld = [&](int x, int y, int z) -> uint8_t
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
    if (!neighbor) return MAX_SKY_LIGHT;  // Full light outside (sky above)
    return neighbor->skyLight[blockIndex(localX, localY, localZ)];
  };

  // Initialize all to 0 (dark)
  std::fill(std::begin(c.skyLight), std::end(c.skyLight), 0);

  // BFS queue for light propagation
  std::queue<glm::ivec3> lightQueue;

  // Phase 1: Direct sunlight - cast rays from top of chunk downward
  // Check if chunk above exists
  Chunk *chunkAbove = chunkManager.getChunk(c.position.x, c.position.y + 1, c.position.z);
  
  for (int x = 0; x < CHUNK_SIZE; x++)
  {
    for (int z = 0; z < CHUNK_SIZE; z++)
    {
      // Get incoming light from above (from chunk above or sky)
      uint8_t incomingLight = MAX_SKY_LIGHT;
      if (chunkAbove)
      {
        // Check the bottom of the chunk above
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
      
      // Propagate light downward through this column
      uint8_t currentLight = incomingLight;
      for (int y = CHUNK_SIZE - 1; y >= 0; y--)
      {
        int idx = blockIndex(x, y, z);
        BlockID block = c.blocks[idx];
        
        if (block == 0)
        {
          // Air - light passes through fully
          c.skyLight[idx] = currentLight;
          if (currentLight > 1)
            lightQueue.push({x, y, z});
        }
        else if (g_blockTypes[block].transparent)
        {
          // Transparent block (leaves) - light passes with minimal reduction
          // Only reduce every other block to let more light through canopies
          if (currentLight > 0 && (y % 2 == 0))
            currentLight = currentLight > 1 ? currentLight - 1 : currentLight;
          c.skyLight[idx] = currentLight;
          if (currentLight > 1)
            lightQueue.push({x, y, z});
        }
        else
        {
          // Solid opaque block - stops light
          currentLight = 0;
          c.skyLight[idx] = 0;
        }
      }
    }
  }

  // Phase 2: Horizontal propagation using BFS
  // Light spreads sideways with attenuation
  while (!lightQueue.empty())
  {
    glm::ivec3 pos = lightQueue.front();
    lightQueue.pop();
    
    int idx = blockIndex(pos.x, pos.y, pos.z);
    uint8_t currentLight = c.skyLight[idx];
    
    if (currentLight <= 1) continue;  // Not enough light to spread
    
    // Check all 6 neighbors (but mainly horizontal for shadow spread)
    const glm::ivec3 dirs[6] = {{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}};
    
    for (const auto& dir : dirs)
    {
      int nx = pos.x + dir.x;
      int ny = pos.y + dir.y;
      int nz = pos.z + dir.z;
      
      // Only process blocks within this chunk for now
      if (nx < 0 || nx >= CHUNK_SIZE ||
          ny < 0 || ny >= CHUNK_SIZE ||
          nz < 0 || nz >= CHUNK_SIZE)
        continue;
      
      int nidx = blockIndex(nx, ny, nz);
      BlockID neighborBlock = c.blocks[nidx];
      
      // Can't propagate into solid blocks
      if (neighborBlock != 0 && !g_blockTypes[neighborBlock].transparent)
        continue;
      
      // Light diminishes by 1 each horizontal step
      uint8_t attenuation = 1;
      uint8_t newLight = (currentLight > attenuation) ? currentLight - attenuation : 0;
      
      // Only update if we're bringing more light
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
  // Calculate lighting if needed
  if (c.dirtyLight)
  {
    calculateSkyLight(c, chunkManager);
  }

  // Lambda to get block at local position, checking neighboring chunks at boundaries
  auto getBlock = [&](int x, int y, int z) -> BlockID
  {
    // If within chunk bounds, return directly
    if (x >= 0 && x < CHUNK_SIZE &&
        y >= 0 && y < CHUNK_SIZE &&
        z >= 0 && z < CHUNK_SIZE)
    {
      return c.blocks[blockIndex(x, y, z)];
    }
    
    // Otherwise, calculate which neighbor chunk and the local position within it
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
    
    // Get the neighbor chunk
    Chunk *neighbor = chunkManager.getChunk(neighborCX, neighborCY, neighborCZ);
    if (neighbor == nullptr)
    {
      return 0; // Treat unloaded chunks as air (will be fixed when neighbor loads)
    }
    
    return neighbor->blocks[blockIndex(localX, localY, localZ)];
  };

  // Lambda to get skylight at position (handles neighbor chunks)
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

  // Lambda to check if a block is solid (for AO calculation)
  auto isSolid = [&](int x, int y, int z) -> bool
  {
    BlockID block = getBlock(x, y, z);
    return block != 0 && !g_blockTypes[block].transparent;
  };

  std::vector<Vertex> verts;
  verts.reserve(CHUNK_VOLUME * 6 * 4); // worst case: six quads per block, four verts per quad
  std::vector<uint32_t> inds;
  inds.reserve(CHUNK_VOLUME * 6 * 6); // six indices per quad

  // Greedy meshing
  for (int dir = 0; dir < 6; dir++)
  {
    glm::ivec3 n = DIRS[dir];
    int axis = 0;
    if (n.y != 0) axis = 1;
    if (n.z != 0) axis = 2;

    int u = (axis + 1) % 3;
    int v = (axis + 2) % 3;

    // 2D mask for the slice
    // 2D mask for the slice - just block type, no AO
    BlockID mask[CHUNK_SIZE][CHUNK_SIZE];

    // Iterate through the chunk along the main axis
    for (int i = 0; i < CHUNK_SIZE; i++)
    {
      // 1. Compute mask
      for (int j = 0; j < CHUNK_SIZE; j++) // v
      {
        for (int k = 0; k < CHUNK_SIZE; k++) // u
        {
          glm::ivec3 pos;
          pos[axis] = i;
          pos[u] = k;
          pos[v] = j;

          BlockID current = c.blocks[blockIndex(pos.x, pos.y, pos.z)];
          
          glm::ivec3 npos = pos + n;
          BlockID neighbor = getBlock(npos.x, npos.y, npos.z);

          // Show face if current is solid and neighbor is air/transparent
          bool showFace = false;
          if (current != 0)
          {
            if (neighbor == 0)
            {
              showFace = true;
            }
            else if (g_blockTypes[neighbor].transparent)
            {
              if (current != neighbor)
              {
                showFace = true;
              }
            }
          }

          mask[j][k] = showFace ? current : 0;
        }
      }

      // 2. Greedy meshing - merge by block type only
      for (int j = 0; j < CHUNK_SIZE; j++)
      {
        for (int k = 0; k < CHUNK_SIZE; k++)
        {
          if (mask[j][k] != 0)
          {
            BlockID type = mask[j][k];
            int w = 1;
            int h = 1;

            // Compute width
            while (k + w < CHUNK_SIZE && mask[j][k + w] == type)
              w++;

            // Compute height
            bool done = false;
            while (j + h < CHUNK_SIZE)
            {
              for (int dx = 0; dx < w; dx++)
              {
                if (mask[j + h][k + dx] != type)
                {
                  done = true;
                  break;
                }
              }
              if (done) break;
              h++;
            }

            // Add quad
            const Vertex *face = FACE_TABLE[dir];
            uint32_t baseIndex = verts.size();

            int tileIndex = g_blockTypes[type].faceTexture[dir];
            int rotation = g_blockTypes[type].faceRotation[dir];

            // Simple face-based shading - same light for all vertices
            float faceShade = FACE_SHADE[dir];

            int axisOffset = (n[axis] > 0) ? 1 : 0;

            for (int vIdx = 0; vIdx < 4; vIdx++)
            {
              Vertex vtx = face[vIdx];
              glm::vec3 finalPos;

              finalPos[axis] = i + axisOffset;

              if (vtx.uv.x > 0.5f) finalPos[u] = k + w;
              else finalPos[u] = k;

              if (vtx.uv.y > 0.5f) finalPos[v] = j + h;
              else finalPos[v] = j;

              vtx.pos = finalPos;
              
              float localU = (vtx.uv.x > 0.5f) ? static_cast<float>(w) : 0.0f;
              float localV = (vtx.uv.y > 0.5f) ? static_cast<float>(h) : 0.0f;
              
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
              vtx.light = faceShade;  // Same light for entire face

              verts.push_back(vtx);
            }

            for (int idx = 0; idx < 6; idx++)
              inds.push_back(baseIndex + FACE_INDICES[idx]);

            // Clear mask
            for (int dy = 0; dy < h; dy++)
              for (int dx = 0; dx < w; dx++)
                mask[j + dy][k + dx] = 0;
          }
        }
      }
    }
  }

  uploadToGPU(c, verts, inds);
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
                        (void *)offsetof(Vertex, light));
  glEnableVertexAttribArray(3);

  c.indexCount = inds.size();
  c.vertexCount = verts.size();
}

void buildChunkMeshOffThread(
    const BlockID* blocks,
    BlockGetter getBlock,
    std::vector<Vertex>& outVertices,
    std::vector<uint32_t>& outIndices)
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

          glm::ivec3 npos = pos + n;
          BlockID neighbor = getBlock(npos.x, npos.y, npos.z);

          bool showFace = false;
          if (current != 0)
          {
            if (neighbor == 0)
            {
              showFace = true;
            }
            else if (g_blockTypes[neighbor].transparent)
            {
              if (current != neighbor)
              {
                showFace = true;
              }
            }
          }

          mask[j][k] = showFace ? current : 0;
        }
      }

      for (int j = 0; j < CHUNK_SIZE; j++)
      {
        for (int k = 0; k < CHUNK_SIZE; k++)
        {
          if (mask[j][k] != 0)
          {
            BlockID type = mask[j][k];
            int w = 1;
            int h = 1;

            while (k + w < CHUNK_SIZE && mask[j][k + w] == type)
              w++;

            bool done = false;
            while (j + h < CHUNK_SIZE)
            {
              for (int dx = 0; dx < w; dx++)
              {
                if (mask[j + h][k + dx] != type)
                {
                  done = true;
                  break;
                }
              }
              if (done) break;
              h++;
            }

            const Vertex *face = FACE_TABLE[dir];
            uint32_t baseIndex = outVertices.size();

            int tileIndex = g_blockTypes[type].faceTexture[dir];
            int rotation = g_blockTypes[type].faceRotation[dir];

            float faceShade = FACE_SHADE[dir];

            int axisOffset = (n[axis] > 0) ? 1 : 0;

            for (int vIdx = 0; vIdx < 4; vIdx++)
            {
              Vertex vtx = face[vIdx];
              glm::vec3 finalPos;

              finalPos[axis] = i + axisOffset;

              if (vtx.uv.x > 0.5f) finalPos[u] = k + w;
              else finalPos[u] = k;

              if (vtx.uv.y > 0.5f) finalPos[v] = j + h;
              else finalPos[v] = j;

              vtx.pos = finalPos;

              float localU = (vtx.uv.x > 0.5f) ? static_cast<float>(w) : 0.0f;
              float localV = (vtx.uv.y > 0.5f) ? static_cast<float>(h) : 0.0f;

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
              vtx.light = faceShade;

              outVertices.push_back(vtx);
            }

            for (int idx = 0; idx < 6; idx++)
              outIndices.push_back(baseIndex + FACE_INDICES[idx]);

            for (int dy = 0; dy < h; dy++)
              for (int dx = 0; dx < w; dx++)
                mask[j + dy][k + dx] = 0;
          }
        }
      }
    }
  }
}