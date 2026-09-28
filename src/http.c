/**
 * @file http.c
 * @brief Implementação do parser HTTP e funções de segurança de caminho.
 */
#include "http.h"

#include <ctype.h>
#include <string.h>

/** Copia `len` bytes para `dst` garantindo terminação. -1 se não couber. */
static int copy_token(char *dst, size_t dst_size, const char *src, size_t len)
{
    if (len == 0 || len >= dst_size)
        return -1;
    memcpy(dst, src, len);
    dst[len] = '\0';
    return 0;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int url_decode(const char *src, char *dst, size_t dst_size)
{
    size_t out = 0;

    for (size_t i = 0; src[i] != '\0'; i++) {
        if (out + 1 >= dst_size)
            return -1;

        if (src[i] == '%') {
            int hi = hex_value(src[i + 1]);
            int lo = hi < 0 ? -1 : hex_value(src[i + 2]);
            if (hi < 0 || lo < 0)
                return -1;
            char c = (char)(hi * 16 + lo);
            if (c == '\0')
                return -1; /* %00 poderia truncar o caminho */
            dst[out++] = c;
            i += 2;
        } else {
            dst[out++] = src[i];
        }
    }
    dst[out] = '\0';
    return 0;
}

int path_is_safe(const char *path)
{
    if (path == NULL || path[0] != '/')
        return 0;
    if (strchr(path, '\\') != NULL)
        return 0;

    /* Percorre segmento a segmento procurando exatamente ".." */
    const char *seg = path;
    while (*seg != '\0') {
        while (*seg == '/')
            seg++;
        const char *end = strchr(seg, '/');
        size_t len = end ? (size_t)(end - seg) : strlen(seg);

        if (len == 2 && seg[0] == '.' && seg[1] == '.')
            return 0;

        seg += len;
    }
    return 1;
}

HttpParseResult http_parse_request_line(const char *raw, HttpRequest *req)
{
    memset(req, 0, sizeof *req);

    const char *line_end = strstr(raw, "\r\n");
    if (line_end == NULL)
        return HTTP_PARSE_BAD_REQUEST;

    /* MÉTODO */
    const char *sp1 = memchr(raw, ' ', (size_t)(line_end - raw));
    if (sp1 == NULL || copy_token(req->method, sizeof req->method, raw, (size_t)(sp1 - raw)) != 0)
        return HTTP_PARSE_BAD_REQUEST;
    for (const char *m = req->method; *m; m++) {
        if (!isupper((unsigned char)*m))
            return HTTP_PARSE_BAD_REQUEST;
    }

    /* URI */
    const char *uri = sp1 + 1;
    const char *sp2 = memchr(uri, ' ', (size_t)(line_end - uri));
    if (sp2 == NULL || sp2 == uri)
        return HTTP_PARSE_BAD_REQUEST;

    size_t uri_len = (size_t)(sp2 - uri);
    if (uri_len >= HTTP_PATH_MAX)
        return HTTP_PARSE_URI_TOO_LONG;

    char raw_path[HTTP_PATH_MAX];
    memcpy(raw_path, uri, uri_len);
    raw_path[uri_len] = '\0';

    char *query = strpbrk(raw_path, "?#");
    if (query != NULL)
        *query = '\0'; /* ignora query string e fragmento */

    if (url_decode(raw_path, req->path, sizeof req->path) != 0)
        return HTTP_PARSE_BAD_REQUEST;

    /* VERSÃO */
    const char *ver = sp2 + 1;
    if (copy_token(req->version, sizeof req->version, ver, (size_t)(line_end - ver)) != 0)
        return HTTP_PARSE_BAD_REQUEST;
    if (strncmp(req->version, "HTTP/", 5) != 0)
        return HTTP_PARSE_BAD_REQUEST;
    if (strcmp(req->version, "HTTP/1.0") != 0 && strcmp(req->version, "HTTP/1.1") != 0)
        return HTTP_PARSE_BAD_VERSION;

    return HTTP_PARSE_OK;
}

const char *http_status_text(int status)
{
    switch (status) {
    case 200: return "OK";
    case 301: return "Moved Permanently";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 414: return "URI Too Long";
    case 500: return "Internal Server Error";
    case 505: return "HTTP Version Not Supported";
    }
    return "Unknown";
}
