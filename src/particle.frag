#version 460 core
in vec2 TexCoord;
flat in float TileIndex;
in vec4 ParticleColor;
in float Lifetime;

out vec4 FragColor;

uniform sampler2DArray textureArray;

void main()
{
    vec4 texColor = texture(textureArray, vec3(TexCoord, TileIndex));
    if (texColor.a < 0.1)
        discard;

    float fade = smoothstep(0.0, 0.3, Lifetime);
    FragColor = texColor * ParticleColor;
    FragColor.a *= fade;
}
