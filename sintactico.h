#ifndef SINTACTICO_H
#define SINTACTICO_H

#include "tokens.h"

/* Analiza la secuencia de tokens (terminada en TOKEN_EOF).
   Reporta cada error sintáctico y devuelve cuántos encontró.
   'fuente' es el código original, para mostrar la línea del error. */
int analizar_sintaxis(const Token *tokens, const char *fuente);

#endif
