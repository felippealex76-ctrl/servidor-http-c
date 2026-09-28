/**
 * @file server.h
 * @brief Servidor HTTP de arquivos estáticos.
 */
#ifndef SERVER_H
#define SERVER_H

typedef struct {
    int         port;      /* porta TCP (1-65535) */
    const char *root_dir;  /* diretório servido */
} ServerConfig;

/**
 * Inicia o servidor e atende conexões até receber SIGINT/SIGTERM.
 * Retorna 0 em encerramento normal e -1 em erro de inicialização.
 */
int server_run(const ServerConfig *config);

#endif /* SERVER_H */
