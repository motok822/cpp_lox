#ifndef clox_table_h
#define clox_table_h
#include "common.h"
#include "value.h"
#include <map>
#include <string>

struct ObjString;

struct Entry
{
    ObjString *key;
    Value value;
};

class Table
{
public:
    std::map<std::string, Value> entries;
    Table() = default;
    inline void tableSet(ObjString *key, Value value);
    inline void tableDelete(ObjString *key);
    inline Value tableGet(ObjString *key);
};

#endif
