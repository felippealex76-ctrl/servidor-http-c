/**
 * @file http.h
 * @brief Parsing da requisição HTTP/1.x e utilitários de protocolo.
 */
#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

#define HTTP_METHOD_MAX  8
#define HTTP_PATH_MAX    1024
#define HTTP_VERSION_MAX 16

typedef struct {
    char method[HTTP_METHOD_MAX];   /* "GET", "HEAD", ... */
    char path[HTTP_PATH_MAX];       /* já decodificado e sem query string */
    char version[HTTP_VERSION_MAX]; /* "HTTP/1.1" */
} HttpRequest;

typedef enum {
    HTTP_PARSE_OK = 0,
    HTTP_PARSE_BAD_REQUEST,   /* 400 */
    HTTP_PARSE_URI_TOO_LONG,  /* 414 */
    HTTP_PARSE_BAD_VERSION    /* 505 */
} HttpParseResult;

/**
 * Faz o parsing da linha de requisição (ex.: "GET /index.html HTTP/1.1").
 * `raw` deve conter ao menos a primeira linha terminada em "\r\n".
 */
HttpParseResult http_parse_request_line(const char *raw, HttpRequest *req);

/** Texto padrão do código de status (200 -> "OK"). */
const char *http_status_text(int status);

/**
 * Decodifica percent-encoding ("%20" -> ' '). Rejeita sequências inválidas
 * e o byte nulo (%00). Retorna 0 em sucesso, -1 em erro.
 */
int url_decode(const char *src, char *dst, size_t dst_size);

/** Retorna 1 se o caminho é seguro (começa com '/', sem segmentos ".."). */
int path_is_safe(const char *path);

#endif /* HTTP_H */
