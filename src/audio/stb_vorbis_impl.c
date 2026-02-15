/* Compile stb_vorbis as a C translation unit so it doesn't conflict with
   C++ Windows SDK headers when included from AudioEngine.cpp. */
#include "../thirdparty/stb_vorbis.c"
