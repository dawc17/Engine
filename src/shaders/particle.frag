#version 460 core
in vec2 TexCoord;
flat in float TileIndex;
in vec4 ParticleColor;
in float Lifetime;
in float SkyLight;

out vec4 FragColor;

uniform sampler2DArray textureArray;
uniform float timeOfDay;
uniform float ambientLight;

void main()
{
    vec4 texColor = texture(textureArray, vec3(TexCoord, TileIndex));
    if (texColor.a < 0.1)
        discard;

    float sunBrightness = timeOfDay;
    float skyLightContribution = SkyLight * sunBrightness;
    float totalLight = max(skyLightContribution, ambientLight);

    float fade = smoothstep(0.0, 0.3, Lifetime);
    vec4 litColor = vec4(texColor.rgb * totalLight, texColor.a);
    FragColor = litColor * ParticleColor;
    FragColor.a *= fade;
}
