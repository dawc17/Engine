#version 460 core
out vec4 FragColor;

in vec2 UV;
flat in float TileIndex;
in float FaceShade;
in vec3 BiomeTint;

uniform sampler2DArray textureArray;
uniform float timeOfDay;
uniform float ambientLight;

void main()
{
    vec4 texColor = texture(textureArray, vec3(UV, TileIndex));
    if (texColor.a < 0.5)
        discard;
    float totalLight = max(timeOfDay, ambientLight);
    float finalLight = totalLight * FaceShade;
    float tintMask = step(0.998, texColor.a);
    vec3 tint = mix(vec3(1.0), BiomeTint, tintMask);
    FragColor = vec4(texColor.rgb * tint * finalLight, 1.0);
}
