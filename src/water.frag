#version 460 core
out vec4 FragColor;

in vec2 LocalUV;
flat in float TileIndex;
in float SkyLight;
in float FaceShade;
in float FragDepth;
in vec3 WorldPos;

uniform float timeOfDay;
uniform vec3 cameraPos;
uniform vec3 skyColor;
uniform vec3 fogColor;
uniform float fogDensity;
uniform float ambientLight;
uniform float time;
uniform bool enableCaustics;

void main()
{
    vec2 uv = WorldPos.xz * 0.1;
    
    float wave1 = sin(uv.x * 3.0 + time * 1.5) * 0.5 + 0.5;
    float wave2 = sin(uv.y * 2.5 + time * 1.2) * 0.5 + 0.5;
    float wave3 = sin((uv.x + uv.y) * 2.0 + time * 0.8) * 0.5 + 0.5;
    float waves = (wave1 + wave2 + wave3) / 3.0;
    
    vec3 deepWater = vec3(0.05, 0.2, 0.4);
    vec3 shallowWater = vec3(0.1, 0.4, 0.6);
    vec3 waterColor = mix(deepWater, shallowWater, waves * 0.3 + 0.3);
    
    float highlight = pow(waves, 3.0) * 0.15;
    waterColor += vec3(highlight);
    
    float sunBrightness = timeOfDay;
    float skyLightContribution = SkyLight * sunBrightness;
    float totalLight = max(skyLightContribution, ambientLight);
    float finalLight = totalLight * FaceShade;
    
    vec3 litColor = waterColor * finalLight;
    
    if (enableCaustics)
    {
        float c1 = sin(uv.x * 8.0 + time * 1.2) * sin(uv.y * 8.0 + time * 0.8);
        float c2 = sin(uv.x * 6.0 - time) * sin(uv.y * 7.0 + time * 1.1);
        float caustic = (c1 + c2) * 0.1 + 0.5;
        litColor += vec3(caustic * 0.08 * finalLight);
    }
    
    vec3 viewDir = normalize(cameraPos - WorldPos);
    float fresnel = pow(1.0 - max(0.0, dot(viewDir, vec3(0.0, 1.0, 0.0))), 3.0);
    litColor = mix(litColor, skyColor * finalLight * 0.8, fresnel * 0.4);
    
    float dist = length(WorldPos - cameraPos);
    float fogFactor = 1.0 - exp(-dist * fogDensity);
    fogFactor = clamp(fogFactor, 0.0, 0.9);
    
    vec3 finalColor = mix(litColor, fogColor, fogFactor);
    
    float alpha = 0.7;
    alpha = mix(alpha, 0.85, fresnel);
    
    FragColor = vec4(finalColor, alpha);
}
