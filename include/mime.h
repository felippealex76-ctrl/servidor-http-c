/**
 * @file mime.h
 * @brief Descoberta do Content-Type a partir da extensão do arquivo.
 */
#ifndef MIME_H
#define MIME_H

/** Retorna o tipo MIME do arquivo. Padrão: "application/octet-stream". */
const char *mime_from_path(const char *path);

#endif /* MIME_H */
