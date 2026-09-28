/**
 * @file main.c
 * @brief Ponto de entrada: lê argumentos e inicia o servidor.
 *
 * Uso: servidor [-p porta] [-d diretorio]
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"

#define DEFAULT_PORT 8080
#define DEFAULT_ROOT "./www"

static void print_usage(const char *prog)
{
    printf("Uso: %s [-p porta] [-d diretorio]\n\n", prog);
    printf("  -p porta      Porta TCP (padrão: %d)\n", DEFAULT_PORT);
    printf("  -d diretorio  Pasta com os arquivos do site (padrão: %s)\n", DEFAULT_ROOT);
    printf("  -h            Mostra esta ajuda\n");
}

static int parse_port(const char *s, int *out)
{
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v < 1 || v > 65535)
        return -1;
    *out = (int)v;
    return 0;
}

int main(int argc, char **argv)
{
    ServerConfig config = { .port = DEFAULT_PORT, .root_dir = DEFAULT_ROOT };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            if (parse_port(argv[++i], &config.port) != 0) {
                fprintf(stderr, "Erro: porta inválida '%s' (use 1-65535).\n", argv[i]);
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            config.root_dir = argv[++i];
        } else {
            fprintf(stderr, "Argumento inválido: '%s'\n\n", argv[i]);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    return server_run(&config) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
