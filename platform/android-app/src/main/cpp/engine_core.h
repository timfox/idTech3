#ifndef ENGINE_CORE_H
#define ENGINE_CORE_H

#include <vector>
#include <string>
void engineCore_init();
void engineCore_configure(int width, int height);
void engineCore_render();
void engineCore_addModBytes(const unsigned char* data, int length, const char* modName);
void engineCore_loadTexture(const char* path);
void engineCore_loadTextureFromBytes(const unsigned char* data, int length, const char* texName);
void engineCore_loadMesh(const char* path);
void engineCore_loadMeshFromBytes(const unsigned char* data, int length, const char* meshName);
void engineCore_loadResource(const char* path);
void engineCore_loadResourceFromBytes(const unsigned char* data, int length, const char* resName);
void engineCore_queueTextureBytes(const unsigned char* data, int length, const char* texName);
void engineCore_queueMeshBytes(const unsigned char* data, int length, const char* meshName);
void engineCore_queueResourceBytes(const unsigned char* data, int length, const char* resName);
void engineCore_runLoaderCycle();
extern std::vector<std::string> gTextureDescriptors;
extern std::vector<std::string> gMeshDescriptors;
extern std::vector<std::string> gResourceDescriptors;
void engineCore_test_ingestTextureBytes(const unsigned char* data, int length, const char* texName);
void engineCore_test_ingestMeshBytes(const unsigned char* data, int length, const char* meshName);
void engineCore_test_ingestResourceBytes(const unsigned char* data, int length, const char* resName);
void engineCore_loadTexture(const char* path);
void engineCore_loadTextureFromBytes(const unsigned char* data, int length, const char* texName);
void engineCore_loadMesh(const char* path);
void engineCore_loadMeshFromBytes(const unsigned char* data, int length, const char* meshName);
void engineCore_loadResource(const char* path);
void engineCore_loadResourceFromBytes(const unsigned char* data, int length, const char* resName);

#endif // ENGINE_CORE_H
