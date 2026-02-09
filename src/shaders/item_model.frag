#version 460 core
out vec4 FragColor;

in vec2 UV;
flat in float TileIndex;
in float FaceShade;

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
    FragColor = vec4(texColor.rgb * finalLight, texColor.a);
}
