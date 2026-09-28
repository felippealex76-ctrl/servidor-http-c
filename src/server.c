/**
 * @file server.c
 * @brief Loop principal do servidor: sockets, leitura da requisição,
 *        resolução do arquivo e envio da resposta.
 *
 * Modelo: iterativo (uma conexão por vez), "Connection: close".
 * Simples de entender e suficiente para servir um site estático local.
 */
#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "http.h"
#include "mime.h"

#define REQUEST_BUF_SIZE 8192
#define FILE_CHUNK_SIZE  16384
#define LISTEN_BACKLOG   16
#define RECV_TIMEOUT_SEC 5
#define SERVER_NAME      "servidor-http-c/1.0"

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

/* ------------------------------------------------------------------ */
/* Envio                                                              */
/* ------------------------------------------------------------------ */

/** Envia todos os bytes, tratando envios parciais e EINTR. */
static int send_all(int fd, const void *data, size_t len)
{
    const char *p = data;
    while (len > 0) {
        ssize_t n = send(fd, p, len, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        p   += n;
        len -= (size_t)n;
    }
    return 0;
}

static int send_headers(int fd, int status, const char *content_type,
                        long long content_length, const char *extra)
{
    char date[64];
    time_t now = time(NULL);
    struct tm gmt;
    gmtime_r(&now, &gmt);
    strftime(date, sizeof date, "%a, %d %b %Y %H:%M:%S GMT", &gmt);

    char header[1024];
    int n = snprintf(header, sizeof header,
                     "HTTP/1.1 %d %s\r\n"
                     "Server: " SERVER_NAME "\r\n"
                     "Date: %s\r\n"
                     "Content-Type: %s\r\n"
                     "Content-Length: %lld\r\n"
                     "X-Content-Type-Options: nosniff\r\n"
                     "%s"
                     "Connection: close\r\n"
                     "\r\n",
                     status, http_status_text(status), date, content_type,
                     content_length, extra ? extra : "");
    if (n < 0 || (size_t)n >= sizeof header)
        return -1;
    return send_all(fd, header, (size_t)n);
}

/** Envia uma página de erro em HTML. Retorna o tamanho do corpo. */
static long long send_error(int fd, int status, int head_only, const char *extra)
{
    char body[512];
    int n = snprintf(body, sizeof body,
                     "<!DOCTYPE html><html lang=\"pt-BR\"><head><meta charset=\"utf-8\">"
                     "<title>%d %s</title></head><body style=\"font-family:sans-serif;"
                     "text-align:center;margin-top:15vh\"><h1>%d</h1><p>%s</p>"
                     "<hr><small>" SERVER_NAME "</small></body></html>",
                     status, http_status_text(status), status, http_status_text(status));

    send_headers(fd, status, "text/html; charset=utf-8", n, extra);
    if (!head_only)
        send_all(fd, body, (size_t)n);
    return head_only ? 0 : n;
}

/* ------------------------------------------------------------------ */
/* Tratamento da requisição                                           */
/* ------------------------------------------------------------------ */

/** Lê do socket até encontrar o fim dos cabeçalhos ("\r\n\r\n"). */
static ssize_t read_request(int fd, char *buf, size_t size)
{
    size_t total = 0;
    while (total < size - 1) {
        ssize_t n = recv(fd, buf + total, size - 1 - total, 0);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return -1; /* erro, timeout ou cliente fechou */
        total += (size_t)n;
        buf[total] = '\0';
        if (strstr(buf, "\r\n\r\n") != NULL)
            return (ssize_t)total;
    }
    return -1; /* cabeçalhos grandes demais */
}

/** Garante que o arquivo resolvido (seguindo symlinks) está dentro da raiz. */
static int is_inside_root(const char *root_real, const char *file_path)
{
    char file_real[PATH_MAX];
    if (realpath(file_path, file_real) == NULL)
        return 0;

    size_t len = strlen(root_real);
    return strncmp(file_real, root_real, len) == 0 &&
           (file_real[len] == '/' || file_real[len] == '\0');
}

/** Atende uma conexão. Retorna o status HTTP enviado e preenche bytes/req. */
static int handle_client(int client_fd, const char *root_real,
                         HttpRequest *req, long long *bytes_sent)
{
    char buf[REQUEST_BUF_SIZE];
    *bytes_sent = 0;

    if (read_request(client_fd, buf, sizeof buf) < 0) {
        *bytes_sent = send_error(client_fd, 400, 0, NULL);
        return 400;
    }

    switch (http_parse_request_line(buf, req)) {
    case HTTP_PARSE_OK:
        break;
    case HTTP_PARSE_URI_TOO_LONG:
        *bytes_sent = send_error(client_fd, 414, 0, NULL);
        return 414;
    case HTTP_PARSE_BAD_VERSION:
        *bytes_sent = send_error(client_fd, 505, 0, NULL);
        return 505;
    default:
        *bytes_sent = send_error(client_fd, 400, 0, NULL);
        return 400;
    }

    int head_only = strcmp(req->method, "HEAD") == 0;
    if (!head_only && strcmp(req->method, "GET") != 0) {
        *bytes_sent = send_error(client_fd, 405, 0, "Allow: GET, HEAD\r\n");
        return 405;
    }

    if (!path_is_safe(req->path)) {
        *bytes_sent = send_error(client_fd, 403, head_only, NULL);
        return 403;
    }

    char file_path[PATH_MAX];
    int n = snprintf(file_path, sizeof file_path, "%s%s", root_real, req->path);
    if (n < 0 || (size_t)n >= sizeof file_path) {
        *bytes_sent = send_error(client_fd, 414, head_only, NULL);
        return 414;
    }

    struct stat st;
    if (stat(file_path, &st) != 0) {
        int status = (errno == EACCES) ? 403 : 404;
        *bytes_sent = send_error(client_fd, status, head_only, NULL);
        return status;
    }

    if (S_ISDIR(st.st_mode)) {
        size_t plen = strlen(req->path);
        if (req->path[plen - 1] != '/') {
            /* "/docs" -> "/docs/" para que links relativos funcionem */
            char location[HTTP_PATH_MAX + 32];
            snprintf(location, sizeof location, "Location: %s/\r\n", req->path);
            *bytes_sent = send_error(client_fd, 301, head_only, location);
            return 301;
        }
        if (strlen(file_path) + strlen("index.html") >= sizeof file_path) {
            *bytes_sent = send_error(client_fd, 414, head_only, NULL);
            return 414;
        }
        strcat(file_path, "index.html");
        if (stat(file_path, &st) != 0) {
            *bytes_sent = send_error(client_fd, 404, head_only, NULL);
            return 404;
        }
    }

    if (!S_ISREG(st.st_mode) || !is_inside_root(root_real, file_path)) {
        *bytes_sent = send_error(client_fd, 403, head_only, NULL);
        return 403;
    }

    int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) {
        int status = (errno == EACCES) ? 403 : 500;
        *bytes_sent = send_error(client_fd, status, head_only, NULL);
        return status;
    }

    if (send_headers(client_fd, 200, mime_from_path(file_path),
                     (long long)st.st_size, NULL) != 0) {
        close(file_fd);
        return 200;
    }

    if (!head_only) {
        char chunk[FILE_CHUNK_SIZE];
        ssize_t r;
        while ((r = read(file_fd, chunk, sizeof chunk)) > 0) {
            if (send_all(client_fd, chunk, (size_t)r) != 0)
                break; /* cliente desconectou */
            *bytes_sent += r;
        }
    }

    close(file_fd);
    return 200;
}

static void log_request(const char *client_ip, const HttpRequest *req,
                        int status, long long bytes)
{
    char ts[32];
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &local);

    printf("[%s] %s \"%s %s\" %d %lldB\n", ts, client_ip,
           req->method[0] ? req->method : "-",
           req->path[0] ? req->path : "-", status, bytes);
    fflush(stdout);
}

/* ------------------------------------------------------------------ */
/* Loop principal                                                     */
/* ------------------------------------------------------------------ */

static int create_listen_socket(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    if (listen(fd, LISTEN_BACKLOG) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    return fd;
}

static void install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* sem SA_RESTART: accept() retorna EINTR */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Cliente que fecha a conexão no meio do envio não derruba o servidor */
    signal(SIGPIPE, SIG_IGN);
}

int server_run(const ServerConfig *config)
{
    char root_real[PATH_MAX];
    if (realpath(config->root_dir, root_real) == NULL) {
        fprintf(stderr, "Erro: diretório '%s' não encontrado.\n", config->root_dir);
        return -1;
    }

    struct stat st;
    if (stat(root_real, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Erro: '%s' não é um diretório.\n", root_real);
        return -1;
    }

    int listen_fd = create_listen_socket(config->port);
    if (listen_fd < 0)
        return -1;

    install_signal_handlers();

    printf("Servindo '%s' em http://localhost:%d (Ctrl+C para sair)\n",
           root_real, config->port);
    fflush(stdout);

    while (g_running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof client_addr;

        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (errno == EINTR)
                continue; /* provavelmente Ctrl+C: o while verifica g_running */
            perror("accept");
            continue;
        }

        /* Evita que um cliente lento trave o servidor para sempre */
        struct timeval tv = { .tv_sec = RECV_TIMEOUT_SEC, .tv_usec = 0 };
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

        char client_ip[INET_ADDRSTRLEN] = "?";
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof client_ip);

        HttpRequest req;
        memset(&req, 0, sizeof req);
        long long bytes = 0;
        int status = handle_client(client_fd, root_real, &req, &bytes);
        log_request(client_ip, &req, status, bytes);

        close(client_fd);
    }

    close(listen_fd);
    printf("\nServidor encerrado.\n");
    return 0;
}
