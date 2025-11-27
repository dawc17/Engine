#version 460 core
out vec4 FragColor;

in vec2 LocalUV;
flat in float TileIndex;
in float SkyLight;
in float FaceShade;
in float FragDepth;
in vec3 WorldPos;

uniform sampler2DArray textureArray;
uniform float timeOfDay;
uniform vec3 cameraPos;
uniform vec3 skyColor;
uniform vec3 fogColor;
uniform float fogDensity;
uniform float ambientLight;

void main()
{
    vec4 texColor = texture(textureArray, vec3(LocalUV, TileIndex));
    
    if (texColor.a < 0.5)
        discard;
    
    float sunBrightness = timeOfDay;
    float skyLightContribution = SkyLight * sunBrightness;
    float totalLight = max(skyLightContribution, ambientLight);
    float finalLight = totalLight * FaceShade;
    
    vec3 litColor = texColor.rgb * finalLight;
    
    float dist = length(WorldPos - cameraPos);
    float fogFactor = 1.0 - exp(-dist * fogDensity);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    
    float shadowFogBoost = 1.0 - SkyLight;
    fogFactor = fogFactor + shadowFogBoost * 0.15;
    fogFactor = clamp(fogFactor, 0.0, 0.95);
    
    vec3 finalColor = mix(litColor, fogColor, fogFactor);
    
    FragColor = vec4(finalColor, texColor.a);
}
