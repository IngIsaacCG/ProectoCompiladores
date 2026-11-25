#ifndef TOKENS_H
#define TOKENS_H

typedef enum {
    /* palabras reservadas */
    TOKEN_INT, TOKEN_FLOAT, TOKEN_CHAR, TOKEN_STRING, TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE,

    /* identificadores y constantes */
    TOKEN_ID, TOKEN_NUM_INT, TOKEN_NUM_FLOAT, TOKEN_CHAR_LIT, TOKEN_STRING_LIT,

    /* operadores */
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_MUL, TOKEN_DIV, TOKEN_ASSIGN,
    TOKEN_EQ, TOKEN_NEQ, TOKEN_LT, TOKEN_GT, TOKEN_LE, TOKEN_GE,

    /* delimitadores */
    TOKEN_SEMICOLON, TOKEN_COMMA, TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE,

    /* control */
    TOKEN_EOF, TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType tipo;
    char lexema[256];
    int linea;
    int columna;
} Token;

#endif
