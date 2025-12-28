#ifndef ENGINE_CORE_H
#define ENGINE_CORE_H

void engineCore_init();
void engineCore_configure(int width, int height);
void engineCore_render();
void engineCore_addModBytes(const unsigned char* data, int length, const char* modName);

#endif // ENGINE_CORE_H
