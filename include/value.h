#ifndef clox_value_h
#define clox_value_h

#include "common.h"
#include <cstring>
#include <cstdio>
#include <vector>

struct Obj;
struct ObjString;
struct ObjFunction;
struct ObjNative;

enum class ValueType
{
    Bool,
    Nil,
    Number,
    Obj,
};

class Value
{
public:
    ValueType type;
    union
    {
        bool boolean;
        double number;
        Obj *obj;
    } as;

    Value() : type(ValueType::Nil) { as.number = 0; }

    static Value Bool(bool value)
    {
        Value v;
        v.type = ValueType::Bool;
        v.as.boolean = value;
        return v;
    }

    static Value Nil()
    {
        Value v;
        v.type = ValueType::Nil;
        v.as.number = 0;
        return v;
    }

    static Value Number(double value)
    {
        Value v;
        v.type = ValueType::Number;
        v.as.number = value;
        return v;
    }

    static Value Object(Obj *value)
    {
        Value v;
        v.type = ValueType::Obj;
        v.as.obj = value;
        return v;
    }

    bool isBool() const { return type == ValueType::Bool; }
    bool isNil() const { return type == ValueType::Nil; }
    bool isNumber() const { return type == ValueType::Number; }
    bool isObj() const { return type == ValueType::Obj; }

    bool asBool() const { return as.boolean; }
    double asNumber() const { return as.number; }
    Obj *asObj() const { return as.obj; }

    bool equals(const Value &other) const
    {
        if (type != other.type)
            return false;
        switch (type)
        {
        case ValueType::Bool:
            return as.boolean == other.as.boolean;
        case ValueType::Nil:
            return true;
        case ValueType::Number:
            return as.number == other.as.number;
        case ValueType::Obj:
            return as.obj == other.as.obj;
        default:
            return false;
        }
    }

    void print() const;
};

class ValueArray : public Value
{
public:
    std::vector<Value> values;

    ValueArray() = default;

    void write(Value value) { values.push_back(value); }

    int count() const { return static_cast<int>(values.size()); }

    void free() { values.clear(); }
};

#endif
