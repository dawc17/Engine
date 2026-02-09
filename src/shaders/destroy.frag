#version 460 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D crackTex;
uniform float timeOfDay;
uniform float ambientLight;
uniform float SkyLight;
uniform float FaceShade;

void main()
{
  vec4 col = texture(crackTex, vUV);
  if (col.a <= 0.05)
    discard;

  float sunBrightness = timeOfDay;
  float skyLightContribution = SkyLight * sunBrightness;
  float totalLight = max(skyLightContribution, ambientLight);
  float finalLight = totalLight * FaceShade;

  FragColor = vec4(col.rgb * finalLight, col.a);
}
