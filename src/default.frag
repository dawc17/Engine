#version 460 core
out vec4 FragColor;

in vec2 LocalUV;
flat in float TileIndex;

uniform sampler2DArray textureArray;

void main()
{
    // Sample from the texture array - layer is the tile index
    // GL_REPEAT handles the tiling, so we don't need fract() or manual clamping
    vec4 texColor = texture(textureArray, vec3(LocalUV, TileIndex));
    
    // Alpha test - discard nearly transparent pixels (for leaves, etc.)
    if (texColor.a < 0.5)
        discard;
    
    FragColor = texColor;
}
