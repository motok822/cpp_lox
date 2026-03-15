#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "table.h"
#include <cstring>
#include <map>

typedef enum
{
    OBJ_STRING,
    OBJ_NATIVE,
    OBJ_FUNCTION,
    OBJ_CLOSURE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_UPVALUE,
    OBJ_BOUND_METHOD,
    OBJ_ARRAY,
} ObjType;

struct Obj
{
    ObjType type;
    bool isMarked;
    Obj *next;
};

struct ObjString : public Obj
{
    int length;
    char *chars;

    ObjString(const char *chars, int length)
    {
        type = OBJ_STRING;
        isMarked = false;
        next = nullptr;
        this->length = length;
        this->chars = new char[length + 1];
        std::memcpy(this->chars, chars, length);
        this->chars[length] = '\0';
    }

    ~ObjString() { delete[] chars; }
};

struct ObjFunction : public Obj
{
    int arity;
    int upvalueCount;
    Chunk chunk;
    ObjString *name;

    ObjFunction()
    {
        type = OBJ_FUNCTION;
        isMarked = false;
        next = nullptr;
        arity = 0;
        upvalueCount = 0;
        name = nullptr;
    }
};

struct ObjUpvalue : public Obj
{
    Value *location;
    Value closed; // スタック*locationが閉じられたときの値を保存
    ObjUpvalue *next;

    ObjUpvalue(Value *location)
    {
        type = OBJ_UPVALUE;
        isMarked = false;
        next = nullptr;
        this->location = location;
        this->closed = Value::Nil();
    }
};

struct ObjClosure : public Obj
{
    ObjUpvalue **upvalues;
    ObjFunction *function;
    int upvalueCount;

    ObjClosure(ObjFunction *function)
    {
        type = OBJ_CLOSURE;
        isMarked = false;
        next = nullptr;
        this->function = function;
        this->upvalueCount = function->upvalueCount;
        upvalues = new ObjUpvalue *[upvalueCount];
        for (int i = 0; i < upvalueCount; i++)
            upvalues[i] = nullptr;
    }
};

struct ObjArray : public Obj
{
    int count;
    int capacity;
    Value *values;

    ObjArray(int capacity = 8)
    {
        type = OBJ_ARRAY;
        isMarked = false;
        next = nullptr;
        this->count = capacity;
        this->capacity = capacity;
        values = new Value[capacity];
        for (int i = 0; i < capacity; i++)
            values[i] = Value::Nil();
    }
};

typedef Value (*NativeFn)(int argCount, Value *args);
struct ObjNative : public Obj
{
    NativeFn function;

    ObjNative(NativeFn function)
    {
        type = OBJ_NATIVE;
        isMarked = false;
        next = nullptr;
        this->function = function;
    }
};

struct ObjClass : public Obj
{
    ObjString *name;
    Table methods;

    ObjClass(ObjString *name) : name(name)
    {
        type = OBJ_CLASS;
        isMarked = false;
        next = nullptr;
    }
};

struct ObjInstance : public Obj
{
    ObjClass *klass;
    Table fields;

    ObjInstance(ObjClass *klass) : klass(klass)
    {
        type = OBJ_INSTANCE;
        isMarked = false;
        next = nullptr;
    }
};

struct ObjBoundMethod : public Obj
{
    Value receiver;
    ObjClosure *method;

    ObjBoundMethod(Value receiver, ObjClosure *method) : receiver(receiver), method(method)
    {
        type = OBJ_BOUND_METHOD;
        isMarked = false;
        next = nullptr;
    }
};

// Table inline methods (defined here because ObjString must be complete)
inline void Table::tableSet(ObjString *key, Value value)
{
    entries[std::string(key->chars, key->length)] = value;
}

inline void Table::tableDelete(ObjString *key)
{
    entries.erase(std::string(key->chars, key->length));
}

inline Value Table::tableGet(ObjString *key)
{
    auto it = entries.find(std::string(key->chars, key->length));
    if (it != entries.end())
    {
        return it->second;
    }
    return Value::Nil();
}

#endif
