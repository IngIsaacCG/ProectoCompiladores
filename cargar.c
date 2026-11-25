#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cargar.h"

char *cargar_archivo(const char *ruta, size_t *tamano)
{
    FILE *f = fopen(ruta, "rb");
    if (f == NULL)
        return NULL;

    /* Averiguar el tamaño del archivo */
    if (fseek(f, 0L, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0 || fseek(f, 0L, SEEK_SET) != 0) { fclose(f); return NULL; }

    char *buf = malloc((size_t)n + 1);
    if (buf == NULL) { fclose(f); return NULL; }

    size_t leidos = fread(buf, 1, (size_t)n, f);
    fclose(f);
    if (leidos != (size_t)n) { free(buf); return NULL; }

    size_t tam = (size_t)n;

    /* Quita la marca BOM UTF-8 (EF BB BF) que algunos editores agregan al inicio */
    if (tam >= 3 && (unsigned char)buf[0] == 0xEF &&
        (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF) {
        memmove(buf, buf + 3, tam - 3);
        tam -= 3;
    }

    buf[tam] = '\0';
    if (tamano != NULL)
        *tamano = tam;
    return buf;
}
