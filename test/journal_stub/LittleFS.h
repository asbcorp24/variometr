#pragma once
#include "FS.h"
struct LittleFSClass {
  bool begin(bool){return memoryStore.mounted;}
  bool format(){++memoryStore.formats;memoryStore.nodes.clear();memoryStore.mounted=true;return true;}
  size_t totalBytes()const{return memoryStore.capacity;}
  size_t usedBytes()const{return memoryStore.used();}
  bool exists(const char *path)const{return memoryStore.nodes.count(path);}
  bool remove(const char *path){return memoryStore.failRemovePath!=path && memoryStore.nodes.erase(path)>0;}
  File open(const char *path,const char *mode="r"){
    if(std::string(path)=="/")return File::root();
    if(std::string(mode)=="w"){
      auto node=std::make_shared<MemoryNode>();node->path=path;memoryStore.nodes[path]=node;return File(node);
    }
    const auto i=memoryStore.nodes.find(path);return i==memoryStore.nodes.end()?File():File(i->second);
  }
};
extern LittleFSClass LittleFS;
