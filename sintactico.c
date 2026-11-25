/* ==========================================================================
   sintactico.c -- Analizador sintáctico descendente recursivo (LL(1)).

   Gramática (símbolo inicial: programa), sin recursión por la izquierda y
   factorizada (diapositivas 3.1 y 4.4):

     programa     ::= { sentencia } EOF
     sentencia    ::= declaracion | asignacion | condicional | mientras | bloque
     declaracion  ::= tipo declarador { ',' declarador } ';'
     tipo         ::= int | float | char | string
     declarador   ::= ID [ '=' expresion ]
     asignacion   ::= ID '=' expresion ';'
     condicional  ::= if '(' expresion ')' sentencia [ else sentencia ]
     mientras     ::= while '(' expresion ')' sentencia
     bloque       ::= '{' { sentencia } '}'

     expresion    ::= expr_rel
     expr_rel     ::= expr_ad rel_p      rel_p  ::= OP_REL expr_ad rel_p | ε
     expr_ad      ::= termino ad_p       ad_p   ::= ('+' | '-') termino ad_p | ε
     termino      ::= unario term_p      term_p ::= ('*' | '/') unario term_p | ε
     unario       ::= '-' unario | primario
     primario     ::= NUMBER | ID | CADENA | CARACTER | '(' expresion ')'

   Cada no terminal es una función. Con el token de anticipación (lookahead)
   la función decide qué producción aplicar. Cuando ninguna opción coincide y
   la regla tiene ε, la función no hace nada (el token pertenece a FOLLOW).

   Recuperación de errores (modo pánico, diapositiva 4.3): al detectar un
   error se reporta, se descartan tokens hasta un ';' (que se consume) o hasta
   un token que inicia sentencia, y el análisis continúa.
   ========================================================================== */

#include <setjmp.h>
#include <stdio.h>
#include "errores.h"
#include "sintactico.h"

/* ------------------------------------------------------------------ estado */
typedef struct {
    const Token *tokens;      /* tokens del léxico; el último es EOF */
    int pos;                  /* índice del token de anticipación */
    int errores;              /* errores sintácticos reportados */
    int ult_linea, ult_col;   /* ubicación del último error (evita duplicados) */
    const char *fuente;       /* código fuente, para mostrar la línea del error */
    jmp_buf *recuperacion;    /* punto de recuperación actual (modo pánico) */
} Parser;

static Parser P;

static void sentencia(void);
static void sentencia_r(void);
static void expresion(void);

/* --------------------------------------------------------------- utilidades */
static const Token *actual(void)     { return &P.tokens[P.pos]; }
static TokenType tipo_actual(void)   { return P.tokens[P.pos].tipo; }

static void avanzar(void)
{
    if (tipo_actual() != TOKEN_EOF)
        P.pos++;
}

static const char *nombre_esperado(TokenType t)
{
    switch (t) {
        case TOKEN_SEMICOLON: return "';'";
        case TOKEN_COMMA:     return "','";
        case TOKEN_LPAREN:    return "'('";
        case TOKEN_RPAREN:    return "')'";
        case TOKEN_LBRACE:    return "'{'";
        case TOKEN_RBRACE:    return "'}'";
        case TOKEN_ASSIGN:    return "'='";
        case TOKEN_ID:        return "un identificador";
        default:              return "un token válido";
    }
}

/* Registra un error en la posición del token actual (sin evitar el flujo). */
static void registrar(const char *mensaje)
{
    const Token *t = actual();
    if (t->linea == P.ult_linea && t->columna == P.ult_col)
        return;                               /* evita duplicados en la misma posición */
    P.ult_linea = t->linea;
    P.ult_col = t->columna;
    P.errores++;
    reportar_error("sintáctico", t->linea, t->columna, mensaje, P.fuente);
}

/* Registra el error y salta al punto de recuperación más cercano. */
static void lanzar(const char *mensaje)
{
    registrar(mensaje);
    longjmp(*P.recuperacion, 1);
}

/* Mensaje del enunciado: "Se esperaba ')' antes de ';'." */
static void esperaba(const char *que)
{
    char msg[512];
    const Token *t = actual();
    if (t->tipo == TOKEN_EOF)
        snprintf(msg, sizeof msg, "Se esperaba %s antes del fin del archivo.", que);
    else
        snprintf(msg, sizeof msg, "Se esperaba %s antes de '%s'.", que, t->lexema);
    lanzar(msg);
}

/* match(tipo): consume el token esperado o reporta error. */
static void consumir(TokenType esperado)
{
    if (tipo_actual() == esperado)
        avanzar();
    else
        esperaba(nombre_esperado(esperado));
}

/* ------------------------------------------------------------ recuperación */
static int es_inicio_sentencia(TokenType t)
{
    switch (t) {
        case TOKEN_INT: case TOKEN_FLOAT: case TOKEN_CHAR: case TOKEN_STRING:
        case TOKEN_ID: case TOKEN_IF: case TOKEN_WHILE:
        case TOKEN_LBRACE: case TOKEN_RBRACE:
            return 1;
        default:
            return 0;
    }
}

/* Modo pánico: descarta tokens hasta un punto seguro. */
static void sincronizar(void)
{
    while (tipo_actual() != TOKEN_EOF) {
        if (tipo_actual() == TOKEN_SEMICOLON) {
            avanzar();                        /* el ';' cierra la sentencia fallida */
            return;
        }
        if (es_inicio_sentencia(tipo_actual()))
            return;
        avanzar();
    }
}

/* Analiza una sentencia; si falla, se recupera y el análisis continúa. */
static void sentencia_r(void)
{
    jmp_buf local;
    jmp_buf *previo = P.recuperacion;
    int inicio = P.pos;

    P.recuperacion = &local;
    if (setjmp(local) == 0) {
        sentencia();
    } else {
        sincronizar();
        if (P.pos == inicio && tipo_actual() != TOKEN_EOF)
            avanzar();                        /* garantiza progreso (sin ciclos infinitos) */
    }
    P.recuperacion = previo;
}

/* ------------------------------------------------------------- expresiones */
/* expresion ::= expr_rel */
static void expresion(void);

/* primario ::= NUMBER | ID | CADENA | CARACTER | '(' expresion ')' */
static void primario(void)
{
    switch (tipo_actual()) {
        case TOKEN_ID:
        case TOKEN_NUM_INT:
        case TOKEN_NUM_FLOAT:
        case TOKEN_CHAR_LIT:
        case TOKEN_STRING_LIT:
            avanzar();
            break;
        case TOKEN_LPAREN:
            avanzar();
            expresion();
            consumir(TOKEN_RPAREN);
            break;
        default:
            esperaba("un operando (identificador, número o '(')");
    }
}

/* unario ::= '-' unario | primario */
static void unario(void)
{
    if (tipo_actual() == TOKEN_MINUS) {
        avanzar();
        unario();
    } else {
        primario();
    }
}

/* term_p ::= ('*' | '/') unario term_p | ε */
static void term_p(void)
{
    if (tipo_actual() == TOKEN_MUL || tipo_actual() == TOKEN_DIV) {
        avanzar();
        unario();
        term_p();
    }
}

/* termino ::= unario term_p */
static void termino(void)
{
    unario();
    term_p();
}

/* ad_p ::= ('+' | '-') termino ad_p | ε */
static void ad_p(void)
{
    if (tipo_actual() == TOKEN_PLUS || tipo_actual() == TOKEN_MINUS) {
        avanzar();
        termino();
        ad_p();
    }
}

/* expr_ad ::= termino ad_p */
static void expr_ad(void)
{
    termino();
    ad_p();
}

static int es_relacional(TokenType t)
{
    return t == TOKEN_LT || t == TOKEN_GT || t == TOKEN_LE ||
           t == TOKEN_GE || t == TOKEN_EQ || t == TOKEN_NEQ;
}

/* rel_p ::= OP_REL expr_ad rel_p | ε */
static void rel_p(void)
{
    if (es_relacional(tipo_actual())) {
        avanzar();
        expr_ad();
        rel_p();
    }
}

/* expr_rel ::= expr_ad rel_p */
static void expr_rel(void)
{
    expr_ad();
    rel_p();
}

static void expresion(void)
{
    expr_rel();
}

/* -------------------------------------------------------------- sentencias */
/* declarador ::= ID [ '=' expresion ] */
static void declarador(void)
{
    consumir(TOKEN_ID);
    if (tipo_actual() == TOKEN_ASSIGN) {
        avanzar();
        expresion();
    }
}

/* declaracion ::= tipo declarador { ',' declarador } ';' */
static void declaracion(void)
{
    avanzar();                                /* tipo (ya verificado en sentencia) */
    declarador();
    while (tipo_actual() == TOKEN_COMMA) {
        avanzar();
        declarador();
    }
    consumir(TOKEN_SEMICOLON);
}

/* asignacion ::= ID '=' expresion ';' */
static void asignacion(void)
{
    consumir(TOKEN_ID);
    consumir(TOKEN_ASSIGN);
    expresion();
    consumir(TOKEN_SEMICOLON);
}

/* condicional ::= 'if' '(' expresion ')' sentencia [ 'else' sentencia ] */
static void condicional(void)
{
    avanzar();                                /* 'if' */
    consumir(TOKEN_LPAREN);
    expresion();
    consumir(TOKEN_RPAREN);
    sentencia_r();
    /* "else colgante": como en C, el 'else' se asocia con el 'if' más cercano. */
    if (tipo_actual() == TOKEN_ELSE) {
        avanzar();
        sentencia_r();
    }
}

/* mientras ::= 'while' '(' expresion ')' sentencia */
static void mientras(void)
{
    avanzar();                                /* 'while' */
    consumir(TOKEN_LPAREN);
    expresion();
    consumir(TOKEN_RPAREN);
    sentencia_r();
}

/* bloque ::= '{' { sentencia } '}' */
static void bloque(void)
{
    const Token *abre = actual();
    avanzar();                                /* '{' */

    while (tipo_actual() != TOKEN_RBRACE && tipo_actual() != TOKEN_EOF)
        sentencia_r();

    if (tipo_actual() == TOKEN_EOF) {
        char msg[512];
        snprintf(msg, sizeof msg,
                 "Se esperaba '}' antes del fin del archivo (el bloque abierto en "
                 "línea %d, columna %d no se cerró).", abre->linea, abre->columna);
        lanzar(msg);
    }
    avanzar();                                /* '}' */
}

/* sentencia ::= declaracion | asignacion | condicional | mientras | bloque */
static void sentencia(void)
{
    switch (tipo_actual()) {
        case TOKEN_INT:
        case TOKEN_FLOAT:
        case TOKEN_CHAR:
        case TOKEN_STRING:
            declaracion();
            break;
        case TOKEN_ID:
            asignacion();
            break;
        case TOKEN_IF:
            condicional();
            break;
        case TOKEN_WHILE:
            mientras();
            break;
        case TOKEN_LBRACE:
            bloque();
            break;
        case TOKEN_ELSE:
            lanzar("'else' sin su 'if' correspondiente.");
            break;
        default:
            esperaba("una sentencia (declaración, asignación, if, while o bloque)");
    }
}

/* ------------------------------------------------------------ punto de entrada */
/* programa ::= { sentencia } EOF */
int analizar_sintaxis(const Token *tokens, const char *fuente)
{
    P.tokens = tokens;
    P.pos = 0;
    P.errores = 0;
    P.ult_linea = -1;
    P.ult_col = -1;
    P.fuente = fuente;
    P.recuperacion = NULL;

    while (tipo_actual() != TOKEN_EOF) {
        if (tipo_actual() == TOKEN_RBRACE) {
            registrar("'}' sin su '{' correspondiente.");
            avanzar();
        } else {
            sentencia_r();
        }
    }
    return P.errores;
}
