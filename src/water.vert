#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aLocalUV;
layout (location = 2) in float aTileIndex;
layout (location = 3) in float aSkyLight;
layout (location = 4) in float aFaceShade;

out vec2 LocalUV;
flat out float TileIndex;
out float SkyLight;
out float FaceShade;
out vec3 WorldPos;

uniform mat4 transform;
uniform mat4 model;

void main()
{
   vec3 pos = aPos;
   // Lower only the top surface a little so water sits beneath land without sinking the whole prism
   if (pos.y > 0.5)
      pos.y -= 0.05;
   
   vec4 worldPosition = model * vec4(pos, 1.0);
   WorldPos = worldPosition.xyz;
   
   gl_Position = transform * vec4(pos, 1.0);
   LocalUV = aLocalUV;
   TileIndex = aTileIndex;
   SkyLight = aSkyLight;
   FaceShade = aFaceShade;
}
