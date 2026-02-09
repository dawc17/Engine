#include "CaveGenerator.h"
#include <array>
#include <cmath>

// Lightweight Perlin 3D implementation with reseeding support.
class Perlin3D
{
public:
  Perlin3D() = default;
  explicit Perlin3D(uint32_t seed) { reseed(seed); }

  void reseed(uint32_t seed)
  {
    uint32_t x = seed;
    for (int i = 0; i < 256; ++i)
    {
      x ^= x << 13; x ^= x >> 17; x ^= x << 5;
      perm[i] = static_cast<uint8_t>(x & 255u);
    }
    // Fisher-Yates shuffle for better distribution
    for (int i = 255; i > 0; --i)
    {
      x ^= x << 13; x ^= x >> 17; x ^= x << 5;
      uint32_t j = x % static_cast<uint32_t>(i + 1);
      std::swap(perm[i], perm[j]);
    }
    for (int i = 0; i < 256; ++i)
      perm[256 + i] = perm[i];
  }

  float noise(float x, float y, float z) const
  {
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;
    int Z = static_cast<int>(std::floor(z)) & 255;

    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);

    float u = fade(x);
    float v = fade(y);
    float w = fade(z);

    int A  = perm[X] + Y;
    int AA = perm[A] + Z;
    int AB = perm[A + 1] + Z;
    int B  = perm[X + 1] + Y;
    int BA = perm[B] + Z;
    int BB = perm[B + 1] + Z;

    float g000 = grad(perm[AA],     x,     y,     z    );
    float g100 = grad(perm[BA],     x-1.0f,y,     z    );
    float g010 = grad(perm[AB],     x,     y-1.0f,z    );
    float g110 = grad(perm[BB],     x-1.0f,y-1.0f,z    );
    float g001 = grad(perm[AA+1],   x,     y,     z-1.0f);
    float g101 = grad(perm[BA+1],   x-1.0f,y,     z-1.0f);
    float g011 = grad(perm[AB+1],   x,     y-1.0f,z-1.0f);
    float g111 = grad(perm[BB+1],   x-1.0f,y-1.0f,z-1.0f);

    float lerpX1 = lerp(u, g000, g100);
    float lerpX2 = lerp(u, g010, g110);
    float lerpX3 = lerp(u, g001, g101);
    float lerpX4 = lerp(u, g011, g111);

    float lerpY1 = lerp(v, lerpX1, lerpX2);
    float lerpY2 = lerp(v, lerpX3, lerpX4);

    return lerp(w, lerpY1, lerpY2); // ~[-1,1]
  }

  float fbm(float x, float y, float z, int octaves, float gain, float freq) const
  {
    float amp = 1.0f;
    float sum = 0.0f;
    for (int i = 0; i < octaves; ++i)
    {
      sum += amp * noise(x * freq, y * freq, z * freq);
      freq *= 2.0f;
      amp *= gain;
    }
    return sum;
  }

private:
  std::array<uint8_t, 512> perm{};

  static inline float fade(float t) { return t*t*t*(t*(t*6.0f-15.0f)+10.0f); }
  static inline float lerp(float t, float a, float b) { return a + t * (b - a); }
  static inline float grad(uint8_t h, float x, float y, float z)
  {
    int g = h & 15;
    float u = g < 8 ? x : y;
    float v = g < 4 ? y : (g == 12 || g == 14 ? x : z);
    return ((g & 1) ? -u : u) + ((g & 2) ? -v : v);
  }
};

struct CaveNoiseSet
{
  Perlin3D shape;
  Perlin3D detail;
  Perlin3D veg;
  uint32_t seedShape = 0;
  uint32_t seedDetail = 0;
  uint32_t seedVeg = 0;
};

static thread_local CaveNoiseSet g_noiseCache;

static const CaveNoiseSet& getNoise(uint32_t seed)
{
  uint32_t sShape = seed * 0x9E3779B1u + 1u;
  uint32_t sDetail = seed * 0x85EBCA77u + 3u;
  uint32_t sVeg = seed * 0xC2B2AE3Du + 7u;

  if (g_noiseCache.seedShape != sShape)
  {
    g_noiseCache.shape.reseed(sShape);
    g_noiseCache.seedShape = sShape;
  }
  if (g_noiseCache.seedDetail != sDetail)
  {
    g_noiseCache.detail.reseed(sDetail);
    g_noiseCache.seedDetail = sDetail;
  }
  if (g_noiseCache.seedVeg != sVeg)
  {
    g_noiseCache.veg.reseed(sVeg);
    g_noiseCache.seedVeg = sVeg;
  }

  return g_noiseCache;
}

float caveDensity(int wx, int wy, int wz, int terrainHeight, uint32_t seed, const CaveConfig& cfg)
{
  const CaveNoiseSet& noise = getNoise(seed);

  float sx = static_cast<float>(wx);
  float sy = static_cast<float>(wy);
  float sz = static_cast<float>(wz);

  // Don't carve caves too close to surface or below minimum
  if (wy < cfg.minCaveHeight) return -1.0f;
  if (wy > terrainHeight - cfg.surfaceMargin) return -1.0f;

  // === CHEESE CAVES (large open caverns) ===
  // Use 3D noise - carve where noise value is below threshold
  float cheeseNoise = noise.shape.fbm(
    sx * cfg.cheeseFrequency,
    sy * cfg.cheeseFrequency * 0.8f,  // Slightly squashed vertically for wider caves
    sz * cfg.cheeseFrequency,
    cfg.cheeseOctaves,
    cfg.cheeseGain,
    1.0f
  );
  
  bool isCheeseCave = cheeseNoise < cfg.cheeseThreshold;
  
  // === SPAGHETTI TUNNELS (connecting passages) ===
  float yStretched = sy * cfg.spaghettiYStretch;
  
  float spaghetti1 = noise.detail.noise(
    sx * cfg.spaghettiFrequency,
    yStretched * cfg.spaghettiFrequency,
    sz * cfg.spaghettiFrequency
  );
  
  float spaghetti2 = noise.veg.noise(
    sx * cfg.spaghettiFrequency + 500.0f,
    yStretched * cfg.spaghettiFrequency,
    sz * cfg.spaghettiFrequency + 500.0f
  );
  
  // Tunnel where both are near zero
  bool isSpaghettiCave = (std::abs(spaghetti1) < cfg.spaghettiThreshold) && 
                         (std::abs(spaghetti2) < cfg.spaghettiThreshold);
  
  // Combine both cave types
  bool isCave = isCheeseCave || isSpaghettiCave;
  
  if (!isCave) return -1.0f;
  
  // Smooth transition near surface margin
  float distToSurface = static_cast<float>(terrainHeight - cfg.surfaceMargin - wy);
  if (distToSurface < 5.0f && distToSurface > 0.0f) {
    float fade = distToSurface / 5.0f;
    // Reduce cave probability near surface
    float fadeNoise = noise.detail.noise(sx * 0.1f, sy * 0.1f, sz * 0.1f);
    if (fadeNoise > fade * 2.0f - 1.0f) {
      return -1.0f;  // No cave
    }
  }

  return 1.0f; // Positive = carve
}

bool caveVegetationMask(int wx, int wy, int wz, uint32_t seed, const CaveConfig& cfg)
{
  const CaveNoiseSet& noise = getNoise(seed);
  float n = noise.veg.noise(wx * cfg.vegFrequency, wy * cfg.vegFrequency, wz * cfg.vegFrequency);
  return n > cfg.vegThreshold;
}

void applyCavesToBlocks(BlockID* blocks, const glm::ivec3& chunkPos, uint32_t worldSeed, 
                        const int* terrainHeights, const CaveConfig& cfg, bool* outVegetationMask)
{
  const int baseX = chunkPos.x * CHUNK_SIZE;
  const int baseY = chunkPos.y * CHUNK_SIZE;
  const int baseZ = chunkPos.z * CHUNK_SIZE;

  for (int z = 0; z < CHUNK_SIZE; ++z)
  {
    for (int x = 0; x < CHUNK_SIZE; ++x)
    {
      // Get terrain height for this column
      int terrainHeight = terrainHeights ? terrainHeights[z * CHUNK_SIZE + x] : 60;
      
      for (int y = 0; y < CHUNK_SIZE; ++y)
      {
        int wx = baseX + x;
        int wy = baseY + y;
        int wz = baseZ + z;

        float d = caveDensity(wx, wy, wz, terrainHeight, worldSeed, cfg);
        int i = blockIndex(x, y, z);

        if (d > 0.0f)
        {
          blocks[i] = 0; // carve to air
          if (outVegetationMask)
            outVegetationMask[i] = false;
        }
        else if (outVegetationMask)
        {
          bool isFloor = false;
          if (y + 1 < CHUNK_SIZE)
          {
            float above = caveDensity(wx, wy + 1, wz, terrainHeight, worldSeed, cfg);
            if (above > 0.0f)
              isFloor = true;
          }
          outVegetationMask[i] = isFloor && caveVegetationMask(wx, wy, wz, worldSeed, cfg);
        }
      }
    }
  }
}

void applyCavesToChunk(Chunk& c, uint32_t worldSeed, const int* terrainHeights, 
                       const CaveConfig& cfg, bool* outVegetationMask)
{
  applyCavesToBlocks(c.blocks, c.position, worldSeed, terrainHeights, cfg, outVegetationMask);
}
