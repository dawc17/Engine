#version 460 core
out vec4 FragColor;

in vec2 LocalUV;
flat in float TileIndex;

uniform sampler2DArray textureArray;

void main()
{
    // Tile within greedy quads using fract - each block gets its own copy
    vec2 tiledUV = fract(LocalUV);
    
    // Clamp to avoid sampling from opposite edge due to GL_REPEAT
    // This creates a tiny inset (half pixel for a 16x16 texture)
    const float HALF_PIXEL = 0.5 / 16.0;
    tiledUV = clamp(tiledUV, HALF_PIXEL, 1.0 - HALF_PIXEL);
    
    // Sample from the texture array - layer is the tile index
    FragColor = texture(textureArray, vec3(tiledUV, TileIndex));
}
