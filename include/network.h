/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 * Veřejné rozhraní síťové vrstvy a vestavěného HTTP serveru.
 */

#ifndef ASTER_NETWORK_H
#define ASTER_NETWORK_H

#include "types.h"

/**
 * @brief Inicializuje emulovanou ISA síťovou kartu NE2000.
 *
 * @return 1 při úspěchu; jinak 0.
 */
int network_init(void);

/**
 * @brief Zpracuje přijaté ethernetové rámce.
 *
 * Má být volána pravidelně z časovače nebo hlavní smyčky.
 */
void network_poll(void);

/**
 * @brief Zjistí, zda je síťová karta inicializovaná.
 *
 * @return 1, pokud je karta připravená; jinak 0.
 */
int network_is_ready(void);

/**
 * @brief Nastaví soubor nebo adresář AsterFS a TCP port pro HTTP server.
 *
 * @param path Absolutní cesta k hostovanému souboru nebo adresáři.
 * @param port TCP port serveru.
 * @return 0 při úspěchu; jinak -1.
 */
int network_http_serve(const char *path, u16 port);

/**
 * @brief Nastaví vestavěnou domovskou stránku a TCP port HTTP serveru.
 *
 * @param port TCP port serveru.
 * @return 0 při úspěchu; jinak -1.
 */
int network_http_serve_default(u16 port);

/** @brief Zastaví HTTP server a zruší aktivní spojení. */
void network_http_stop(void);

#endif