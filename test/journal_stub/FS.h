#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#define FILE_WRITE "w"
struct MemoryNode {
  std::string path;
  std::vector<uint8_t> data,durable;
};
struct MemoryStore {
  std::map<std::string,std::shared_ptr<MemoryNode>> nodes;
  size_t capacity=512*1024;
  bool mounted=true,failWrites=false;
  std::string failRemovePath;
  unsigned formats=0;
  size_t used()const{size_t n=0;for(const auto &kv:nodes)n+=kv.second->data.size();return n;}
  void crash(){for(auto &kv:nodes)kv.second->data=kv.second->durable;}
};
extern MemoryStore memoryStore;
class File {
 public:
  File()=default;
  explicit File(std::shared_ptr<MemoryNode> n):node_(n){}
  static File root(){File f;f.directory_=true;for(const auto &kv:memoryStore.nodes)f.children_.push_back(kv.second);return f;}
  explicit operator bool()const{return directory_||bool(node_);}
  File openNextFile(){return child_<children_.size()?File(children_[child_++]):File();}
  const char *name()const{return node_?node_->path.c_str():"/";}
  int available()const{return node_&&position_<node_->data.size();}
  int read(){return available()?node_->data[position_++]:-1;}
  size_t write(const uint8_t *bytes,size_t n){
    if(!node_||memoryStore.failWrites||memoryStore.used()+n>memoryStore.capacity)return 0;
    node_->data.insert(node_->data.end(),bytes,bytes+n);return n;
  }
  void flush(){if(node_)node_->durable=node_->data;}
  void close(){node_.reset();directory_=false;}
 private:
  std::shared_ptr<MemoryNode> node_;
  std::vector<std::shared_ptr<MemoryNode>> children_;
  size_t position_=0,child_=0;
  bool directory_=false;
};
