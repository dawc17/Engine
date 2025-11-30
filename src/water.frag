#version 460 core
out vec4 FragColor;

in vec2 LocalUV;
flat in float TileIndex;
in float SkyLight;
in float FaceShade;
in vec3 WorldPos;

uniform float timeOfDay;
uniform vec3 cameraPos;
uniform vec3 skyColor;
uniform vec3 fogColor;
uniform float fogDensity;
uniform float ambientLight;
uniform float time;

void main()
{
    float sunBrightness = timeOfDay;
    float totalLight = max(SkyLight * sunBrightness, ambientLight);

    vec2 flowUV = LocalUV;
    bool isFlowing = abs(flowUV.x - 0.5) > 0.01 || abs(flowUV.y - 0.5) > 0.01;
    
    vec2 animatedUV;
    if (isFlowing)
    {
        vec2 flowDir = normalize(flowUV - vec2(0.5));
        animatedUV = WorldPos.xz * 0.1 + flowDir * time * 0.5;
    }
    else
    {
        animatedUV = WorldPos.xz * 0.1 + vec2(time * 0.02, time * 0.015);
    }
    
    float wave1 = sin(animatedUV.x * 3.0 + animatedUV.y * 2.0) * 0.5 + 0.5;
    float wave2 = sin(animatedUV.x * 2.0 - animatedUV.y * 3.0 + time * 0.3) * 0.5 + 0.5;
    float wave3 = sin((animatedUV.x + animatedUV.y) * 4.0 + time * 0.2) * 0.5 + 0.5;
    
    float combinedWave = (wave1 + wave2 + wave3) / 3.0;
    
    float ripple = combinedWave * 0.15 - 0.075;
    
    float shade = clamp(0.85 + ripple, 0.75, 1.0) * FaceShade;
    
    vec3 deepColor = vec3(0.02, 0.15, 0.45);
    vec3 shallowColor = vec3(0.15, 0.45, 0.75);
    vec3 baseColor = mix(deepColor, shallowColor, combinedWave * 0.3 + 0.5);
    
    vec3 litColor = baseColor * shade * totalLight;
    
    float dist = length(WorldPos - cameraPos);
    float fogFactor = 1.0 - exp(-dist * fogDensity);
    fogFactor = clamp(fogFactor, 0.0, 0.85);

    vec3 finalColor = mix(litColor, fogColor, fogFactor);

    float alpha = 0.75 + combinedWave * 0.1;
    
    FragColor = vec4(finalColor, alpha);
}
