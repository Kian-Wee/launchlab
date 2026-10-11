#pragma once
#include <string>
#include <map>
#include <vector>
#include <cstring>
#include <set>
class Preferences {
public:
  static inline std::map<std::string,std::vector<unsigned char>> data;
  static inline bool failWrite=false,shortRead=false;
  static inline void (*beforeWrite)()=nullptr;
  // Existing keys can return zero blob length on type/query failure; the real
  // ESP32 Preferences getBytesLength() folds all nvs_get_blob errors to zero.
  static inline std::set<std::string> zeroLengthKeys;
  std::string space;
  bool begin(const char *s,bool){space=s;return true;}
  bool isKey(const char *key){return data.count(space+key)!=0;}
  size_t getBytesLength(const char *key){auto i=data.find(space+key);return i==data.end() || zeroLengthKeys.count(space+key)?0:i->second.size();}
  size_t getBytes(const char *key,void *out,size_t size) {
    auto i=data.find(space+key);if(i==data.end())return 0;
    const size_t n=std::min(size,i->second.size());std::memcpy(out,i->second.data(),n);return shortRead?n-1:n;
  }
  bool remove(const char *key){return data.erase(space+key)!=0;}
  size_t putBytes(const char *key,const void *bytes,size_t n) {
    if(beforeWrite)beforeWrite();
    if(failWrite)return 0;
    const auto *p=static_cast<const unsigned char *>(bytes);data[space+key]={p,p+n};return n;
  }
};
