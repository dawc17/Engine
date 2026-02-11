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

    float tintMask = step(0.998, texColor.a);
    vec3 tint = mix(vec3(1.0), ParticleColor.rgb, tintMask);

    float fade = smoothstep(0.0, 0.3, Lifetime);
    FragColor = vec4(texColor.rgb * tint * totalLight, fade);
}
