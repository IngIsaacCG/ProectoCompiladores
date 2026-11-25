#ifndef CARGAR_H
#define CARGAR_H

#include <stddef.h>

/* Lee el archivo completo en memoria dinámica y lo termina en '\0'.
   Devuelve NULL si falla. El llamador debe liberar el resultado con free(). */
char *cargar_archivo(const char *ruta, size_t *tamano);

#endif
