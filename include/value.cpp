#include "object.h"

void Value::print() const
{
    switch (type)
    {
    case ValueType::Bool:
        printf("%s", as.boolean ? "true" : "false");
        break;
    case ValueType::Nil:
        printf("nil");
        break;
    case ValueType::Number:
        printf("%g", as.number);
        break;
    case ValueType::Obj:
    {
        Obj *obj = as.obj;
        switch (obj->type)
        {
        case OBJ_STRING:
            printf("%s", ((ObjString *)obj)->chars);
            break;
        case OBJ_FUNCTION:
        {
            ObjFunction *fn = (ObjFunction *)obj;
            if (fn->name != nullptr)
                printf("<fn %s>", fn->name->chars);
            else
                printf("<script>");
            break;
        }
        case OBJ_UPVALUE:
            printf("upvalue");
            break;
        case OBJ_CLOSURE:
        {
            ObjClosure *closure = (ObjClosure *)obj;
            ObjFunction *fn = closure->function;
            if (fn->name != nullptr)
                printf("<fn %s>", fn->name->chars);
            else
                printf("<script>");
            break;
        }
        case OBJ_ARRAY:
        {
            ObjArray *array = (ObjArray *)obj;
            printf("[");
            for (int i = 0; i < array->count; i++)
            {
                array->values[i].print();
                if (i < array->count - 1)
                    printf(", ");
            }
            printf("]");
            break;
        }
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
        default:
            printf("[obj]");
            break;
        }
        break;
    }
    }
}
