#ifndef clox_scanner_h
#define clox_scanner_h

#include <cstring>

typedef enum
{
    // Single-character tokens.
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_MINUS,
    TOKEN_PLUS,
    TOKEN_SEMICOLON,
    TOKEN_SLASH,
    TOKEN_STAR,
    // one or two character tokens.
    TOKEN_BANG,
    TOKEN_BANG_EQUAL,
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    // literals.
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,
    // keywords.
    TOKEN_AND,
    TOKEN_CLASS,
    TOKEN_ELSE,
    TOKEN_FALSE,
    TOKEN_FOR,
    TOKEN_FUN,
    TOKEN_IF,
    TOKEN_NIL,
    TOKEN_OR,
    TOKEN_PRINT,
    TOKEN_RETURN,
    TOKEN_SUPER,
    TOKEN_THIS,
    TOKEN_TRUE,
    TOKEN_VAR,
    TOKEN_WHILE,
    TOKEN_CONTINUE,
    // end of file.
    TOKEN_ERROR,
    TOKEN_EOF

} TokenType;

class Token
{
public:
    TokenType type;
    const char *start;
    int length;
    int line;

    Token() : type(TOKEN_EOF), start(nullptr), length(0), line(0) {}

    Token(TokenType type, const char *start, int length, int line)
        : type(type), start(start), length(length), line(line) {}
};

class Scanner
{
private:
    const char *start;
    const char *current;
    int line;

    bool isAtEnd() { return *current == '\0'; }

    Token makeToken(TokenType type)
    {
        return Token(type, start, (int)(current - start), line);
    }

    Token errorToken(const char *message)
    {
        return Token(TOKEN_ERROR, message, (int)strlen(message), line);
    }

    char advance()
    {
        current++;
        return current[-1];
    }

    bool match(char expected)
    {
        if (isAtEnd())
            return false;
        if (*current != expected)
            return false;
        current++;
        return true;
    }

    char peek() { return *current; }

    char peekNext()
    {
        if (isAtEnd())
            return '\0';
        return current[1];
    }

    void skipWhitespace()
    {
        for (;;)
        {
            char c = peek();
            switch (c)
            {
            case ' ':
            case '\r':
            case '\t':
            case '\n':
                if (c == '\n')
                    line++;
                advance();
                break;
            case '/':
                if (peekNext() == '/')
                {
                    while (peek() != '\n' && !isAtEnd())
                        advance();
                }
                else
                {
                    return;
                }
                break;
            default:
                return;
            }
        }
    }

    bool isDigit(char c) { return c >= '0' && c <= '9'; }

    bool isAlpha(char c)
    {
        return (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
               c == '_';
    }

    Token stringToken()
    {
        while (peek() != '"' && !isAtEnd())
        {
            if (peek() == '\n')
                line++;
            advance();
        }
        if (isAtEnd())
            return errorToken("Unterminated string.");
        advance();
        return makeToken(TOKEN_STRING);
    }

    Token number()
    {
        while (isDigit(peek()))
            advance();
        if (peek() == '.' && isDigit(peekNext()))
        {
            advance();
            while (isDigit(peek()))
                advance();
        }
        return makeToken(TOKEN_NUMBER);
    }

    TokenType checkKeyword(int startOffset, int length, const char *rest, TokenType type)
    {
        if (current - start == startOffset + length &&
            memcmp(start + startOffset, rest, length) == 0)
            return type;
        return TOKEN_IDENTIFIER;
    }

    TokenType identifierType()
    {
        switch (start[0])
        {
        case 'a':
            return checkKeyword(1, 2, "nd", TOKEN_AND);
        case 'c':
            if (current - start > 1)
            {
                switch (start[1])
                {
                case 'l':
                    return checkKeyword(2, 3, "ass", TOKEN_CLASS);
                case 'o':
                    return checkKeyword(2, 6, "ntinue", TOKEN_CONTINUE);
                }
            }
            break;
        case 'e':
            return checkKeyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (current - start > 1)
            {
                switch (start[1])
                {
                case 'a':
                    return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                case 'o':
                    return checkKeyword(2, 1, "r", TOKEN_FOR);
                case 'u':
                    return checkKeyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i':
            return checkKeyword(1, 1, "f", TOKEN_IF);
        case 'n':
            return checkKeyword(1, 2, "il", TOKEN_NIL);
        case 'o':
            return checkKeyword(1, 1, "r", TOKEN_OR);
        case 'p':
            return checkKeyword(1, 4, "rint", TOKEN_PRINT);
        case 'r':
            return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 's':
            return checkKeyword(1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (current - start > 1)
            {
                switch (start[1])
                {
                case 'h':
                    return checkKeyword(2, 2, "is", TOKEN_THIS);
                case 'r':
                    return checkKeyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v':
            return checkKeyword(1, 2, "ar", TOKEN_VAR);
        case 'w':
            return checkKeyword(1, 4, "hile", TOKEN_WHILE);
        }
        return TOKEN_IDENTIFIER;
    }

    Token identifier()
    {
        while (isAlpha(peek()) || isDigit(peek()))
            advance();
        return makeToken(identifierType());
    }

public:
    Scanner() : start(nullptr), current(nullptr), line(1) {}

    Scanner(const char *source) : start(source), current(source), line(1) {}

    Token scanToken()
    {
        skipWhitespace();
        start = current;

        if (isAtEnd())
            return makeToken(TOKEN_EOF);

        char c = advance();
        if (isDigit(c))
            return number();
        if (isAlpha(c))
            return identifier();

        switch (c)
        {
        case '(':
            return makeToken(TOKEN_LEFT_PAREN);
        case ')':
            return makeToken(TOKEN_RIGHT_PAREN);
        case '{':
            return makeToken(TOKEN_LEFT_BRACE);
        case '}':
            return makeToken(TOKEN_RIGHT_BRACE);
        case '[':
            return makeToken(TOKEN_LEFT_BRACKET);
        case ']':
            return makeToken(TOKEN_RIGHT_BRACKET);
        case ',':
            return makeToken(TOKEN_COMMA);
        case '.':
            return makeToken(TOKEN_DOT);
        case '-':
            return makeToken(TOKEN_MINUS);
        case '+':
            return makeToken(TOKEN_PLUS);
        case ';':
            return makeToken(TOKEN_SEMICOLON);
        case '/':
            return makeToken(TOKEN_SLASH);
        case '*':
            return makeToken(TOKEN_STAR);
        case '!':
            return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"':
            return stringToken();
        }

        return errorToken("Unexpected character.");
    }
};

#endif
