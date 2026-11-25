/* main.c -- Analizador léxico (tu código) + carga de archivos + analizador sintáctico.
   Uso: analizador archivo.src [archivo2.src ...]                                  */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "tokens.h"
#include "cargar.h"
#include "errores.h"
#include "sintactico.h"

/* ============================ ANALIZADOR LÉXICO ============================ */

typedef struct {
    const char *fuente;
    int pos;
    int linea;
    int columna;
} Lexer;

/* Mira el carácter actual sin moverse */
static char actual(Lexer *lexer)
{
    return lexer->fuente[lexer->pos];
}

/* Mira el carácter siguiente */
static char siguiente(Lexer *lexer)
{
    if (lexer->fuente[lexer->pos] == '\0')
        return '\0';
    return lexer->fuente[lexer->pos + 1];
}

/* Consume el carácter, avanza y actualiza línea y columna */
static char avanzar(Lexer *lexer)
{
    char c = lexer->fuente[lexer->pos];
    if (c != '\0') {
        lexer->pos++;
        if (c == '\n') {
            lexer->linea++;
            lexer->columna = 1;
        } else {
            lexer->columna++;
        }
    }
    return c;
}

static void inicializar_lexer(Lexer *lexer, const char *codigo_fuente)
{
    lexer->fuente = codigo_fuente;
    lexer->pos = 0;
    lexer->linea = 1;
    lexer->columna = 1;
}

static Token crear_token(TokenType tipo, const char *lexema, int linea, int columna)
{
    Token t;
    t.tipo = tipo;
    strncpy(t.lexema, lexema, sizeof(t.lexema) - 1);
    t.lexema[sizeof(t.lexema) - 1] = '\0';
    t.linea = linea;
    t.columna = columna;
    return t;
}

/* Ignora espacios y comentarios (0 si todo bien, -1 si /* no se cerró) */
static int ignorar(Lexer *lexer, int *err_linea, int *err_col)
{
    while (actual(lexer) != '\0') {
        char c = actual(lexer);

        if (isspace((unsigned char)c)) {
            avanzar(lexer);
        } else if (c == '/' && siguiente(lexer) == '/') {
            while (actual(lexer) != '\0' && actual(lexer) != '\n')
                avanzar(lexer);
        } else if (c == '/' && siguiente(lexer) == '*') {
            *err_linea = lexer->linea;
            *err_col = lexer->columna;
            avanzar(lexer);
            avanzar(lexer);

            int cerrado = 0;
            while (actual(lexer) != '\0') {
                if (actual(lexer) == '*' && siguiente(lexer) == '/') {
                    avanzar(lexer);
                    avanzar(lexer);
                    cerrado = 1;
                    break;
                }
                avanzar(lexer);
            }
            if (!cerrado)
                return -1;
        } else {
            break;
        }
    }
    return 0;
}

/* Distingue palabras reservadas de identificadores */
static TokenType v_reservada(const char *lexema)
{
    if (strcmp(lexema, "int") == 0)    return TOKEN_INT;
    if (strcmp(lexema, "float") == 0)  return TOKEN_FLOAT;
    if (strcmp(lexema, "char") == 0)   return TOKEN_CHAR;
    if (strcmp(lexema, "string") == 0) return TOKEN_STRING;
    if (strcmp(lexema, "if") == 0)     return TOKEN_IF;
    if (strcmp(lexema, "else") == 0)   return TOKEN_ELSE;
    if (strcmp(lexema, "while") == 0)  return TOKEN_WHILE;
    return TOKEN_ID;
}

static Token leer_palabra(Lexer *lexer)
{
    int linea_ini = lexer->linea;
    int col_ini = lexer->columna;
    char buffer[256];
    int i = 0;

    while (isalnum((unsigned char)actual(lexer)) || actual(lexer) == '_') {
        if (i < 255)
            buffer[i++] = avanzar(lexer);
        else
            avanzar(lexer);
    }
    buffer[i] = '\0';

    return crear_token(v_reservada(buffer), buffer, linea_ini, col_ini);
}

/* Enteros y flotantes. Un número mal formado como 1.2.3 se lee como 1.2 seguido de '.' */
static Token leer_numero(Lexer *lexer)
{
    int linea_ini = lexer->linea;
    int col_ini = lexer->columna;
    char buffer[256];
    int i = 0;
    int es_float = 0;

    while (isdigit((unsigned char)actual(lexer))) {
        if (i < 255) buffer[i++] = avanzar(lexer);
        else avanzar(lexer);
    }

    if (actual(lexer) == '.' && isdigit((unsigned char)siguiente(lexer))) {
        es_float = 1;
        if (i < 255) buffer[i++] = avanzar(lexer);
        while (isdigit((unsigned char)actual(lexer))) {
            if (i < 255) buffer[i++] = avanzar(lexer);
            else avanzar(lexer);
        }
    }
    buffer[i] = '\0';

    return crear_token(es_float ? TOKEN_NUM_FLOAT : TOKEN_NUM_INT, buffer, linea_ini, col_ini);
}

static Token leer_cadena(Lexer *lexer)
{
    int linea_ini = lexer->linea;
    int col_ini = lexer->columna;
    char buffer[256];
    int i = 0;

    buffer[i++] = avanzar(lexer);                      /* comilla inicial */
    while (actual(lexer) != '\0' && actual(lexer) != '"' && actual(lexer) != '\n') {
        if (i < 254) buffer[i++] = avanzar(lexer);
        else avanzar(lexer);
    }

    if (actual(lexer) == '"') {
        buffer[i++] = avanzar(lexer);
        buffer[i] = '\0';
        return crear_token(TOKEN_STRING_LIT, buffer, linea_ini, col_ini);
    }
    return crear_token(TOKEN_ERROR, "Cadena de texto sin cerrar", linea_ini, col_ini);
}

static Token leer_caracter(Lexer *lexer)
{
    int linea_ini = lexer->linea;
    int col_ini = lexer->columna;
    char buffer[8] = {0};

    buffer[0] = avanzar(lexer);

    if (actual(lexer) == '\0' || actual(lexer) == '\n' || actual(lexer) == '\'') {
        if (actual(lexer) == '\'') avanzar(lexer);
        return crear_token(TOKEN_ERROR, "Literal char vacio o sin cerrar", linea_ini, col_ini);
    }

    buffer[1] = avanzar(lexer);

    if (actual(lexer) == '\'') {
        buffer[2] = avanzar(lexer);
        return crear_token(TOKEN_CHAR_LIT, buffer, linea_ini, col_ini);
    }

    while (actual(lexer) != '\0' && actual(lexer) != '\'' && actual(lexer) != '\n')
        avanzar(lexer);
    if (actual(lexer) == '\'') avanzar(lexer);

    return crear_token(TOKEN_ERROR, "Literal char excede un caracter o sin cerrar",
                       linea_ini, col_ini);
}

/* Devuelve el siguiente token (equivale a getNextToken() de la diapositiva 2.5) */
static Token obsig_token(Lexer *lexer)
{
    int err_linea = 0, err_col = 0;

    if (ignorar(lexer, &err_linea, &err_col) != 0)
        return crear_token(TOKEN_ERROR, "Comentario multilinea sin cerrar", err_linea, err_col);

    char c = actual(lexer);
    if (c == '\0')
        return crear_token(TOKEN_EOF, "EOF", lexer->linea, lexer->columna);

    int linea_ini = lexer->linea;
    int col_ini = lexer->columna;

    if (isalpha((unsigned char)c) || c == '_') return leer_palabra(lexer);
    if (isdigit((unsigned char)c))             return leer_numero(lexer);
    if (c == '"')                              return leer_cadena(lexer);
    if (c == '\'')                             return leer_caracter(lexer);

    avanzar(lexer);

    switch (c) {
        case '+': return crear_token(TOKEN_PLUS, "+", linea_ini, col_ini);
        case '-': return crear_token(TOKEN_MINUS, "-", linea_ini, col_ini);
        case '*': return crear_token(TOKEN_MUL, "*", linea_ini, col_ini);
        case '/': return crear_token(TOKEN_DIV, "/", linea_ini, col_ini);
        case ';': return crear_token(TOKEN_SEMICOLON, ";", linea_ini, col_ini);
        case ',': return crear_token(TOKEN_COMMA, ",", linea_ini, col_ini);
        case '(': return crear_token(TOKEN_LPAREN, "(", linea_ini, col_ini);
        case ')': return crear_token(TOKEN_RPAREN, ")", linea_ini, col_ini);
        case '{': return crear_token(TOKEN_LBRACE, "{", linea_ini, col_ini);
        case '}': return crear_token(TOKEN_RBRACE, "}", linea_ini, col_ini);

        case '=':
            if (actual(lexer) == '=') {
                avanzar(lexer);
                return crear_token(TOKEN_EQ, "==", linea_ini, col_ini);
            }
            return crear_token(TOKEN_ASSIGN, "=", linea_ini, col_ini);

        case '!':
            if (actual(lexer) == '=') {
                avanzar(lexer);
                return crear_token(TOKEN_NEQ, "!=", linea_ini, col_ini);
            }
            return crear_token(TOKEN_ERROR, "Operador '!' invalido (se esperaba '!=')",
                               linea_ini, col_ini);

        case '<':
            if (actual(lexer) == '=') {
                avanzar(lexer);
                return crear_token(TOKEN_LE, "<=", linea_ini, col_ini);
            }
            return crear_token(TOKEN_LT, "<", linea_ini, col_ini);

        case '>':
            if (actual(lexer) == '=') {
                avanzar(lexer);
                return crear_token(TOKEN_GE, ">=", linea_ini, col_ini);
            }
            return crear_token(TOKEN_GT, ">", linea_ini, col_ini);

        default: {
            char msg[64];
            snprintf(msg, sizeof msg, "Caracter no reconocido '%c'", c);
            return crear_token(TOKEN_ERROR, msg, linea_ini, col_ini);
        }
    }
}

/* Nombre del token tal como aparece en el enunciado (INT, ID, NUMBER, GREATER...) */
static const char *nombre_token(TokenType tipo)
{
    switch (tipo) {
        case TOKEN_INT: return "INT";
        case TOKEN_FLOAT: return "FLOAT";
        case TOKEN_CHAR: return "CHAR";
        case TOKEN_STRING: return "STRING";
        case TOKEN_IF: return "IF";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_ID: return "ID";
        case TOKEN_NUM_INT:
        case TOKEN_NUM_FLOAT: return "NUMBER";
        case TOKEN_CHAR_LIT: return "CHAR_LIT";
        case TOKEN_STRING_LIT: return "STRING_LIT";
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_MUL: return "TIMES";
        case TOKEN_DIV: return "DIVIDE";
        case TOKEN_ASSIGN: return "ASSIGN";
        case TOKEN_EQ: return "EQ";
        case TOKEN_NEQ: return "NEQ";
        case TOKEN_LT: return "LESS";
        case TOKEN_GT: return "GREATER";
        case TOKEN_LE: return "LESS_EQ";
        case TOKEN_GE: return "GREATER_EQ";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR_LEXICO";
        default: return "DESCONOCIDO";
    }
}

/* Tokeniza todo el archivo. Los errores léxicos se reportan y NO se pasan
   al parser. Devuelve un arreglo dinámico que termina en TOKEN_EOF. */
static Token *tokenizar(const char *fuente, int *cantidad, int *err_lexicos)
{
    Lexer lx;
    inicializar_lexer(&lx, fuente);

    int cap = 64, n = 0;
    Token *arr = malloc(cap * sizeof *arr);
    if (arr == NULL)
        return NULL;

    for (;;) {
        Token t = obsig_token(&lx);

        if (t.tipo == TOKEN_ERROR) {
            reportar_error("léxico", t.linea, t.columna, t.lexema, fuente);
            (*err_lexicos)++;
            continue;
        }

        if (n == cap) {
            Token *nuevo = realloc(arr, 2 * cap * sizeof *arr);
            if (nuevo == NULL) { free(arr); return NULL; }
            arr = nuevo;
            cap *= 2;
        }
        arr[n++] = t;
        if (t.tipo == TOKEN_EOF)
            break;
    }

    *cantidad = n;
    return arr;
}

/* Secuencia de tokens agrupada por línea (formato del enunciado) */
static void imprimir_secuencia(const Token *t, int n)
{
    int linea_actual = -1;
    for (int i = 0; i < n - 1; i++) {          /* n-1: se omite el EOF */
        if (t[i].linea != linea_actual) {
            if (linea_actual != -1)
                printf("\n");
            printf("  Línea %3d: ", t[i].linea);
            linea_actual = t[i].linea;
        }
        printf("%s ", nombre_token(t[i].tipo));
    }
    printf("\n");
}

/* ============================ PROCESAMIENTO ============================ */

/* Devuelve 0 si es válido, 1 si tiene errores, 2 si no se pudo leer */
static int procesar(const char *ruta)
{
    size_t tam = 0;
    char *fuente = cargar_archivo(ruta, &tam);
    if (fuente == NULL) {
        fprintf(stderr, "No se pudo leer '%s'.\n", ruta);
        return 2;
    }

    printf("==========================================================\n");
    printf("Archivo: %s\n", ruta);
    printf("==========================================================\n");

    printf("\n>>> ANÁLISIS LÉXICO\n");
    int n = 0, err_lex = 0;
    Token *tokens = tokenizar(fuente, &n, &err_lex);
    if (tokens == NULL) {
        fprintf(stderr, "Memoria insuficiente.\n");
        free(fuente);
        return 2;
    }
    printf("\nTokens reconocidos (%d):\n", n - 1);
    imprimir_secuencia(tokens, n);
    printf("\nErrores léxicos: %d\n", err_lex);

    printf("\n>>> ANÁLISIS SINTÁCTICO\n");
    int err_sint = analizar_sintaxis(tokens, fuente);
    printf("\nErrores sintácticos: %d\n", err_sint);

    printf("\n>>> RESULTADO\n");
    if (err_lex + err_sint == 0) {
        printf("  El programa es léxica y sintácticamente VÁLIDO.\n");
    } else {
        printf("  El programa es INVÁLIDO: %d error(es) léxico(s), "
               "%d error(es) sintáctico(s).\n", err_lex, err_sint);
        if (err_lex > 0 && err_sint > 0)
            printf("  Nota: algún error sintáctico puede ser consecuencia de un "
                   "error léxico previo.\n");
    }

    free(tokens);
    free(fuente);
    return (err_lex + err_sint) ? 1 : 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Uso: %s archivo.src [archivo2.src ...]\n", argv[0]);
        return 2;
    }

    int codigo = 0;
    for (int i = 1; i < argc; i++) {
        int r = procesar(argv[i]);
        if (r == 2)
            codigo = 2;
        else if (r == 1 && codigo == 0)
            codigo = 1;
    }
    return codigo;
}
