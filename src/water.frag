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

    // Gentle color ripple based on world space to keep seams aligned across chunks
    vec2 flow = WorldPos.xz * 0.08 + vec2(time * 0.12, time * 0.09);
    float ripple =
        sin(flow.x) * 0.08 +
        sin(flow.y) * 0.08 +
        sin((flow.x + flow.y) * 0.5) * 0.05;

    float shade = clamp(0.9 + ripple * 0.08, 0.78, 1.05);
    vec3 baseColor = vec3(0.08, 0.33, 0.75); // flat blue water
    vec3 litColor = baseColor * shade * totalLight;
    
    float dist = length(WorldPos - cameraPos);
    float fogFactor = 1.0 - exp(-dist * fogDensity);
    fogFactor = clamp(fogFactor, 0.0, 0.85);

    vec3 finalColor = mix(litColor, fogColor, fogFactor);

    FragColor = vec4(finalColor, 0.7);
}
