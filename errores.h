#ifndef ERRORES_H
#define ERRORES_H

/* Muestra un diagnóstico con el formato del enunciado, seguido de la línea
   afectada y un '^' bajo la columna del error.
   tipo: "léxico" o "sintáctico". */
void reportar_error(const char *tipo, int linea, int columna,
                    const char *mensaje, const char *fuente);

#endif
