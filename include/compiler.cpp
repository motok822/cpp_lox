#include "compiler.h"
#include "object.h"
#include "common.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Parse rules table
// Order must match TokenType enum in scanner.h
Compiler::ParseRule Compiler::rules[] = {
    {&Compiler::grouping, &Compiler::call, PREC_CALL}, // TOKEN_LEFT_PAREN
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_RIGHT_PAREN
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_LEFT_BRACE
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_RIGHT_BRACE
    {nullptr, &Compiler::subscript, PREC_CALL},        // TOKEN_LEFT_BRACKET
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_RIGHT_BRACKET
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_COMMA
    {nullptr, &Compiler::dot, PREC_CALL},              // TOKEN_DOT
    {&Compiler::unary, &Compiler::binary, PREC_TERM},  // TOKEN_MINUS
    {nullptr, &Compiler::binary, PREC_TERM},           // TOKEN_PLUS
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_SEMICOLON
    {nullptr, &Compiler::binary, PREC_FACTOR},         // TOKEN_SLASH
    {nullptr, &Compiler::binary, PREC_FACTOR},         // TOKEN_STAR
    {&Compiler::unary, nullptr, PREC_NONE},            // TOKEN_BANG
    {nullptr, &Compiler::binary, PREC_EQUALITY},       // TOKEN_BANG_EQUAL
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_EQUAL
    {nullptr, &Compiler::binary, PREC_EQUALITY},       // TOKEN_EQUAL_EQUAL
    {nullptr, &Compiler::binary, PREC_COMPARISON},     // TOKEN_GREATER
    {nullptr, &Compiler::binary, PREC_COMPARISON},     // TOKEN_GREATER_EQUAL
    {nullptr, &Compiler::binary, PREC_COMPARISON},     // TOKEN_LESS
    {nullptr, &Compiler::binary, PREC_COMPARISON},     // TOKEN_LESS_EQUAL
    {&Compiler::variable, nullptr, PREC_NONE},         // TOKEN_IDENTIFIER
    {&Compiler::string, nullptr, PREC_NONE},           // TOKEN_STRING
    {&Compiler::number, nullptr, PREC_NONE},           // TOKEN_NUMBER
    {nullptr, &Compiler::and_, PREC_AND},              // TOKEN_AND
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_CLASS
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_ELSE
    {&Compiler::literal, nullptr, PREC_NONE},          // TOKEN_FALSE
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_FOR
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_FUN
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_IF
    {&Compiler::literal, nullptr, PREC_NONE},          // TOKEN_NIL
    {nullptr, &Compiler::or_, PREC_OR},                // TOKEN_OR
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_PRINT
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_RETURN
    {&Compiler::super_, nullptr, PREC_NONE},           // TOKEN_SUPER
    {&Compiler::this_, nullptr, PREC_NONE},            // TOKEN_THIS
    {&Compiler::literal, nullptr, PREC_NONE},          // TOKEN_TRUE
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_VAR
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_WHILE
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_CONTINUE
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_TRY
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_CATCH
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_THROW
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_ERROR
    {nullptr, nullptr, PREC_NONE},                     // TOKEN_EOF
};

Compiler::ParseRule *Compiler::getRule(TokenType type)
{
    return &rules[type];
}

Chunk *Compiler::currentChunk()
{
    return &current->function->chunk;
}

ObjString *Compiler::copyString(const char *chars, int length)
{
    return new ObjString(chars, length);
}

// ---- Error handling ----

void Compiler::errorAt(Token *token, const char *message)
{
    if (parser.panicMode)
        return;
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);
    if (token->type == TOKEN_EOF)
    {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR)
    {
        // Nothing.
    }
    else
    {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }
    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

void Compiler::errorAtCurrent(const char *message)
{
    errorAt(&parser.current, message);
}

void Compiler::error(const char *message)
{
    errorAt(&parser.previous, message);
}

// ---- Emit bytecode ----

void Compiler::emitByte(uint8_t byte)
{
    currentChunk()->writeChunk(byte, parser.previous.line);
}

void Compiler::emitBytes(uint8_t byte1, uint8_t byte2)
{
    emitByte(byte1);
    emitByte(byte2);
}

void Compiler::emitReturn()
{
    if (current->type == TYPE_INITIALIZER)
    {
        emitBytes(OP_GET_LOCAL, 0);
    }
    else
    {
        emitByte(OP_NIL);
    }
    emitByte(OP_RETURN);
}

int Compiler::emitJump(uint8_t instruction)
{
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

void Compiler::patchJump(int offset)
{
    int jump = currentChunk()->count - offset - 2;
    if (jump > UINT16_MAX)
    {
        error("Too much code to jump over.");
    }
    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

void Compiler::emitLoop(int loopStart)
{
    emitByte(OP_LOOP);
    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX)
    {
        error("Loop body too large.");
    }
    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

uint8_t Compiler::makeConstant(Value value)
{
    int constant = currentChunk()->addConstant(value);
    if (constant > UINT8_MAX)
    {
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t)constant;
}

void Compiler::emitConstant(Value value)
{
    emitBytes(OP_CONSTANT, makeConstant(value));
}

// ---- Compiler state init / end ----

void Compiler::initCompiler(CompilerState *compiler, FunctionType type)
{
    compiler->enclosing = current;
    compiler->function = new ObjFunction();
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->innermostLoopStart = -1;
    compiler->innermostLoopScopeDepth = -1;
    current = compiler;
    if (type != TYPE_SCRIPT)
    {
        current->function->name = copyString(parser.previous.start,
                                             parser.previous.length);
    }
    Local *local = &current->locals[current->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    if (type != TYPE_FUNCTION)
    {
        local->name.start = "this";
        local->name.length = 4;
    }
    else
    {
        local->name.start = "";
        local->name.length = 0;
    }
}

ObjFunction *Compiler::endCompiler()
{
    emitReturn();
    ObjFunction *function = current->function;
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError)
    {
        disassembleChunk(&current->function->chunk,
                         function->name != NULL ? function->name->chars : "<script>");
    }
#endif
    current = current->enclosing;
    return function;
}

// ---- Scanner interface ----

void Compiler::advance()
{
    parser.previous = parser.current;
    for (;;)
    {
        parser.current = compilerScanner.scanToken();
        if (parser.current.type != TOKEN_ERROR)
            break;
        errorAtCurrent(parser.current.start);
    }
}

bool Compiler::check(TokenType type)
{
    return parser.current.type == type;
}

bool Compiler::match(TokenType type)
{
    if (!check(type))
        return false;
    advance();
    return true;
}

void Compiler::consume(TokenType type, const char *message)
{
    if (parser.current.type == type)
    {
        advance();
        return;
    }
    errorAtCurrent(message);
}

// ---- Scope management ----

void Compiler::beginScope()
{
    current->scopeDepth++;
}

void Compiler::endScope()
{
    current->scopeDepth--;
    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].depth > current->scopeDepth)
    {
        if (current->locals[current->localCount - 1].isCaptured)
        {
            emitByte(OP_CLOSE_UPVALUE);
        }
        else
        {
            emitByte(OP_POP);
        }
        current->localCount--;
    }
}

// ---- Identifiers and variables ----

bool Compiler::identifiersEqual(Token *a, Token *b)
{
    if (a->length != b->length)
        return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

int Compiler::resolveLocal(CompilerState *compiler, Token *name)
{
    for (int i = compiler->localCount - 1; i >= 0; i--)
    {
        Local *local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name))
        {
            if (local->depth == -1)
            {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

int Compiler::addUpvalue(CompilerState *compiler, uint8_t index, bool isLocal)
{
    int upvalueCount = compiler->function->upvalueCount;
    for (int i = 0; i < upvalueCount; i++)
    {
        if (compiler->upvalues[i].index == index &&
            compiler->upvalues[i].isLocal == isLocal)
        {
            return i;
        }
    }
    if (upvalueCount == UINT8_COUNT)
    {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

int Compiler::resolveUpvalue(CompilerState *compiler, Token *name)
{
    if (compiler->enclosing == nullptr)
        return -1;
    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1)
    {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1)
    {
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

void Compiler::addLocal(Token *name)
{
    if (current->localCount == UINT8_COUNT)
    {
        error("Too many local variables in function.");
        return;
    }
    Local *local = &current->locals[current->localCount++];
    local->name = *name;
    local->depth = -1;
    local->isCaptured = false;
}

void Compiler::declareVariable()
{
    if (current->scopeDepth == 0)
        return;
    Token *name = &parser.previous;
    for (int i = current->localCount - 1; i >= 0; i--)
    {
        Local *local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth)
            break;
        if (identifiersEqual(name, &local->name))
        {
            error("Already a variable with this name in this scope.");
        }
    }
    addLocal(name);
}

uint8_t Compiler::identifierConstant(Token *name)
{
    return makeConstant(Value::Object((Obj *)copyString(name->start, name->length)));
}

uint8_t Compiler::parseVariable(const char *errorMessage)
{
    consume(TOKEN_IDENTIFIER, errorMessage);
    declareVariable();
    if (current->scopeDepth > 0)
        return 0;
    return identifierConstant(&parser.previous);
}

void Compiler::markInitialized()
{
    if (current->scopeDepth == 0)
        return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

void Compiler::defineVariable(uint8_t global)
{
    if (current->scopeDepth > 0)
    {
        markInitialized();
        return;
    }
    emitBytes(OP_DEFINE_GLOBAL, global);
}

// ---- Expression parsing ----

void Compiler::number(bool canAssign)
{
    (void)canAssign;
    double value = strtod(parser.previous.start, nullptr);
    emitConstant(Value::Number(value));
}

void Compiler::string(bool canAssign)
{
    (void)canAssign;
    emitConstant(Value::Object(
        (Obj *)copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

void Compiler::grouping(bool canAssign)
{
    expression(canAssign);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

void Compiler::literal(bool canAssign)
{
    (void)canAssign;
    switch (parser.previous.type)
    {
    case TOKEN_FALSE:
        emitByte(OP_FALSE);
        break;
    case TOKEN_NIL:
        emitByte(OP_NIL);
        break;
    case TOKEN_TRUE:
        emitByte(OP_TRUE);
        break;
    default:
        return;
    }
}

void Compiler::unary(bool canAssign)
{
    TokenType operatorType = parser.previous.type;
    parsePrecedence(PREC_UNARY);
    switch (operatorType)
    {
    case TOKEN_MINUS:
        emitByte(OP_NEGATE);
        break;
    case TOKEN_BANG:
        emitByte(OP_NOT);
        break;
    default:
        return;
    }
}

void Compiler::namedVariable(Token name, bool canAssign)
{
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1)
    {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    }
    else if ((arg = resolveUpvalue(current, &name)) != -1)
    {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    }
    else
    {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (match(TOKEN_LEFT_BRACKET))
    {
        expression(false);
        consume(TOKEN_RIGHT_BRACKET, "Expect ']' after array index.");
        switch (getOp)
        {
        case OP_GET_GLOBAL:
            getOp = OP_ARRAY_GET_GLOBAL;
            setOp = OP_ARRAY_SET_GLOBAL;
            break;
        case OP_GET_LOCAL:
            getOp = OP_ARRAY_GET_LOCAL;
            setOp = OP_ARRAY_SET_LOCAL;
            break;
        case OP_GET_UPVALUE:
            getOp = OP_ARRAY_GET_UPVALUE;
            setOp = OP_ARRAY_SET_UPVALUE;
            break;
        default:
            error("Invalid array access.");
            return;
        }
    }

    if (canAssign && match(TOKEN_EQUAL))
    {
        expression(canAssign);
        emitBytes(setOp, (uint8_t)arg);
    }
    else
    {
        emitBytes(getOp, (uint8_t)arg);
    }
}

void Compiler::variable(bool canAssign)
{
    namedVariable(parser.previous, canAssign);
}

void Compiler::this_(bool canAssign)
{
    (void)canAssign;
    if (currentClass == nullptr)
    {
        error("Can't use 'this' outside of a class.");
        return;
    }
    variable(false);
}

void Compiler::binary(bool canAssign)
{
    (void)canAssign;
    TokenType operatorType = parser.previous.type;
    ParseRule *rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));
    switch (operatorType)
    {
    case TOKEN_PLUS:
        emitByte(OP_ADD);
        break;
    case TOKEN_MINUS:
        emitByte(OP_SUBTRACT);
        break;
    case TOKEN_STAR:
        emitByte(OP_MULTIPLY);
        break;
    case TOKEN_SLASH:
        emitByte(OP_DIVIDE);
        break;
    case TOKEN_EQUAL_EQUAL:
        emitByte(OP_EQUAL);
        break;
    case TOKEN_BANG_EQUAL:
        emitBytes(OP_EQUAL, OP_NOT);
        break;
    case TOKEN_GREATER:
        emitByte(OP_GREATER);
        break;
    case TOKEN_GREATER_EQUAL:
        emitBytes(OP_LESS, OP_NOT);
        break;
    case TOKEN_LESS:
        emitByte(OP_LESS);
        break;
    case TOKEN_LESS_EQUAL:
        emitBytes(OP_GREATER, OP_NOT);
        break;
    default:
        return;
    }
}

void Compiler::and_(bool canAssign)
{
    (void)canAssign;
    int endJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    parsePrecedence(PREC_AND);
    patchJump(endJump);
}

void Compiler::or_(bool canAssign)
{
    (void)canAssign;
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);
    patchJump(elseJump);
    emitByte(OP_POP);
    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

uint8_t Compiler::argumentList()
{
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN))
    {
        do
        {
            expression(false);
            if (argCount == 255)
            {
                error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after function parameters.");
    return argCount;
}

void Compiler::call(bool canAssign)
{
    (void)canAssign;
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

void Compiler::subscript(bool canAssign)
{
    (void)canAssign;
    expression(false);
    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after index.");
    emitByte(OP_INDEX_GET);
}

void Compiler::dot(bool canAssign)
{
    consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint8_t name = identifierConstant(&parser.previous);
    if (canAssign && match(TOKEN_EQUAL))
    {
        expression(canAssign);
        emitBytes(OP_SET_PROPERTY, name);
    }
    else if (match(TOKEN_LEFT_PAREN))
    {
        uint8_t argCount = argumentList();
        emitBytes(OP_INVOKE, name);
        emitByte(argCount);
    }
    else
    {
        emitBytes(OP_GET_PROPERTY, name);
    }
}

Token Compiler::syntheticToken(const char *text)
{
    Token token;
    token.start = text;
    token.length = (int)strlen(text);
    return token;
}

void Compiler::super_(bool canAssign)
{
    (void)canAssign;
    if (currentClass == nullptr)
    {
        error("Can't use 'super' outside of a class.");
    }
    else if (!currentClass->hasSuperclass)
    {
        error("Can't use 'super' in a class with no superclass.");
    }

    consume(TOKEN_DOT, "Expect '.' after 'super'.");
    consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
    uint8_t name = identifierConstant(&parser.previous);
    namedVariable(syntheticToken("this"), false);
    if (match(TOKEN_LEFT_PAREN))
    {
        uint8_t argCount = argumentList();
        namedVariable(syntheticToken("super"), false);
        emitBytes(OP_SUPER_INVOKE, name);
        emitByte(argCount);
    }
    else
    {
        namedVariable(syntheticToken("super"), false);
        emitBytes(OP_GET_SUPER, name);
    }
}

void Compiler::parsePrecedence(Precedence precedence)
{
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == nullptr)
    {
        error("Expect expression.");
        return;
    }
    bool canAssign = precedence <= PREC_ASSIGNMENT;
    (this->*prefixRule)(canAssign);
    while (precedence <= getRule(parser.current.type)->precedence)
    {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        (this->*infixRule)(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL))
    {
        error("Invalid assignment target.");
    }
}

void Compiler::expression(bool canAssign)
{
    (void)canAssign;
    parsePrecedence(PREC_ASSIGNMENT);
}

// ---- Statements ----

void Compiler::block()
{
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
    {
        declaration(false);
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

void Compiler::printStatement()
{
    expression(false);
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

void Compiler::expressionStatement()
{
    expression(false);
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

void Compiler::synchronize()
{
    parser.panicMode = false;
    while (parser.current.type != TOKEN_EOF)
    {
        if (parser.previous.type == TOKEN_SEMICOLON)
            return;
        switch (parser.current.type)
        {
        case TOKEN_CLASS:
        case TOKEN_FUN:
        case TOKEN_VAR:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_PRINT:
        case TOKEN_RETURN:
            return;
        default:
            break;
        }
        advance();
    }
}

void Compiler::varDeclaration()
{
    uint8_t global = parseVariable("Expect variable name.");
    bool isArray = false;

    if (match(TOKEN_LEFT_BRACKET))
    {
        expression(false);
        consume(TOKEN_RIGHT_BRACKET, "Expect ']' after array size.");
        emitByte(OP_ARRAY);
        isArray = true;
    }

    if (match(TOKEN_EQUAL))
    {
        expression(false);
    }
    else if (!isArray)
    {
        emitByte(OP_NIL);
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    defineVariable(global);
}

void Compiler::function(FunctionType type)
{
    CompilerState compiler;
    initCompiler(&compiler, type);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN))
    {
        do
        {
            current->function->arity++;
            if (current->function->arity > 255)
            {
                error("Can't have more than 255 parameters.");
            }
            uint8_t constant = parseVariable("Expect parameter name.");
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction *fn = endCompiler();
    emitBytes(OP_CLOSURE, makeConstant(Value::Object((Obj *)fn)));
    for (int i = 0; i < fn->upvalueCount; i++)
    {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

void Compiler::funDeclaration()
{
    uint8_t global = parseVariable("Expect function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

void Compiler::method()
{
    consume(TOKEN_IDENTIFIER, "Expect method name.");
    uint8_t constant = identifierConstant(&parser.previous);
    FunctionType type = TYPE_METHOD;
    if (parser.previous.length == 4 &&
        memcmp(parser.previous.start, "init", 4) == 0)
    {
        type = TYPE_INITIALIZER;
    }
    function(type);
    emitBytes(OP_METHOD, constant);
}

void Compiler::classDeclaration()
{
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser.previous;
    uint8_t nameConstant = identifierConstant(&parser.previous);
    declareVariable();
    emitBytes(OP_CLASS, nameConstant);
    defineVariable(nameConstant);

    ClassCompiler classCompiler;
    classCompiler.enclosing = currentClass;
    classCompiler.hasSuperclass = false;
    currentClass = &classCompiler;

    if (match(TOKEN_LESS))
    {
        consume(TOKEN_IDENTIFIER, "Expect superclass name.");
        variable(false);
        if (identifiersEqual(&className, &parser.previous))
        {
            error("A class can't inherit from itself.");
        }

        beginScope();
        Token superToken = syntheticToken("super");
        addLocal(&superToken);
        defineVariable(0);

        namedVariable(className, false);
        emitByte(OP_INHERIT);
        classCompiler.hasSuperclass = true;
    }

    namedVariable(className, false);

    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
    {
        method();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emitByte(OP_POP);
    if (classCompiler.hasSuperclass)
    {
        endScope();
    }
    currentClass = currentClass->enclosing;
}

void Compiler::ifStatement()
{
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression(false);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement(false);
    int elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);
    emitByte(OP_POP);
    if (match(TOKEN_ELSE))
    {
        statement(false);
    }
    patchJump(elseJump);
}

void Compiler::whileStatement()
{
    int loopStart = currentChunk()->count;

    int enclosingLoopStart = current->innermostLoopStart;
    int enclosingLoopScopeDepth = current->innermostLoopScopeDepth;

    current->innermostLoopStart = loopStart;
    current->innermostLoopScopeDepth = current->scopeDepth;

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression(false);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement(false);
    emitLoop(loopStart);
    patchJump(exitJump);
    emitByte(OP_POP);

    current->innermostLoopStart = enclosingLoopStart;
    current->innermostLoopScopeDepth = enclosingLoopScopeDepth;
}

void Compiler::forStatement()
{
    beginScope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON))
    {
    }
    else if (match(TOKEN_VAR))
    {
        varDeclaration();
    }
    else
    {
        expressionStatement();
    }
    int loopStart = currentChunk()->count;

    int enclosingLoopStart = current->innermostLoopStart;
    int enclosingLoopScopeDepth = current->innermostLoopScopeDepth;

    current->innermostLoopStart = loopStart;
    current->innermostLoopScopeDepth = current->scopeDepth;

    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON))
    {
        expression(false);
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP);
    }
    if (!match(TOKEN_RIGHT_PAREN))
    {
        int bodyJump = emitJump(OP_JUMP);
        int incrementStart = currentChunk()->count;
        expression(false);
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(loopStart);
        loopStart = incrementStart;

        current->innermostLoopStart = incrementStart;

        patchJump(bodyJump);
    }

    statement(false);
    emitLoop(loopStart);
    if (exitJump != -1)
    {
        patchJump(exitJump);
        emitByte(OP_POP);
    }

    current->innermostLoopStart = enclosingLoopStart;
    current->innermostLoopScopeDepth = enclosingLoopScopeDepth;

    endScope();
}

void Compiler::continueStatement()
{
    if (current->innermostLoopStart == -1)
    {
        error("Can't use 'continue' outside of a loop.");
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'.");
    emitLoop(current->innermostLoopStart);
}

void Compiler::returnStatement()
{
    if (current->type == TYPE_SCRIPT)
    {
        error("Can't return from top-level code.");
    }
    if (match(TOKEN_SEMICOLON))
    {
        emitReturn();
    }
    else
    {
        if (current->type == TYPE_INITIALIZER)
        {
            error("Can't return a value from an initializer.");
        }
        expression(false);
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

void Compiler::tryStatement()
{
    consume(TOKEN_LEFT_BRACE, "Expect '{' after 'try'.");
    int tryJump = emitJump(OP_TRY);

    beginScope();
    block();
    endScope();

    emitByte(OP_TRY_END);
    int skipCatchJump = emitJump(OP_JUMP);

    patchJump(tryJump);

    consume(TOKEN_CATCH, "Expect 'catch' after try block.");
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'catch'.");
    consume(TOKEN_IDENTIFIER, "Expect variable name in catch.");
    Token catchVar = parser.previous;
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after catch variable.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before catch body.");

    beginScope();
    addLocal(&catchVar);
    markInitialized();

    block();
    endScope();

    patchJump(skipCatchJump);
}

void Compiler::throwStatement()
{
    expression(false);
    consume(TOKEN_SEMICOLON, "Expect ';' after throw expression.");
    emitByte(OP_THROW);
}

void Compiler::statement(bool canAssign)
{
    (void)canAssign;
    if (match(TOKEN_PRINT))
    {
        printStatement();
    }
    else if (match(TOKEN_RETURN))
    {
        returnStatement();
    }
    else if (match(TOKEN_WHILE))
    {
        whileStatement();
    }
    else if (match(TOKEN_FOR))
    {
        forStatement();
    }
    else if (match(TOKEN_IF))
    {
        ifStatement();
    }
    else if (match(TOKEN_CONTINUE))
    {
        continueStatement();
    }
    else if (match(TOKEN_TRY))
    {
        tryStatement();
    }
    else if (match(TOKEN_THROW))
    {
        throwStatement();
    }
    else if (match(TOKEN_LEFT_BRACE))
    {
        beginScope();
        block();
        endScope();
    }
    else
    {
        expressionStatement();
    }
}

void Compiler::declaration(bool canAssign)
{
    (void)canAssign;
    if (match(TOKEN_VAR))
    {
        varDeclaration();
    }
    else if (match(TOKEN_CLASS))
    {
        classDeclaration();
    }
    else if (match(TOKEN_FUN))
    {
        funDeclaration();
    }
    else
    {
        statement(false);
    }

    if (parser.panicMode)
    {
        synchronize();
    }
}

// ---- Public API ----

ObjFunction *Compiler::compile(const char *source, Chunk *chunk)
{
    compilerScanner = Scanner(source);
    CompilerState compiler;
    initCompiler(&compiler, TYPE_SCRIPT);
    compilingChunk = chunk;
    parser.hadError = false;
    parser.panicMode = false;

    advance();
    while (!match(TOKEN_EOF))
    {
        declaration(false);
    }
    ObjFunction *function = endCompiler();
    return parser.hadError ? nullptr : function;
}

void Compiler::markCompilerRoots()
{
    // TODO: implement when GC is added
}
