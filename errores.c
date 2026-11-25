#include <stdio.h>
#include "errores.h"

void reportar_error(const char *tipo, int linea, int columna,
                    const char *mensaje, const char *fuente)
{
    printf("Error %s en línea %d, columna %d:\n", tipo, linea, columna);
    printf("  %s\n", mensaje);

    /* Localiza el inicio de la línea 'linea' dentro del código fuente */
    const char *ini = fuente;
    int l = 1;
    while (l < linea && *ini != '\0') {
        if (*ini == '\n')
            l++;
        ini++;
    }
    if (l != linea)
        return;

    /* Muestra la línea y un '^' bajo la columna (se conservan los tabuladores) */
    printf("    ");
    for (const char *p = ini; *p != '\0' && *p != '\n' && *p != '\r'; p++)
        putchar(*p);
    printf("\n    ");

    int c = 1;
    for (const char *p = ini; c < columna && *p != '\0' && *p != '\n'; p++, c++)
        putchar(*p == '\t' ? '\t' : ' ');
    printf("^\n");
}
