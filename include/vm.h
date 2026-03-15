#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "debug.h"
#include "table.h"
#include "object.h"
#include "value.h"
#include "compiler.h"
#include <stdarg.h>
#include <ctime>
#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct
{
    ObjClosure *closure;
    uint8_t *ip;
    Value *slots;
} CallFrame;

typedef enum
{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

#define EXCEPTION_HANDLERS_MAX 16

struct ExceptionHandler
{
    uint8_t *catchIp;
    Value *stackTop;
    int frameCount;
};

static Value clockNative(int argCount, Value *args)
{
    (void)argCount;
    (void)args;
    return Value::Number((double)clock() / CLOCKS_PER_SEC);
}

class VM
{
public:
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    Chunk *chunk;
    ObjUpvalue *openUpvalues;
    uint8_t *ip;
    Value stack[STACK_MAX];
    Value *stackTop;
    Obj *objects;
    Table strings;
    Table globals;
    int grayCount;
    int grayCapacity;
    Obj **grayStack;
    size_t bytesAllocated;
    size_t nextGC;
    ObjString *initString;
    ExceptionHandler exceptionHandlers[EXCEPTION_HANDLERS_MAX];
    int exceptionHandlerCount;

    VM() : frameCount(0), chunk(nullptr), ip(nullptr), stackTop(stack),
           objects(nullptr), grayCount(0), grayCapacity(0), grayStack(nullptr),
           bytesAllocated(0), nextGC(1024 * 1024), initString(nullptr), openUpvalues(nullptr),
           exceptionHandlerCount(0)
    {
    }

    ~VM()
    {
    }

    void init()
    {
        resetStack();
        initString = new ObjString("init", 4);
        defineNative("clock", clockNative);
    }

    void resetStack()
    {
        stackTop = stack;
        frameCount = 0;
        exceptionHandlerCount = 0;
    }

    void runtimeError(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fputs("\n", stderr);
        for (int i = frameCount - 1; i >= 0; i--)
        {
            CallFrame *frame = &frames[i];
            ObjFunction *function = frame->closure->function;
            size_t instruction = frame->ip - function->chunk.code - 1;
            fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
            if (function->name == NULL)
            {
                fprintf(stderr, "script\n");
            }
            else
            {
                fprintf(stderr, "%s()\n", function->name->chars);
            }
        }
        resetStack();
    }

    InterpretResult interpret(const char *source)
    {
        Chunk chk;
        Compiler compiler;
        ObjFunction *function = compiler.compile(source, &chk);
        if (function == NULL)
            return INTERPRET_COMPILE_ERROR;

        ObjClosure *closure = new ObjClosure(function);
        push(Value::Object((Obj *)closure));
        CallFrame *frame = &frames[frameCount++];
        frame->closure = closure;
        frame->ip = function->chunk.code;
        frame->slots = stack;

        return run();
    }

private:
    void push(Value value)
    {
        *stackTop = value;
        stackTop++;
    }

    Value pop()
    {
        stackTop--;
        return *stackTop;
    }

    Value peek(int distance)
    {
        return stackTop[-1 - distance];
    }

    bool call(ObjClosure *closure, int argCount)
    {
        if (argCount != closure->function->arity)
        {
            runtimeError("Expected %d arguments but got %d.", closure->function->arity, argCount);
            return false;
        }
        if (frameCount == FRAMES_MAX)
        {
            runtimeError("Stack overflow.");
            return false;
        }
        CallFrame *frame = &frames[frameCount++];
        frame->closure = closure;
        frame->ip = closure->function->chunk.code;
        frame->slots = stackTop - argCount - 1;
        return true;
    }

    bool callValue(Value callee, int argCount)
    {
        if (callee.isObj())
        {
            switch (callee.asObj()->type)
            {
            case OBJ_CLOSURE:
                return call((ObjClosure *)callee.asObj(), argCount);
            case OBJ_CLASS:
            {
                ObjClass *klass = (ObjClass *)callee.asObj();
                ObjInstance *instance = new ObjInstance(klass);
                stackTop[-argCount - 1] = Value::Object((Obj *)instance);
                Value initializer;
                if (initializer = klass->methods.tableGet(initString); !initializer.isNil())
                {
                    return call((ObjClosure *)initializer.asObj(), argCount);
                }
                return true;
            }
            case OBJ_BOUND_METHOD:
            {
                ObjBoundMethod *bound = (ObjBoundMethod *)callee.asObj();
                stackTop[-argCount - 1] = bound->receiver;
                return call(bound->method, argCount);
            }
            case OBJ_NATIVE:
            {
                ObjNative *native = (ObjNative *)callee.asObj();
                Value result = native->function(argCount, stackTop - argCount);
                stackTop -= argCount + 1;
                push(result);
                return true;
            }
            default:
                break;
            }
        }
        runtimeError("Can only call functions and classes.");
        return false;
    }

    void defineNative(const char *name, NativeFn function)
    {
        ObjString *nameStr = new ObjString(name, (int)strlen(name));
        push(Value::Object((Obj *)nameStr));
        push(Value::Object((Obj *)new ObjNative(function)));
        globals.tableSet((ObjString *)peek(1).asObj(), peek(0));
        pop();
        pop();
    }

    bool isFalsey(Value value)
    {
        return (value.isNil()) || (value.isBool() && !value.asBool());
    }

    void concatenate()
    {
        ObjString *b = (ObjString *)pop().asObj();
        ObjString *a = (ObjString *)pop().asObj();
        int length = a->length + b->length;
        char *chars = new char[length + 1];
        std::memcpy(chars, a->chars, a->length);
        std::memcpy(chars + a->length, b->chars, b->length);
        chars[length] = '\0';
        ObjString *result = new ObjString(chars, length);
        delete[] chars;
        push(Value::Object((Obj *)result));
    }

    void closeUpvalues(Value *last)
    {
        ObjUpvalue *upvalue = openUpvalues;
        while (upvalue != nullptr && upvalue->location >= last)
        {
            upvalue->closed = *upvalue->location;
            upvalue->location = &upvalue->closed;
            upvalue = upvalue->next;
        }
    }

    ObjUpvalue *captureUpvalue(Value *local)
    {
        ObjUpvalue *upvalue = openUpvalues;
        while (upvalue != nullptr && upvalue->location > local)
        {
            upvalue = upvalue->next;
        }
        if (upvalue != nullptr && upvalue->location == local)
        {
            return upvalue;
        }
        ObjUpvalue *createdUpvalue = new ObjUpvalue(local);
        createdUpvalue->next = openUpvalues;
        openUpvalues = createdUpvalue;
        return createdUpvalue;
    }

    bool bindMethod(ObjClass *klass, ObjString *name)
    {
        Value method = klass->methods.tableGet(name);
        if (method.isNil())
        {
            return false;
        }
        ObjBoundMethod *bound = new ObjBoundMethod(peek(0), (ObjClosure *)method.asObj());
        pop();
        push(Value::Object((Obj *)bound));
        return true;
    }

    void defineMethod(ObjString *name)
    {
        Value method = peek(0);
        ObjClass *klass = (ObjClass *)peek(1).asObj();
        klass->methods.tableSet(name, method);
        pop();
    }

    bool invokeFromClass(ObjClass *klass, ObjString *name, int argCount)
    {
        Value method;
        if (method = klass->methods.tableGet(name); !method.isNil())
        {
            return call((ObjClosure *)method.asObj(), argCount);
        }
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }

    bool invoke(ObjString *name, int argCount)
    {
        Value receiver = peek(argCount);
        if (!receiver.isObj() || receiver.asObj()->type != OBJ_INSTANCE)
        {
            runtimeError("Only instances have methods.");
            return false;
        }
        ObjInstance *instance = (ObjInstance *)receiver.asObj();

        Value value;
        if (value = instance->fields.tableGet(name); !value.isNil())
        {
            stackTop[-argCount - 1] = value;
            return callValue(value, argCount);
        }
        return invokeFromClass(instance->klass, name, argCount);
    }

    InterpretResult run()
    {
        CallFrame *frame = &frames[frameCount - 1];
#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() ((ObjString *)READ_CONSTANT().asObj())
#define BINARY_OP(valueType, op)                                      \
    do                                                                \
    {                                                                 \
        if (!peek(0).isNumber() || !peek(1).isNumber())               \
        {                                                             \
            runtimeError("Runtime error: Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR;                           \
        }                                                             \
        double b = pop().asNumber();                                  \
        double a = pop().asNumber();                                  \
        push(valueType(a op b));                                      \
    } while (false)
        for (;;)
        {
#ifdef DEBUG_TRACE_EXECUTION
            printf("          ");
            for (Value *slot = stack; slot < stackTop; slot++)
            {
                printf("[ ");
                slot->print();
                printf(" ]");
            }
            printf("\n");
            disassembleInstruction(&frame->closure->function->chunk,
                                   (int)(frame->ip - frame->closure->function->chunk.code));
#endif
            uint8_t instruction;
            switch (instruction = READ_BYTE())
            {
            case OP_CONSTANT:
            {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_NEGATE:
            {
                if (!peek(0).isNumber())
                {
                    runtimeError("Runtime error: Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(Value::Number(-pop().asNumber()));
                break;
            }
            case OP_RETURN:
            {
                Value result = pop();
                closeUpvalues(frame->slots);
                frameCount--;
                if (frameCount == 0)
                {
                    pop();
                    return INTERPRET_OK;
                }
                stackTop = frame->slots;
                push(result);
                frame = &frames[frameCount - 1];
                break;
            }
            case OP_ADD:
            {
                if (peek(0).isObj() && peek(1).isObj())
                {
                    concatenate();
                }
                else if (peek(0).isNumber() && peek(1).isNumber())
                {
                    double b = pop().asNumber();
                    double a = pop().asNumber();
                    push(Value::Number(a + b));
                }
                else
                {
                    runtimeError("Runtime error: Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT:
                BINARY_OP(Value::Number, -);
                break;
            case OP_MULTIPLY:
                BINARY_OP(Value::Number, *);
                break;
            case OP_DIVIDE:
                BINARY_OP(Value::Number, /);
                break;
            case OP_NIL:
                push(Value::Nil());
                break;
            case OP_TRUE:
                push(Value::Bool(true));
                break;
            case OP_FALSE:
                push(Value::Bool(false));
                break;
            case OP_NOT:
                push(Value::Bool(isFalsey(pop())));
                break;
            case OP_EQUAL:
            {
                Value b = pop();
                Value a = pop();
                push(Value::Bool(a.equals(b)));
                break;
            }
            case OP_PRINT:
            {
                pop().print();
                printf("\n");
                break;
            }
            case OP_GREATER:
                BINARY_OP(Value::Bool, >);
                break;
            case OP_LESS:
                BINARY_OP(Value::Bool, <);
                break;
            case OP_POP:
                pop();
                break;
            case OP_DEFINE_GLOBAL:
            {
                ObjString *name = READ_STRING();
                globals.tableSet(name, peek(0));
                pop();
                break;
            }
            case OP_GET_GLOBAL:
            {
                ObjString *name = READ_STRING();
                std::string key(name->chars, name->length);
                auto it = globals.entries.find(key);
                if (it == globals.entries.end())
                {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(it->second);
                break;
            }
            case OP_SET_GLOBAL:
            {
                ObjString *name = READ_STRING();
                std::string key(name->chars, name->length);
                if (globals.entries.find(key) == globals.entries.end())
                {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                globals.tableSet(name, peek(0));
                break;
            }
            case OP_SET_LOCAL:
            {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }
            case OP_GET_LOCAL:
            {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OP_JUMP_IF_FALSE:
            {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0)))
                    frame->ip += offset;
                break;
            }
            case OP_JUMP:
            {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_LOOP:
            {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL:
            {
                int argCount = READ_BYTE();
                if (!callValue(peek(argCount), argCount))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &frames[frameCount - 1];
                break;
            }
            case OP_CLOSURE:
            {
                ObjFunction *function = (ObjFunction *)READ_CONSTANT().asObj();
                ObjClosure *closure = new ObjClosure(function);
                push(Value::Object((Obj *)closure));
                for (int i = 0; i < closure->upvalueCount; i++)
                {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal)
                    {
                        closure->upvalues[i] = captureUpvalue(frame->slots + index);
                    }
                    else
                    {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_GET_UPVALUE:
            {
                uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE:
            {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
                break;
            }
            case OP_CLOSE_UPVALUE:
            {
                closeUpvalues(stackTop - 1);
                pop();
                break;
            }
            case OP_CLASS:
            {
                ObjString *name = READ_STRING();
                push(Value::Object((Obj *)new ObjClass(name)));
                break;
            }
            case OP_GET_PROPERTY:
            {
                if (!peek(0).isObj() || peek(0).asObj()->type != OBJ_INSTANCE)
                {
                    runtimeError("Only instances have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjInstance *instance = (ObjInstance *)peek(0).asObj();
                ObjString *name = READ_STRING();
                std::string key(name->chars, name->length);
                auto it = instance->fields.entries.find(key);
                if (it != instance->fields.entries.end())
                {
                    pop();
                    push(it->second);
                    break;
                }
                if (!bindMethod(instance->klass, name))
                {
                    runtimeError("Undefined property '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_PROPERTY:
            {
                if (!peek(1).isObj() || peek(1).asObj()->type != OBJ_INSTANCE)
                {
                    runtimeError("Only instances have fields.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjInstance *instance = (ObjInstance *)peek(1).asObj();
                ObjString *name = READ_STRING();
                std::string key(name->chars, name->length);
                instance->fields.tableSet(name, peek(0));
                Value value = peek(0);
                pop();
                push(value);
                break;
            }
            case OP_METHOD:
            {
                ObjString *name = READ_STRING();
                defineMethod(name);
                break;
            }
            case OP_INVOKE:
            {
                ObjString *method = READ_STRING();
                int argCount = READ_BYTE();
                if (!invoke(method, argCount))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &frames[frameCount - 1];
                break;
            }
            case OP_INHERIT:
            {
                Value superclassVal = peek(1);
                if (!superclassVal.isObj() || superclassVal.asObj()->type != OBJ_CLASS)
                {
                    runtimeError("Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjClass *superclass = (ObjClass *)superclassVal.asObj();
                ObjClass *subclass = (ObjClass *)peek(0).asObj();
                for (const auto &entry : superclass->methods.entries)
                {
                    subclass->methods.tableSet(new ObjString(entry.first.c_str(), (int)entry.first.length()), entry.second);
                }
                pop();
                break;
            }
            case OP_GET_SUPER:
            {
                ObjString *name = READ_STRING();
                Value superclassVal = pop();
                if (!superclassVal.isObj() || superclassVal.asObj()->type != OBJ_CLASS)
                {
                    runtimeError("Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjClass *superclass = (ObjClass *)superclassVal.asObj();
                if (!bindMethod(superclass, name))
                {
                    runtimeError("Undefined property '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUPER_INVOKE:
            {
                ObjString *method = READ_STRING();
                int argCount = READ_BYTE();
                Value superclassVal = pop();
                if (!superclassVal.isObj() || superclassVal.asObj()->type != OBJ_CLASS)
                {
                    runtimeError("Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjClass *superclass = (ObjClass *)superclassVal.asObj();
                if (!invokeFromClass(superclass, method, argCount))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &frames[frameCount - 1];
                break;
            }
            case OP_ARRAY:
            {
                int size = static_cast<int>(pop().asNumber());
                ObjArray *array = new ObjArray(size);
                push(Value::Object((Obj *)array));
                break;
            }
            case OP_ARRAY_GET_GLOBAL:
            {
                ObjString *name = READ_STRING();
                std::string key(name->chars, name->length);
                auto it = globals.entries.find(key);
                if (it == globals.entries.end())
                {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value arrayVal = it->second;
                if (!arrayVal.isObj() || arrayVal.asObj()->type != OBJ_ARRAY)
                {
                    runtimeError("Variable '%s' is not an array.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjArray *array = (ObjArray *)arrayVal.asObj();
                Value indexVal = pop();
                if (!indexVal.isNumber())
                {
                    runtimeError("Array index must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                int index = static_cast<int>(indexVal.asNumber());
                if (index < 0 || index >= array->count)
                {
                    runtimeError("Array index out of bounds.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(array->values[index]);
                break;
            }
            case OP_ARRAY_SET_GLOBAL:
            {
                ObjString *name = READ_STRING();
                std::string key(name->chars, name->length);
                auto it = globals.entries.find(key);
                if (it == globals.entries.end())
                {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value arrayVal = it->second;
                if (!arrayVal.isObj() || arrayVal.asObj()->type != OBJ_ARRAY)
                {
                    runtimeError("Variable '%s' is not an array.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjArray *array = (ObjArray *)arrayVal.asObj();
                Value val = pop();
                Value indexVal = pop();
                if (!indexVal.isNumber())
                {
                    runtimeError("Array index must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                int index = static_cast<int>(indexVal.asNumber());
                if (index < 0 || index >= array->count)
                {
                    runtimeError("Array index out of bounds.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                array->values[index] = val;
                push(val);
                break;
            }
            case OP_ARRAY_GET_LOCAL:
            {
                uint8_t slot = READ_BYTE();
                Value arrayVal = frame->slots[slot];
                if (!arrayVal.isObj() || arrayVal.asObj()->type != OBJ_ARRAY)
                {
                    runtimeError("Variable is not an array.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjArray *array = (ObjArray *)arrayVal.asObj();
                Value indexVal = pop();
                if (!indexVal.isNumber())
                {
                    runtimeError("Array index must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                int index = static_cast<int>(indexVal.asNumber());
                if (index < 0 || index >= array->count)
                {
                    runtimeError("Array index out of bounds.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(array->values[index]);
                break;
            }
            case OP_ARRAY_SET_LOCAL:
            {
                uint8_t slot = READ_BYTE();
                Value arrayVal = frame->slots[slot];
                if (!arrayVal.isObj() || arrayVal.asObj()->type != OBJ_ARRAY)
                {
                    runtimeError("Variable is not an array.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjArray *array = (ObjArray *)arrayVal.asObj();
                Value val = pop();
                Value indexVal = pop();
                if (!indexVal.isNumber())
                {
                    runtimeError("Array index must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                int index = static_cast<int>(indexVal.asNumber());
                if (index < 0 || index >= array->count)
                {
                    runtimeError("Array index out of bounds.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                array->values[index] = val;
                push(val);
                break;
            }
            case OP_ARRAY_GET_UPVALUE:
            {
                uint8_t slot = READ_BYTE();
                Value arrayVal = *frame->closure->upvalues[slot]->location;
                if (!arrayVal.isObj() || arrayVal.asObj()->type != OBJ_ARRAY)
                {
                    runtimeError("Variable is not an array.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjArray *array = (ObjArray *)arrayVal.asObj();
                Value indexVal = pop();
                if (!indexVal.isNumber())
                {
                    runtimeError("Array index must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                int index = static_cast<int>(indexVal.asNumber());
                if (index < 0 || index >= array->count)
                {
                    runtimeError("Array index out of bounds.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(array->values[index]);
                break;
            }
            case OP_ARRAY_SET_UPVALUE:
            {
                uint8_t slot = READ_BYTE();
                Value arrayVal = *frame->closure->upvalues[slot]->location;
                if (!arrayVal.isObj() || arrayVal.asObj()->type != OBJ_ARRAY)
                {
                    runtimeError("Variable is not an array.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjArray *array = (ObjArray *)arrayVal.asObj();
                Value val = pop();
                Value indexVal = pop();
                if (!indexVal.isNumber())
                {
                    runtimeError("Array index must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                int index = static_cast<int>(indexVal.asNumber());
                if (index < 0 || index >= array->count)
                {
                    runtimeError("Array index out of bounds.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                array->values[index] = val;
                push(val);
                break;
            }
            case OP_INDEX_GET:
            {
                Value indexVal = pop();
                Value arrayVal = pop();
                if (!arrayVal.isObj() || arrayVal.asObj()->type != OBJ_ARRAY)
                {
                    runtimeError("Can only subscript arrays.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjArray *array = (ObjArray *)arrayVal.asObj();
                if (!indexVal.isNumber())
                {
                    runtimeError("Array index must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                int index = static_cast<int>(indexVal.asNumber());
                if (index < 0 || index >= array->count)
                {
                    runtimeError("Array index out of bounds.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(array->values[index]);
                break;
            }
            case OP_TRY:
            {
                uint16_t offset = READ_SHORT();
                if (exceptionHandlerCount >= EXCEPTION_HANDLERS_MAX)
                {
                    runtimeError("Too many nested exception handlers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ExceptionHandler *handler = &exceptionHandlers[exceptionHandlerCount++];
                handler->catchIp = frame->ip + offset;
                handler->stackTop = stackTop;
                handler->frameCount = frameCount;
                break;
            }
            case OP_TRY_END:
            {
                exceptionHandlerCount--;
                break;
            }
            case OP_THROW:
            {
                Value thrown = pop();
                if (exceptionHandlerCount == 0)
                {
                    push(thrown);
                    runtimeError("Unhandled exception.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ExceptionHandler *handler = &exceptionHandlers[--exceptionHandlerCount];
                while (frameCount > handler->frameCount)
                {
                    closeUpvalues(frames[frameCount - 1].slots);
                    frameCount--;
                }
                frame = &frames[frameCount - 1];
                closeUpvalues(handler->stackTop);
                stackTop = handler->stackTop;
                push(thrown);
                frame->ip = handler->catchIp;
                break;
            }
            }
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
        }
    }
};

static VM vm;

static void initVM()
{
    vm.init();
}

static void freeVM()
{
}

static InterpretResult interpret(const char *source)
{
    return vm.interpret(source);
}

#endif
