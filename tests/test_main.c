/**
 * @file test_main.c
 * @brief Testes unitários do parser HTTP, decodificação de URL,
 *        segurança de caminhos e tabela MIME.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "mime.h"


static int tests_run    = 0;
static int tests_failed = 0;

#define ASSERT(cond)                                                            \
    do {                                                                        \
        if (!(cond)) {                                                          \
            fprintf(stderr, "  FALHOU %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            tests_failed++;                                                     \
            return;                                                             \
        }                                                                       \
    } while (0)

#define RUN_TEST(fn)                                                             \
    do {                                                                         \
        int before = tests_failed;                                               \
        tests_run++;                                                             \
        fn();                                                                    \
        printf("%s %s\n", tests_failed == before ? "[ OK ]" : "[FAIL]", #fn);    \
    } while (0)

/* ---------- parser ---------- */

static void test_parse_simple_get(void)
{
    HttpRequest r;
    ASSERT(http_parse_request_line("GET /index.html HTTP/1.1\r\nHost: x\r\n\r\n", &r) == HTTP_PARSE_OK);
    ASSERT(strcmp(r.method, "GET") == 0);
    ASSERT(strcmp(r.path, "/index.html") == 0);
    ASSERT(strcmp(r.version, "HTTP/1.1") == 0);
}

static void test_parse_strips_query_string(void)
{
    HttpRequest r;
    ASSERT(http_parse_request_line("GET /busca?q=c&page=2 HTTP/1.0\r\n", &r) == HTTP_PARSE_OK);
    ASSERT(strcmp(r.path, "/busca") == 0);
}

static void test_parse_decodes_url(void)
{
    HttpRequest r;
    ASSERT(http_parse_request_line("GET /meu%20arquivo.txt HTTP/1.1\r\n", &r) == HTTP_PARSE_OK);
    ASSERT(strcmp(r.path, "/meu arquivo.txt") == 0);
}

static void test_parse_rejects_malformed(void)
{
    HttpRequest r;
    ASSERT(http_parse_request_line("GET /index.html HTTP/1.1", &r) == HTTP_PARSE_BAD_REQUEST); /* sem CRLF */
    ASSERT(http_parse_request_line("GET\r\n", &r) == HTTP_PARSE_BAD_REQUEST);
    ASSERT(http_parse_request_line("GET /x\r\n", &r) == HTTP_PARSE_BAD_REQUEST);
    ASSERT(http_parse_request_line(" /x HTTP/1.1\r\n", &r) == HTTP_PARSE_BAD_REQUEST);
    ASSERT(http_parse_request_line("get /x HTTP/1.1\r\n", &r) == HTTP_PARSE_BAD_REQUEST);
    ASSERT(http_parse_request_line("GET /x FTP/1.1\r\n", &r) == HTTP_PARSE_BAD_REQUEST);
    ASSERT(http_parse_request_line("GET /%zz HTTP/1.1\r\n", &r) == HTTP_PARSE_BAD_REQUEST);
}

static void test_parse_bad_version(void)
{
    HttpRequest r;
    ASSERT(http_parse_request_line("GET / HTTP/2.0\r\n", &r) == HTTP_PARSE_BAD_VERSION);
}

static void test_parse_uri_too_long(void)
{
    char req[HTTP_PATH_MAX + 64];
    strcpy(req, "GET /");
    memset(req + 5, 'a', HTTP_PATH_MAX);
    strcpy(req + 5 + HTTP_PATH_MAX, " HTTP/1.1\r\n");
    HttpRequest r;
    ASSERT(http_parse_request_line(req, &r) == HTTP_PARSE_URI_TOO_LONG);
}

/* ---------- url_decode ---------- */

static void test_url_decode(void)
{
    char out[64];
    ASSERT(url_decode("/a%2Fb%41", out, sizeof out) == 0);
    ASSERT(strcmp(out, "/a/bA") == 0);
    ASSERT(url_decode("/%00", out, sizeof out) == -1);   /* byte nulo */
    ASSERT(url_decode("/%4", out, sizeof out) == -1);    /* incompleto */
    ASSERT(url_decode("/abcdef", out, 4) == -1);         /* não cabe */
}

/* ---------- path_is_safe ---------- */

static void test_path_safety(void)
{
    ASSERT(path_is_safe("/"));
    ASSERT(path_is_safe("/index.html"));
    ASSERT(path_is_safe("/css/style.css"));
    ASSERT(path_is_safe("/arquivo..txt"));     /* ".." dentro do nome é ok */
    ASSERT(path_is_safe("/.well-known/x"));

    ASSERT(!path_is_safe("/../etc/passwd"));
    ASSERT(!path_is_safe("/css/../../segredo"));
    ASSERT(!path_is_safe("/.."));
    ASSERT(!path_is_safe("//..//x"));
    ASSERT(!path_is_safe("sem-barra"));
    ASSERT(!path_is_safe("/a\\..\\b"));
    ASSERT(!path_is_safe(NULL));
}

static void test_encoded_traversal_is_caught(void)
{
    /* %2e%2e = ".." — o parser decodifica e o path_is_safe bloqueia */
    HttpRequest r;
    ASSERT(http_parse_request_line("GET /%2e%2e/%2e%2e/etc/passwd HTTP/1.1\r\n", &r) == HTTP_PARSE_OK);
    ASSERT(!path_is_safe(r.path));
}

/* ---------- mime ---------- */

static void test_mime_types(void)
{
    ASSERT(strcmp(mime_from_path("/index.html"), "text/html; charset=utf-8") == 0);
    ASSERT(strcmp(mime_from_path("/FOTO.PNG"), "image/png") == 0);
    ASSERT(strcmp(mime_from_path("/app.js"), "text/javascript; charset=utf-8") == 0);
    ASSERT(strcmp(mime_from_path("/sem_extensao"), "application/octet-stream") == 0);
    ASSERT(strcmp(mime_from_path("/v1.2/arquivo"), "application/octet-stream") == 0);
    ASSERT(strcmp(mime_from_path("/arquivo."), "application/octet-stream") == 0);
}

static void test_status_text(void)
{
    ASSERT(strcmp(http_status_text(200), "OK") == 0);
    ASSERT(strcmp(http_status_text(404), "Not Found") == 0);
    ASSERT(strcmp(http_status_text(999), "Unknown") == 0);
}

int main(void)
{
    printf("Executando testes...\n\n");

    RUN_TEST(test_parse_simple_get);
    RUN_TEST(test_parse_strips_query_string);
    RUN_TEST(test_parse_decodes_url);
    RUN_TEST(test_parse_rejects_malformed);
    RUN_TEST(test_parse_bad_version);
    RUN_TEST(test_parse_uri_too_long);
    RUN_TEST(test_url_decode);
    RUN_TEST(test_path_safety);
    RUN_TEST(test_encoded_traversal_is_caught);
    RUN_TEST(test_mime_types);
    RUN_TEST(test_status_text);

    printf("\n%d teste(s), %d falha(s)\n", tests_run, tests_failed);
    return tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
