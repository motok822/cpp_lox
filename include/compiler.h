#ifndef clox_compiler_h
#define clox_compiler_h

#include "common.h"
#include "chunk.h"
#include "object.h"
#include "scanner.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

struct Parser
{
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
};

struct Upvalue
{
    uint8_t index;
    bool isLocal;
};

enum Precedence
{
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_PRIMARY
};

struct Local
{
    Token name;
    int depth;
    bool isCaptured;
};

enum FunctionType
{
    TYPE_FUNCTION,
    TYPE_METHOD,
    TYPE_SCRIPT,
    TYPE_INITIALIZER
};

struct ClassCompiler
{
    ClassCompiler *enclosing;
    bool hasSuperclass;
};

struct CompilerState
{
    CompilerState *enclosing;
    ObjFunction *function;
    FunctionType type;
    Local locals[UINT8_COUNT];
    int localCount;
    Upvalue upvalues[UINT8_COUNT];
    int scopeDepth;
    int innermostLoopStart;
    int innermostLoopScopeDepth;
};

class Compiler
{
public:
    ObjFunction *compile(const char *source, Chunk *chunk);
    void markCompilerRoots();

private:
    typedef void (Compiler::*ParseFn)(bool canAssign);

    struct ParseRule
    {
        ParseFn prefix;
        ParseFn infix;
        Precedence precedence;
    };

    Parser parser;
    CompilerState *current = nullptr;
    ClassCompiler *currentClass = nullptr;
    Chunk *compilingChunk = nullptr;
    Scanner compilerScanner;

    static ParseRule rules[];

    ParseRule *getRule(TokenType type);
    Chunk *currentChunk();
    ObjString *copyString(const char *chars, int length);

    // Error handling
    void errorAt(Token *token, const char *message);
    void errorAtCurrent(const char *message);
    void error(const char *message);

    // Emit bytecode
    void emitByte(uint8_t byte);
    void emitBytes(uint8_t byte1, uint8_t byte2);
    void emitReturn();
    int emitJump(uint8_t instruction);
    void patchJump(int offset);
    void emitLoop(int loopStart);
    uint8_t makeConstant(Value value);
    void emitConstant(Value value);

    // Compiler state init / end
    void initCompiler(CompilerState *compiler, FunctionType type);
    ObjFunction *endCompiler();

    // Scanner interface
    void advance();
    bool check(TokenType type);
    bool match(TokenType type);
    void consume(TokenType type, const char *message);

    // Scope management
    void beginScope();
    void endScope();

    // Identifiers and variables
    bool identifiersEqual(Token *a, Token *b);
    int resolveLocal(CompilerState *compiler, Token *name);
    int addUpvalue(CompilerState *compiler, uint8_t index, bool isLocal);
    int resolveUpvalue(CompilerState *compiler, Token *name);
    void addLocal(Token *name);
    void declareVariable();
    uint8_t identifierConstant(Token *name);
    uint8_t parseVariable(const char *errorMessage);
    void markInitialized();
    void defineVariable(uint8_t global);

    // Expression parsing
    void number(bool canAssign);
    void string(bool canAssign);
    void grouping(bool canAssign);
    void literal(bool canAssign);
    void unary(bool canAssign);
    void namedVariable(Token name, bool canAssign);
    void variable(bool canAssign);
    void this_(bool canAssign);
    void binary(bool canAssign);
    void and_(bool canAssign);
    void or_(bool canAssign);
    uint8_t argumentList();
    void call(bool canAssign);
    void subscript(bool canAssign);
    void dot(bool canAssign);
    Token syntheticToken(const char *text);
    void super_(bool canAssign);
    void parsePrecedence(Precedence precedence);
    void expression(bool canAssign);

    // Statements
    void block();
    void printStatement();
    void expressionStatement();
    void synchronize();
    void varDeclaration();
    void function(FunctionType type);
    void funDeclaration();
    void method();
    void classDeclaration();
    void ifStatement();
    void whileStatement();
    void forStatement();
    void continueStatement();
    void returnStatement();
    void tryStatement();
    void throwStatement();
    void statement(bool canAssign);
    void declaration(bool canAssign);
};

#endif
