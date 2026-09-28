/**
 * @file mime.c
 * @brief Tabela de extensões -> tipos MIME.
 */
#include "mime.h"

#include <stddef.h>
#include <string.h>
#include <strings.h>

typedef struct {
    const char *ext;
    const char *type;
} MimeEntry;

static const MimeEntry MIME_TABLE[] = {
    { "html", "text/html; charset=utf-8" },
    { "htm",  "text/html; charset=utf-8" },
    { "css",  "text/css; charset=utf-8" },
    { "js",   "text/javascript; charset=utf-8" },
    { "json", "application/json" },
    { "txt",  "text/plain; charset=utf-8" },
    { "md",   "text/markdown; charset=utf-8" },
    { "png",  "image/png" },
    { "jpg",  "image/jpeg" },
    { "jpeg", "image/jpeg" },
    { "gif",  "image/gif" },
    { "svg",  "image/svg+xml" },
    { "ico",  "image/x-icon" },
    { "webp", "image/webp" },
    { "pdf",  "application/pdf" },
    { "woff2","font/woff2" },
};

const char *mime_from_path(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *dot   = strrchr(path, '.');

    /* Sem extensão, ou o ponto está em um diretório (ex.: /v1.2/arquivo) */
    if (dot == NULL || (slash != NULL && dot < slash) || dot[1] == '\0')
        return "application/octet-stream";

    const char *ext = dot + 1;
    for (size_t i = 0; i < sizeof MIME_TABLE / sizeof MIME_TABLE[0]; i++) {
        if (strcasecmp(ext, MIME_TABLE[i].ext) == 0)
            return MIME_TABLE[i].type;
    }
    return "application/octet-stream";
}
