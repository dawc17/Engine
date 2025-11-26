#include "Meshing.h"
#include "BlockTypes.h"
#include <glad/glad.h>
#include <cstddef> // for offsetof

static const Vertex FACE_POS_X[4] = { {{1, 0, 0}, {1, 0}, 0}, {{1, 1, 0}, {1, 1}, 0}, {{1, 1, 1}, {0, 1}, 0}, {{1, 0, 1}, {0, 0}, 0} };
static const Vertex FACE_NEG_X[4] = { {{0, 0, 1}, {1, 0}, 0}, {{0, 1, 1}, {1, 1}, 0}, {{0, 1, 0}, {0, 1}, 0}, {{0, 0, 0}, {0, 0}, 0} };
static const Vertex FACE_POS_Y[4] = { {{0, 1, 0}, {1, 0}, 0}, {{0, 1, 1}, {1, 1}, 0}, {{1, 1, 1}, {0, 1}, 0}, {{1, 1, 0}, {0, 0}, 0} };
static const Vertex FACE_NEG_Y[4] = { {{0, 0, 1}, {1, 0}, 0}, {{0, 0, 0}, {1, 1}, 0}, {{1, 0, 0}, {0, 1}, 0}, {{1, 0, 1}, {0, 0}, 0} };
static const Vertex FACE_POS_Z[4] = { {{1, 0, 1}, {1, 0}, 0}, {{1, 1, 1}, {1, 1}, 0}, {{0, 1, 1}, {0, 1}, 0}, {{0, 0, 1}, {0, 0}, 0} };
static const Vertex FACE_NEG_Z[4] = { {{0, 0, 0}, {1, 0}, 0}, {{0, 1, 0}, {1, 1}, 0}, {{1, 1, 0}, {0, 1}, 0}, {{1, 0, 0}, {0, 0}, 0} };

static const Vertex *FACE_TABLE[6] = {
    FACE_POS_X, FACE_NEG_X,
    FACE_POS_Y, FACE_NEG_Y,
    FACE_POS_Z, FACE_NEG_Z
};

static const uint32_t FACE_INDICES[6] = {
    0, 1, 2,
    0, 2, 3};

void buildChunkMesh(Chunk &c)
{
  auto getBlock = [&](int x, int y, int z) -> BlockID
  {
    if (x < 0 || x >= CHUNK_SIZE ||
        y < 0 || y >= CHUNK_SIZE ||
        z < 0 || z >= CHUNK_SIZE)
      return 0; // treat out-of-bounds as air for now
    return c.blocks[blockIndex(x, y, z)];
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

          if (current != 0 && neighbor == 0)
          {
            mask[j][k] = current;
          }
          else
          {
            mask[j][k] = 0;
          }
        }
      }

      // 2. Greedy meshing on mask
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
            {
              w++;
            }

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

            // Get the tile index and rotation for this block type and face direction
            int tileIndex = g_blockTypes[type].faceTexture[dir];
            int rotation = g_blockTypes[type].faceRotation[dir];

            // Determine if this is a positive or negative facing direction
            // For positive dirs (+X,+Y,+Z), face is on far side of slice (i+1)
            // For negative dirs (-X,-Y,-Z), face is on near side of slice (i)
            int axisOffset = (n[axis] > 0) ? 1 : 0;

            for (int vIdx = 0; vIdx < 4; vIdx++)
            {
              Vertex vtx = face[vIdx];
              glm::vec3 finalPos;

              // Place the face at the correct position along the axis
              finalPos[axis] = i + axisOffset;

              // U axis - use the face template's uv.x to determine which edge (0 or w)
              if (vtx.uv.x > 0.5f) finalPos[u] = k + w;
              else finalPos[u] = k;

              // V axis - use the face template's uv.y to determine which edge (0 or h)
              if (vtx.uv.y > 0.5f) finalPos[v] = j + h;
              else finalPos[v] = j;

              vtx.pos = finalPos;
              
              // Build UVs in block units so the shader can repeat the atlas tile across merged quads
              float localU = (vtx.uv.x > 0.5f) ? static_cast<float>(w) : 0.0f;
              float localV = (vtx.uv.y > 0.5f) ? static_cast<float>(h) : 0.0f;
              
              // Apply rotation/flip to UVs (0=normal, 1=90° CCW, 2=flip vertical, 3=90° CW)
              switch (rotation)
              {
                case 1: // 90° CCW - swap and flip
                  {
                    float tmp = localU;
                    localU = localV;
                    localV = static_cast<float>(w) - tmp;
                  }
                  break;
                case 2: // Flip vertical (upside down)
                  localV = static_cast<float>(h) - localV;
                  break;
                case 3: // 90° CW - swap and flip
                  {
                    float tmp = localU;
                    localU = static_cast<float>(h) - localV;
                    localV = tmp;
                  }
                  break;
                default: // 0 - no rotation
                  break;
              }
              
              vtx.uv = glm::vec2(localU, localV);
              vtx.tileIndex = static_cast<float>(tileIndex);

              verts.push_back(vtx);
            }

            for (int idx = 0; idx < 6; idx++)
              inds.push_back(baseIndex + FACE_INDICES[idx]);

            // Clear mask
            for (int dy = 0; dy < h; dy++)
            {
              for (int dx = 0; dx < w; dx++)
              {
                mask[j + dy][k + dx] = 0;
              }
            }
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

  c.indexCount = inds.size();
  c.vertexCount = verts.size();
}
