#ifndef clox_chunk_h
#define clox_chunk_h
#include "common.h"
#include "value.h"
#include <cstdlib>

typedef enum
{
    OP_RETURN,
    OP_NEGATE,
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_NOT,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_PRINT,
    OP_POP,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_SET_LOCAL,
    OP_GET_LOCAL,
    OP_SET_UPVALUE,
    OP_GET_UPVALUE,
    OP_JUMP_IF_FALSE,
    OP_JUMP,
    OP_LOOP,
    OP_CALL,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_CLASS,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_METHOD,
    OP_INVOKE,
    OP_INHERIT,
    OP_GET_SUPER,
    OP_SUPER_INVOKE,
    OP_ARRAY,
    OP_ARRAY_GET_GLOBAL,
    OP_ARRAY_SET_GLOBAL,
    OP_ARRAY_GET_LOCAL,
    OP_ARRAY_SET_LOCAL,
    OP_ARRAY_GET_UPVALUE,
    OP_ARRAY_SET_UPVALUE,
    OP_INDEX_GET,
    OP_TRY,
    OP_TRY_END,
    OP_THROW,
} OpCode;

class Chunk
{
public:
    int count;
    int capacity;
    uint8_t *code;
    int *lines;
    ValueArray constants;

    Chunk() : count(0), capacity(0), code(nullptr), lines(nullptr)
    {
        constants = ValueArray();
    }

    void writeChunk(uint8_t byte, int line)
    {
        if (capacity < count + 1)
        {
            int oldCapacity = capacity;
            capacity = oldCapacity < 8 ? 8 : oldCapacity * 2;
            code = (uint8_t *)realloc(code, sizeof(uint8_t) * capacity);
            lines = (int *)realloc(lines, sizeof(int) * capacity);
        }
        code[count] = byte;
        lines[count] = line;
        count++;
    }
    int addConstant(Value value)
    {
        constants.write(value);
        return constants.count() - 1;
    }
    void freeChunk()
    {
        free(code);
        free(lines);
        constants.free();
        code = nullptr;
        lines = nullptr;
        count = 0;
        capacity = 0;
    }
};

#endif