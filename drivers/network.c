/*
 * AsterOS Kernel
 * Autor: Pavel Kalas
 * Rok: 2026
 *
 * Ovladač NE2000 a vestavěný HTTP server pro QEMU NAT.
 */

#include "aster_version.h"
#include "io_ports.h"
#include "network.h"
#include "storage.h"
#include "string.h"
#include "types.h"

#define NE2K_BASE       0x300U
#define NE2K_CMD        0x00U
#define NE2K_PSTART     0x01U
#define NE2K_PSTOP      0x02U
#define NE2K_BNRY       0x03U
#define NE2K_TPSR       0x04U
#define NE2K_TBCR0      0x05U
#define NE2K_TBCR1      0x06U
#define NE2K_ISR        0x07U
#define NE2K_RSAR0      0x08U
#define NE2K_RSAR1      0x09U
#define NE2K_RBCR0      0x0AU
#define NE2K_RBCR1      0x0BU
#define NE2K_RCR        0x0CU
#define NE2K_TCR        0x0DU
#define NE2K_DCR        0x0EU
#define NE2K_IMR        0x0FU
#define NE2K_DATA       0x10U
#define NE2K_RESET      0x1FU

#define NE2K_PAR0       0x01U
#define NE2K_CURR       0x07U

#define NE2K_CR_STP     0x01U
#define NE2K_CR_STA     0x02U
#define NE2K_CR_TXP     0x04U
#define NE2K_CR_RD0     0x08U
#define NE2K_CR_RD1     0x10U
#define NE2K_CR_RD2     0x20U
#define NE2K_CR_PS0     0x40U

#define NE2K_TX_START   0x40U
#define NE2K_RX_START   0x46U
#define NE2K_RX_STOP    0x80U

#define ETHER_HEADER_LEN 14U
#define IPV4_HEADER_LEN  20U
#define TCP_HEADER_LEN   20U
#define FRAME_MAX        1600U
#define HTTP_DEFAULT_PORT 8080U
#define TCP_MAX_PAYLOAD  (FRAME_MAX - ETHER_HEADER_LEN - IPV4_HEADER_LEN - TCP_HEADER_LEN)
#define HTTP_RESPONSE_MAX TCP_MAX_PAYLOAD
#define HTTP_REQUEST_ADDRESS_PART_MAX 48U

#define TCP_FIN 0x01U
#define TCP_SYN 0x02U
#define TCP_RST 0x04U
#define TCP_PSH 0x08U
#define TCP_ACK 0x10U

enum connection_state {
    CONNECTION_CLOSED,
    CONNECTION_SYN_RECEIVED,
    CONNECTION_ESTABLISHED,
    CONNECTION_FIN_SENT
};

enum http_target_result {
    HTTP_TARGET_OK,
    HTTP_TARGET_DIRECTORY,
    HTTP_TARGET_NOT_FOUND,
    HTTP_TARGET_FORBIDDEN
};

static const u8 g_guest_ip[4] = { 10U, 0U, 2U, 15U };

static int g_ready = 0;
static int g_http_enabled = 0;
static int g_http_root_is_directory = 0;
static int g_http_default_page = 0;
static u8 g_mac[6];
static u8 g_rx_frame[FRAME_MAX];
static u8 g_tx_frame[FRAME_MAX];
static char g_http_root[ASTERFS_NAME_LEN];
static u8 g_http_response[HTTP_RESPONSE_MAX];
static u8 g_client_mac[6];
static u8 g_client_ip[4];
static u16 g_client_port = 0;
static u16 g_http_port = HTTP_DEFAULT_PORT;
static u32 g_client_next_seq = 0;
static u32 g_server_next_seq = 0;
static u16 g_ip_identification = 1;
static enum connection_state g_connection = CONNECTION_CLOSED;
static u16 g_directory_listing_position = 0;
static char g_directory_listing_url_prefix[ASTERFS_NAME_LEN];

static const char g_http_default_page_html[] =
    "<!doctype html><html><head><meta charset=\"utf-8\"/><title>"
    "Domovsk\xC3\xA1 str\xC3\xA1nka</title><style>*{margin:0;padding:0;}"
    "h1,p,small{margin:3px;padding:2px;font-family:\"lucida console\";}"
    "h1{color:green;}small{font-size:12px;color:gray;}</style></head><body>"
    "<h1>Funguje to!</h1><p>Pokud vid\xC3\xADs tuto zpr\xC3\xA1vu, "
    "znamen\xC3\xA1 to, \xC5\xBE" "e se HTTP server \xC3\xBAsp\xC4\x9B\xC5\xA1n\xC4\x9B "
    "spustil a funguje.</p></body></html>";

/**
 * @brief Přečte jeden bajt z I/O portu.
 *
 * @param port Adresa I/O portu.
 * @return Přečtená hodnota.
 */
static inline u8 inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/**
 * @brief Přečte 16bitovou hodnotu z I/O portu.
 *
 * @param port Adresa I/O portu.
 * @return Přečtená hodnota.
 */
static inline u16 inw(u16 port) {
    u16 value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/**
 * @brief Načte 16bitovou hodnotu v pořadí big-endian.
 *
 * @param data Ukazatel na nejméně dva bajty vstupu.
 * @return Složená hodnota.
 */
static u16 read_be16(const u8 *data) {
    return (u16)(((u16)data[0] << 8) | data[1]);
}

/**
 * @brief Načte 16bitovou hodnotu v pořadí little-endian.
 *
 * @param data Ukazatel na nejméně dva bajty vstupu.
 * @return Složená hodnota.
 */
static u16 read_le16(const u8 *data) {
    return (u16)((u16)data[0] | ((u16)data[1] << 8));
}

/**
 * @brief Načte 32bitovou hodnotu v pořadí big-endian.
 *
 * @param data Ukazatel na nejméně čtyři bajty vstupu.
 * @return Složená hodnota.
 */
static u32 read_be32(const u8 *data) {
    return ((u32)data[0] << 24)
         | ((u32)data[1] << 16)
         | ((u32)data[2] << 8)
         | (u32)data[3];
}

/**
 * @brief Zapíše 16bitovou hodnotu v pořadí big-endian.
 *
 * @param data Cílový buffer o velikosti alespoň dvou bajtů.
 * @param value Zapisovaná hodnota.
 */
static void write_be16(u8 *data, u16 value) {
    data[0] = (u8)(value >> 8);
    data[1] = (u8)value;
}

/**
 * @brief Zapíše 32bitovou hodnotu v pořadí big-endian.
 *
 * @param data Cílový buffer o velikosti alespoň čtyř bajtů.
 * @param value Zapisovaná hodnota.
 */
static void write_be32(u8 *data, u32 value) {
    data[0] = (u8)(value >> 24);
    data[1] = (u8)(value >> 16);
    data[2] = (u8)(value >> 8);
    data[3] = (u8)value;
}

/**
 * @brief Porovná dva bloky bajtů.
 *
 * @param first První blok.
 * @param second Druhý blok.
 * @param length Počet porovnávaných bajtů.
 * @return 1, pokud jsou bloky stejné; jinak 0.
 */
static int bytes_equal(const u8 *first, const u8 *second, usize length) {
    usize index;

    for (index = 0; index < length; ++index) {
        if (first[index] != second[index]) {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief Připojí text do HTTP odpovědi bez překročení její kapacity.
 *
 * @param response Cílový buffer odpovědi.
 * @param position Aktuální pozice zápisu.
 * @param text Text k připojení.
 * @return Nová pozice zápisu.
 */
static u16 http_append_text(u8 *response, u16 position, const char *text) {
    while (*text && position < HTTP_RESPONSE_MAX) {
        response[position++] = (u8)*text++;
    }

    return position;
}

/**
 * @brief Připojí neznaménkovou 16bitovou hodnotu v desítkové podobě.
 *
 * @param response Cílový buffer odpovědi.
 * @param position Aktuální pozice zápisu.
 * @param value Hodnota k převodu.
 * @return Nová pozice zápisu.
 */
static u16 http_append_u16(u8 *response, u16 position, u16 value) {
    char digits[5];
    usize count = 0;

    do {
        digits[count++] = (char)('0' + (value % 10U));
        value = (u16)(value / 10U);
    } while (value != 0U && count < sizeof(digits));

    while (count > 0U && position < HTTP_RESPONSE_MAX) {
        response[position++] = (u8)digits[--count];
    }

    return position;
}

/**
 * @brief Připojí text bezpečně escapovaný pro HTML.
 *
 * @param response Cílový buffer odpovědi.
 * @param position Aktuální pozice zápisu.
 * @param text Zdrojový text.
 * @param length Délka zdrojového textu v bajtech.
 * @return Nová pozice zápisu.
 */
static u16 http_append_html_escaped(u8 *response, u16 position,
                                    const u8 *text, usize length) {
    usize index;

    for (index = 0; index < length && position < HTTP_RESPONSE_MAX; ++index) {
        switch (text[index]) {
            case '&':
                position = http_append_text(response, position, "&amp;");
                break;
            case '<':
                position = http_append_text(response, position, "&lt;");
                break;
            case '>':
                position = http_append_text(response, position, "&gt;");
                break;
            case '"':
                position = http_append_text(response, position, "&quot;");
                break;
            case '\'':
                position = http_append_text(response, position, "&#39;");
                break;
            default:
                response[position++] = (text[index] >= 0x20U && text[index] <= 0x7EU)
                    ? text[index] : (u8)'?';
                break;
        }
    }

    return position;
}

/**
 * @brief Porovná dva ASCII řetězce bez rozlišení velikosti písmen.
 *
 * @param text První řetězec.
 * @param expected Očekávaný řetězec.
 * @param length Počet porovnávaných bajtů.
 * @return 1 při shodě; jinak 0.
 */
static int http_ascii_equals(const u8 *text, const char *expected, usize length) {
    usize index;

    for (index = 0; index < length; ++index) {
        u8 character = text[index];
        u8 match = (u8)expected[index];

        if (character >= 'A' && character <= 'Z') {
            character = (u8)(character + ('a' - 'A'));
        }
        if (match >= 'A' && match <= 'Z') {
            match = (u8)(match + ('a' - 'A'));
        }
        if (character != match) {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief Rozdělí první řádek HTTP požadavku na metodu a cílovou cestu.
 *
 * @param request Buffer HTTP požadavku.
 * @param length Délka požadavku v bajtech.
 * @param method Výstupní ukazatel na metodu.
 * @param method_length Výstupní délka metody.
 * @param target Výstupní ukazatel na cílovou cestu.
 * @param target_length Výstupní délka cílové cesty.
 * @return 0 při úspěchu; -1 pro neplatný první řádek.
 */
static int http_request_line(const u8 *request, u16 length,
                             const u8 **method, usize *method_length,
                             const u8 **target, usize *target_length) {
    usize index = 0;
    usize start;

    while (index < length && request[index] != ' ' && request[index] != '\r'
           && request[index] != '\n') {
        ++index;
    }
    if (index == 0U || index >= length || request[index] != ' ') {
        return -1;
    }
    *method = request;
    *method_length = index;

    ++index;
    start = index;
    while (index < length && request[index] != ' ' && request[index] != '\r'
           && request[index] != '\n') {
        ++index;
    }
    if (index == start || index >= length || request[index] != ' ') {
        return -1;
    }

    *target = request + start;
    *target_length = index - start;
    return 0;
}

/**
 * @brief Vyhledá a vrátí hodnotu hlavičky HTTP Host.
 *
 * @param request Buffer HTTP požadavku.
 * @param length Délka požadavku v bajtech.
 * @param host Výstupní ukazatel na hodnotu hlavičky.
 * @param host_length Výstupní délka hodnoty hlavičky.
 * @return 0 při nalezení hlavičky; -1, pokud hlavička chybí.
 */
static int http_host_header(const u8 *request, u16 length,
                            const u8 **host, usize *host_length) {
    usize index = 0;

    while (index < length && request[index] != '\n') {
        ++index;
    }
    if (index < length) {
        ++index;
    }

    while (index < length) {
        usize line_start = index;
        usize line_end;
        usize value_start;

        while (index < length && request[index] != '\r' && request[index] != '\n') {
            ++index;
        }
        line_end = index;

        if (line_end >= line_start + 5U
            && http_ascii_equals(request + line_start, "host", 4U)
            && request[line_start + 4U] == ':') {
            value_start = line_start + 5U;
            while (value_start < line_end
                   && (request[value_start] == ' ' || request[value_start] == '\t')) {
                ++value_start;
            }
            while (line_end > value_start
                   && (request[line_end - 1U] == ' ' || request[line_end - 1U] == '\t')) {
                --line_end;
            }
            if (line_end > value_start) {
                *host = request + value_start;
                *host_length = line_end - value_start;
                return 0;
            }
        }

        while (index < length && (request[index] == '\r' || request[index] == '\n')) {
            ++index;
        }
    }

    return -1;
}

/**
 * @brief Připojí absolutní adresu právě zpracovávaného HTTP požadavku.
 *
 * Upřednostní hlavičku Host; pokud chybí, použije statickou guest adresu
 * a aktivní HTTP port.
 *
 * @param response Cílový buffer odpovědi.
 * @param position Aktuální pozice zápisu.
 * @param request Buffer HTTP požadavku.
 * @param request_length Délka požadavku v bajtech.
 * @return Nová pozice zápisu.
 */
static u16 http_append_request_address(u8 *response, u16 position,
                                       const u8 *request, u16 request_length) {
    const u8 *host;
    const u8 *target;
    usize host_length;
    usize method_length;
    usize target_length;

    position = http_append_text(response, position, "http://");
    if (http_host_header(request, request_length, &host, &host_length) == 0) {
        if (host_length > HTTP_REQUEST_ADDRESS_PART_MAX) {
            host_length = HTTP_REQUEST_ADDRESS_PART_MAX;
        }
        position = http_append_html_escaped(response, position, host, host_length);
    } else {
        position = http_append_text(response, position, "10.0.2.15:");
        position = http_append_u16(response, position, g_http_port);
    }

    if (http_request_line(request, request_length, &host, &method_length,
                          &target, &target_length) == 0) {
        if (target_length > HTTP_REQUEST_ADDRESS_PART_MAX) {
            target_length = HTTP_REQUEST_ADDRESS_PART_MAX;
        }
        position = http_append_html_escaped(response, position, target, target_length);
    } else {
        position = http_append_text(response, position, "/");
    }

    return position;
}

/**
 * @brief Sestaví stylizovanou HTML odpověď pro chybu HTTP.
 *
 * @param status Stavová řádka HTTP odpovědi.
 * @param document_title Titulek HTML dokumentu.
 * @param heading Nadpis zobrazený v HTML.
 * @param message Vysvětlující text chyby.
 * @param request Původní HTTP požadavek.
 * @param request_length Délka požadavku v bajtech.
 * @return Délka sestavené odpovědi.
 */
static u16 http_build_error(const char *status, const char *document_title,
                            const char *heading, const char *message,
                            const u8 *request, u16 request_length) {
    u16 position = 0;

    position = http_append_text(g_http_response, position, "HTTP/1.1 ");
    position = http_append_text(g_http_response, position, status);
    position = http_append_text(g_http_response, position, "\r\nContent-Type: text/html; charset=utf-8\r\n");
    position = http_append_text(g_http_response, position, "Connection: close\r\n\r\n");
    position = http_append_text(g_http_response, position,
        "<!doctype html><html><head><meta charset=\"utf-8\"/><title>");
    position = http_append_text(g_http_response, position, document_title);
    position = http_append_text(g_http_response, position,
        "</title><style>*{margin:0;padding:0;}h1,p,small{margin:3px;padding:2px;"
        "font-family:\"lucida console\";}h1{color:red;}small{font-size:12px;color:gray;}"
        "</style></head><body><h1>");
    position = http_append_text(g_http_response, position, heading);
    position = http_append_text(g_http_response, position, "</h1><p>");
    position = http_append_text(g_http_response, position, message);
    position = http_append_text(g_http_response, position, "</p><hr><small>http server at: ");
    position = http_append_request_address(g_http_response, position, request, request_length);
    position = http_append_text(g_http_response, position,
        " - aster core " ASTER_CORE_VERSION_TAG "</small></body></html>");
    return position;
}

/**
 * @brief Sestaví odpověď s vestavěnou domovskou stránkou.
 *
 * Vestavěná stránka je dostupná pouze přes požadavek GET na kořen `/`.
 *
 * @param request Buffer HTTP požadavku.
 * @param request_length Délka požadavku v bajtech.
 * @return Délka sestavené odpovědi.
 */
static u16 http_build_default_page(const u8 *request, u16 request_length) {
    const u8 *method;
    const u8 *target;
    usize method_length;
    usize target_length;
    u16 position = 0;

    if (http_request_line(request, request_length, &method, &method_length,
                          &target, &target_length) != 0
        || method_length != 3U || !bytes_equal(method, (const u8 *)"GET", 3U)) {
        return http_build_error("403 Forbidden", "403!!", "403 - Forbidden",
                                "Pristup k teto strance na tomto serveru byl zamitnut.",
                                request, request_length);
    }
    if (target_length != 1U || target[0] != '/') {
        return http_build_error("404 Not Found", "404!!", "404 - Not found",
                                "Tato stranka na tomto serveru nebyla nalezena.",
                                request, request_length);
    }

    position = http_append_text(g_http_response, position,
                                "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                                "Connection: close\r\n\r\n");
    return http_append_text(g_http_response, position, g_http_default_page_html);
}

/**
 * @brief Připojí jednu položku adresáře do HTML indexu.
 *
 * Adresáře dostávají odkaz s koncovým lomítkem, soubory odkaz na svou
 * relativní URL pod právě zobrazeným adresářem.
 *
 * @param name Název položky adresáře.
 * @param is_dir Nenulová hodnota pro adresář.
 * @param size Velikost souboru v bajtech; zde se nepoužívá.
 */
static void http_append_directory_entry(const char *name, u8 is_dir, u16 size) {
    usize name_length;

    if (!name || g_directory_listing_position >= HTTP_RESPONSE_MAX) {
        return;
    }

    name_length = aster_strlen(name);
    g_directory_listing_position = http_append_text(g_http_response,
                                                    g_directory_listing_position,
                                                    "<p>");
    if (is_dir) {
        g_directory_listing_position = http_append_text(g_http_response,
                                                        g_directory_listing_position,
                                                        "<a href=\"");
        g_directory_listing_position = http_append_html_escaped(g_http_response,
                                                                 g_directory_listing_position,
                                                                 (const u8 *)g_directory_listing_url_prefix,
                                                                 aster_strlen(g_directory_listing_url_prefix));
        g_directory_listing_position = http_append_html_escaped(g_http_response,
                                                                 g_directory_listing_position,
                                                                 (const u8 *)name,
                                                                 name_length);
        g_directory_listing_position = http_append_text(g_http_response,
                                                        g_directory_listing_position,
                                                        "/\">");
        g_directory_listing_position = http_append_html_escaped(g_http_response,
                                                                 g_directory_listing_position,
                                                                 (const u8 *)name,
                                                                 name_length);
        g_directory_listing_position = http_append_text(g_http_response,
                                                        g_directory_listing_position,
                                                        "/</a>");
    } else {
        g_directory_listing_position = http_append_text(g_http_response,
                                                        g_directory_listing_position,
                                                        "<a href=\"");
        g_directory_listing_position = http_append_html_escaped(g_http_response,
                                     g_directory_listing_position,
                                     (const u8 *)g_directory_listing_url_prefix,
                                     aster_strlen(g_directory_listing_url_prefix));
        g_directory_listing_position = http_append_html_escaped(g_http_response,
                                                                 g_directory_listing_position,
                                                                 (const u8 *)name,
                                                                 name_length);
        g_directory_listing_position = http_append_text(g_http_response,
                                                        g_directory_listing_position,
                                                        "\">");
        g_directory_listing_position = http_append_html_escaped(g_http_response,
                                                                 g_directory_listing_position,
                                                                 (const u8 *)name,
                                                                 name_length);
        g_directory_listing_position = http_append_text(g_http_response,
                                                        g_directory_listing_position,
                                                        "</a>");
    }
    g_directory_listing_position = http_append_text(g_http_response,
                                                    g_directory_listing_position,
                                                    "</p>");
    (void)size;
}

/**
 * @brief Nastaví URL prefix pro odkazy generované v indexu adresáře.
 *
 * @param request Buffer HTTP požadavku.
 * @param request_length Délka požadavku v bajtech.
 */
static void http_set_directory_listing_url_prefix(const u8 *request, u16 request_length) {
    const u8 *method;
    const u8 *target;
    usize method_length;
    usize target_length;
    usize index = 0;

    g_directory_listing_url_prefix[index++] = '/';
    if (http_request_line(request, request_length, &method, &method_length,
                          &target, &target_length) == 0
        && target_length > 1U && target[0] == '/') {
        for (index = 0; index + 1U < sizeof(g_directory_listing_url_prefix)
             && index < target_length; ++index) {
            g_directory_listing_url_prefix[index] = (char)target[index];
        }
        if (index > 0U && g_directory_listing_url_prefix[index - 1U] != '/'
            && index + 1U < sizeof(g_directory_listing_url_prefix)) {
            g_directory_listing_url_prefix[index++] = '/';
        }
    }
    g_directory_listing_url_prefix[index] = '\0';
}

/**
 * @brief Sestaví HTML index obsahu zadaného adresáře AsterFS.
 *
 * @param directory Absolutní cesta k vypisovanému adresáři.
 * @param request Buffer HTTP požadavku.
 * @param request_length Délka požadavku v bajtech.
 * @return Délka sestavené odpovědi.
 */
static u16 http_build_directory_listing(const char *directory,
                                        const u8 *request, u16 request_length) {
    u16 position = 0;

    position = http_append_text(g_http_response, position,
        "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n\r\n<!doctype html><html><head>"
        "<meta charset=\"utf-8\"/><title>Index of /</title><style>"
        "*{margin:0;padding:0;}h1,p,small{margin:3px;padding:2px;"
        "font-family:\"lucida console\";}small{font-size:12px;color:gray;}"
        "</style></head><body><h1>Index of /</h1>");
    http_set_directory_listing_url_prefix(request, request_length);
    g_directory_listing_position = position;
    asterfs_list_dir(directory, http_append_directory_entry);
    position = g_directory_listing_position;
    position = http_append_text(g_http_response, position, "<hr><small>http server at: ");
    position = http_append_request_address(g_http_response, position, request, request_length);
    return http_append_text(g_http_response, position,
                            " - aster core " ASTER_CORE_VERSION_TAG "</small></body></html>");
}

/**
 * @brief Určí MIME typ podle přípony hostovaného souboru.
 *
 * @param path Cesta k souboru.
 * @return Hodnota hlavičky Content-Type.
 */
static const char *http_content_type(const char *path) {
    if (aster_ends_with(path, ".htm") || aster_ends_with(path, ".html")) {
        return "text/html; charset=utf-8";
    }
    if (aster_ends_with(path, ".css")) {
        return "text/css; charset=utf-8";
    }
    if (aster_ends_with(path, ".js")) {
        return "text/javascript; charset=utf-8";
    }
    if (aster_ends_with(path, ".json")) {
        return "application/json; charset=utf-8";
    }
    return "text/plain; charset=utf-8";
}

/**
 * @brief Sestaví bezpečnou cestu pod kořenem hostovaného adresáře.
 *
 * Odmítá řídicí znaky, zpětná lomítka a segmenty `..`, aby požadavek
 * nemohl opustit nastavený kořen AsterFS.
 *
 * @param path Cílový buffer pro absolutní cestu.
 * @param suffix URL část relativní vůči kořeni.
 * @param suffix_length Délka URL části v bajtech.
 * @return 0 při úspěchu; -1 při neplatné nebo příliš dlouhé cestě.
 */
static int http_join_root(char *path, const u8 *suffix, usize suffix_length) {
    usize position = 0;
    usize index;

    while (g_http_root[position] && position + 1U < ASTERFS_NAME_LEN) {
        path[position] = g_http_root[position];
        ++position;
    }

    if (g_http_root[position] != '\0') {
        return -1;
    }

    if (position > 0U && path[position - 1U] != '/') {
        if (position + 1U >= ASTERFS_NAME_LEN) {
            return -1;
        }
        path[position++] = '/';
    }

    for (index = 0; index < suffix_length; ++index) {
        u8 character = suffix[index];

        if (character < 0x20U || character == '\\'
            || (character == '.' && index + 1U < suffix_length && suffix[index + 1U] == '.')) {
            return -1;
        }
        if (position + 1U >= ASTERFS_NAME_LEN) {
            return -1;
        }
        path[position++] = (char)character;
    }

    path[position] = '\0';
    return 0;
}

/**
 * @brief Odstraní nadbytečná koncová lomítka z cesty.
 *
 * @param path Cesta upravovaná na místě.
 */
static void http_trim_trailing_slashes(char *path) {
    usize length = aster_strlen(path);

    while (length > 1U && path[length - 1U] == '/') {
        path[--length] = '\0';
    }
}

/**
 * @brief Připojí název potomka k cestě adresáře.
 *
 * @param path Cesta adresáře upravovaná na místě.
 * @param child Název potomka.
 * @return 0 při úspěchu; -1 při prázdném vstupu nebo nedostatku místa.
 */
static int http_append_child_path(char *path, const char *child) {
    usize path_length = aster_strlen(path);
    usize child_length = aster_strlen(child);

    if (path_length == 0U || child_length == 0U
        || path_length + child_length + 2U > ASTERFS_NAME_LEN) {
        return -1;
    }

    if (path[path_length - 1U] != '/') {
        path[path_length++] = '/';
    }
    aster_memcpy(path + path_length, child, child_length + 1U);
    return 0;
}

/**
 * @brief Vyhodnotí požadavek mířící na adresář.
 *
 * Upřednostní soubor `home.htm`, pak zkontroluje marker
 * `.block-content-view`; bez obou vrátí výsledek pro výpis obsahu.
 *
 * @param path Cesta adresáře, případně výstupní cesta k `home.htm`.
 * @return Výsledek směrování HTTP cíle.
 */
static enum http_target_result http_directory_target(char *path) {
    char candidate[ASTERFS_NAME_LEN];

    aster_strcpy(candidate, path);
    if (http_append_child_path(candidate, "home.htm") != 0) {
        return HTTP_TARGET_NOT_FOUND;
    }
    if (asterfs_get_type(candidate) == 0) {
        aster_strcpy(path, candidate);
        return HTTP_TARGET_OK;
    }

    aster_strcpy(candidate, path);
    if (http_append_child_path(candidate, ".block-content-view") != 0) {
        return HTTP_TARGET_NOT_FOUND;
    }
    if (asterfs_get_type(candidate) == 0) {
        return HTTP_TARGET_FORBIDDEN;
    }

    return HTTP_TARGET_DIRECTORY;
}

/**
 * @brief Přeloží HTTP GET požadavek na soubor, adresář nebo stav chyby.
 *
 * @param request Buffer HTTP požadavku.
 * @param length Délka požadavku v bajtech.
 * @param path Výstupní buffer pro cestu AsterFS.
 * @return Výsledek směrování HTTP cíle.
 */
static enum http_target_result http_target_path(const u8 *request, u16 length, char *path) {
    const u8 *method;
    const u8 *target;
    usize method_length;
    usize target_length;

    if (http_request_line(request, length, &method, &method_length,
                          &target, &target_length) != 0
        || method_length != 3U || !bytes_equal(method, (const u8 *)"GET", 3U)) {
        return HTTP_TARGET_FORBIDDEN;
    }

    if (target_length == 0U || target[0] != '/') {
        return HTTP_TARGET_NOT_FOUND;
    }

    if (!g_http_root_is_directory) {
        if (target_length != 1U) {
            return HTTP_TARGET_NOT_FOUND;
        }
        aster_memcpy(path, g_http_root, sizeof(g_http_root));
        return HTTP_TARGET_OK;
    }

    if (target_length == 1U) {
        aster_memcpy(path, g_http_root, sizeof(g_http_root));
        return http_directory_target(path);
    }

    if (http_join_root(path, target + 1U, target_length - 1U) != 0) {
        return HTTP_TARGET_NOT_FOUND;
    }
    http_trim_trailing_slashes(path);
    if (asterfs_get_type(path) == 0) {
        return HTTP_TARGET_OK;
    }
    if (asterfs_get_type(path) == 1) {
        return http_directory_target(path);
    }
    return HTTP_TARGET_NOT_FOUND;
}

/**
 * @brief Sestaví úplnou HTTP odpověď pro přijatý požadavek.
 *
 * Obslouží vestavěnou stránku, soubor, index adresáře i odpovědi 403, 404,
 * 500 a 503.
 *
 * @param request Buffer HTTP požadavku.
 * @param request_length Délka požadavku v bajtech.
 * @return Délka sestavené odpovědi.
 */
static u16 http_build_response(const u8 *request, u16 request_length) {
    char path[ASTERFS_NAME_LEN];
    const char *content_type;
    int read_length;
    enum http_target_result target_result;
    u16 position = 0;

    if (!g_http_enabled) {
        return http_build_error("503 Service Unavailable", "503!!", "503 - Service unavailable",
                                "Tato sluzba na tomto serveru neni dostupna.",
                                request, request_length);
    }
    if (g_http_default_page) {
        return http_build_default_page(request, request_length);
    }

    target_result = http_target_path(request, request_length, path);
    if (target_result == HTTP_TARGET_FORBIDDEN) {
        return http_build_error("403 Forbidden", "403!!", "403 - Forbidden",
                                "Pristup k teto strance na tomto serveru byl zamitnut.",
                                request, request_length);
    }
    if (target_result == HTTP_TARGET_DIRECTORY) {
        return http_build_directory_listing(path, request, request_length);
    }
    if (target_result != HTTP_TARGET_OK) {
        return http_build_error("404 Not Found", "404!!", "404 - Not found",
                                "Tato stranka na tomto serveru nebyla nalezena.",
                                request, request_length);
    }

    if (asterfs_get_type(path) != 0) {
        return http_build_error("404 Not Found", "404!!", "404 - Not found",
                                "Tato stranka na tomto serveru nebyla nalezena.",
                                request, request_length);
    }

    content_type = http_content_type(path);
    position = http_append_text(g_http_response, position, "HTTP/1.1 200 OK\r\nContent-Type: ");
    position = http_append_text(g_http_response, position, content_type);
    position = http_append_text(g_http_response, position, "\r\nConnection: close\r\n\r\n");
    read_length = asterfs_read_file(path, g_http_response + position,
                                    (u16)(HTTP_RESPONSE_MAX - position));
    if (read_length < 0) {
        return http_build_error("500 Internal Server Error", "500!!", "500 - Internal server error",
                                "Pri zpracovani teto stranky na serveru doslo k chybe.",
                                request, request_length);
    }

    return (u16)(position + (u16)read_length);
}

/**
 * @brief Vypočítá standardní 16bitový jedničkový kontrolní součet.
 *
 * @param data Data zahrnutá do součtu.
 * @param length Délka dat v bajtech.
 * @param sum Počáteční součet, například z pseudo hlavičky TCP.
 * @return Doplněk výsledného 16bitového součtu.
 */
static u16 internet_checksum(const u8 *data, usize length, u32 sum) {
    usize index = 0;

    while (index + 1U < length) {
        sum += ((u32)data[index] << 8) | data[index + 1U];
        index += 2U;
    }

    if (index < length) {
        sum += (u32)data[index] << 8;
    }

    while ((sum >> 16) != 0U) {
        sum = (sum & 0xFFFFU) + (sum >> 16);
    }

    return (u16)~sum;
}

/**
 * @brief Načte data z paměti karty NE2000 přes vzdálené DMA.
 *
 * @param page Stránka paměti karty.
 * @param offset Posun v rámci stránky.
 * @param destination Cílový buffer.
 * @param length Počet čtených bajtů.
 */
static void ne2k_dma_read(u8 page, u8 offset, u8 *destination, u16 length) {
    u16 index;

    outb(NE2K_BASE + NE2K_RBCR0, (u8)length);
    outb(NE2K_BASE + NE2K_RBCR1, (u8)(length >> 8));
    outb(NE2K_BASE + NE2K_RSAR0, offset);
    outb(NE2K_BASE + NE2K_RSAR1, page);
    outb(NE2K_BASE + NE2K_CMD, NE2K_CR_STA | NE2K_CR_RD0);

    for (index = 0; index < length; index += 2U) {
        u16 value = inw(NE2K_BASE + NE2K_DATA);
        destination[index] = (u8)value;
        if (index + 1U < length) {
            destination[index + 1U] = (u8)(value >> 8);
        }
    }

    outb(NE2K_BASE + NE2K_CMD, NE2K_CR_STA | NE2K_CR_RD2);
}

/**
 * @brief Zapíše data do paměti karty NE2000 přes vzdálené DMA.
 *
 * @param page Stránka paměti karty.
 * @param offset Posun v rámci stránky.
 * @param source Zdrojový buffer.
 * @param length Počet zapisovaných bajtů.
 */
static void ne2k_dma_write(u8 page, u8 offset, const u8 *source, u16 length) {
    u16 index;

    outb(NE2K_BASE + NE2K_RBCR0, (u8)length);
    outb(NE2K_BASE + NE2K_RBCR1, (u8)(length >> 8));
    outb(NE2K_BASE + NE2K_RSAR0, offset);
    outb(NE2K_BASE + NE2K_RSAR1, page);
    outb(NE2K_BASE + NE2K_CMD, NE2K_CR_STA | NE2K_CR_RD1);

    for (index = 0; index < length; index += 2U) {
        u16 value = source[index];
        if (index + 1U < length) {
            value |= (u16)source[index + 1U] << 8;
        }
        outw(NE2K_BASE + NE2K_DATA, value);
    }

    outb(NE2K_BASE + NE2K_CMD, NE2K_CR_STA | NE2K_CR_RD2);
}

/**
 * @brief Odešle připravený ethernetový rámec z vysílacího bufferu.
 *
 * Doplňuje rámec na minimální ethernetovou délku 60 bajtů.
 *
 * @param length Délka připraveného rámce bez FCS.
 */
static void ne2k_transmit(u16 length) {
    if (length < 60U) {
        aster_memset(g_tx_frame + length, 0, 60U - length);
        length = 60U;
    }

    ne2k_dma_write(NE2K_TX_START, 0, g_tx_frame, length);
    outb(NE2K_BASE + NE2K_TPSR, NE2K_TX_START);
    outb(NE2K_BASE + NE2K_TBCR0, (u8)length);
    outb(NE2K_BASE + NE2K_TBCR1, (u8)(length >> 8));
    outb(NE2K_BASE + NE2K_ISR, 0xFFU);
    outb(NE2K_BASE + NE2K_CMD, NE2K_CR_STA | NE2K_CR_TXP | NE2K_CR_RD2);
}

/**
 * @brief Sestaví a odešle ARP odpověď na požadavek pro guest IP.
 *
 * @param request Přijatý ethernetový ARP rámec.
 */
static void send_arp_reply(const u8 *request) {
    u8 *arp = g_tx_frame + ETHER_HEADER_LEN;

    aster_memcpy(g_tx_frame, request + 6U, 6U);
    aster_memcpy(g_tx_frame + 6U, g_mac, 6U);
    g_tx_frame[12] = 0x08U;
    g_tx_frame[13] = 0x06U;

    aster_memcpy(arp, request + ETHER_HEADER_LEN, 8U);
    arp[6] = 0U;
    arp[7] = 2U;
    aster_memcpy(arp + 8U, g_mac, 6U);
    aster_memcpy(arp + 14U, g_guest_ip, 4U);
    aster_memcpy(arp + 18U, request + ETHER_HEADER_LEN + 8U, 6U);
    aster_memcpy(arp + 24U, request + ETHER_HEADER_LEN + 14U, 4U);

    ne2k_transmit(ETHER_HEADER_LEN + 28U);
}

/**
 * @brief Sestaví a odešle TCP segment aktivnímu HTTP klientovi.
 *
 * Vytváří ethernetovou, IPv4 a TCP hlavičku včetně odpovídajících
 * kontrolních součtů.
 *
 * @param flags Kombinace příznaků TCP.
 * @param payload Volitelná data aplikační vrstvy.
 * @param payload_length Délka dat v bajtech.
 */
static void send_tcp(u8 flags, const u8 *payload, u16 payload_length) {
    u8 *ip = g_tx_frame + ETHER_HEADER_LEN;
    u8 *tcp = ip + IPV4_HEADER_LEN;
    u16 tcp_length = (u16)(TCP_HEADER_LEN + payload_length);
    u16 ip_length = (u16)(IPV4_HEADER_LEN + tcp_length);
    u32 tcp_sum = 0;

    aster_memcpy(g_tx_frame, g_client_mac, 6U);
    aster_memcpy(g_tx_frame + 6U, g_mac, 6U);
    g_tx_frame[12] = 0x08U;
    g_tx_frame[13] = 0x00U;

    aster_memset(ip, 0, IPV4_HEADER_LEN);
    ip[0] = 0x45U;
    write_be16(ip + 2U, ip_length);
    write_be16(ip + 4U, g_ip_identification++);
    ip[8] = 64U;
    ip[9] = 6U;
    aster_memcpy(ip + 12U, g_guest_ip, 4U);
    aster_memcpy(ip + 16U, g_client_ip, 4U);
    write_be16(ip + 10U, internet_checksum(ip, IPV4_HEADER_LEN, 0));

    aster_memset(tcp, 0, TCP_HEADER_LEN);
    write_be16(tcp, g_http_port);
    write_be16(tcp + 2U, g_client_port);
    write_be32(tcp + 4U, g_server_next_seq);
    write_be32(tcp + 8U, g_client_next_seq);
    tcp[12] = 0x50U;
    tcp[13] = flags;
    write_be16(tcp + 14U, 0x4000U);
    if (payload_length != 0U) {
        aster_memcpy(tcp + TCP_HEADER_LEN, payload, payload_length);
    }

    tcp_sum += ((u32)g_guest_ip[0] << 8) | g_guest_ip[1];
    tcp_sum += ((u32)g_guest_ip[2] << 8) | g_guest_ip[3];
    tcp_sum += ((u32)g_client_ip[0] << 8) | g_client_ip[1];
    tcp_sum += ((u32)g_client_ip[2] << 8) | g_client_ip[3];
    tcp_sum += 6U;
    tcp_sum += tcp_length;
    write_be16(tcp + 16U, internet_checksum(tcp, tcp_length, tcp_sum));

    ne2k_transmit((u16)(ETHER_HEADER_LEN + ip_length));
}

/**
 * @brief Zpracuje přijatý ARP požadavek.
 *
 * @param frame Přijatý ethernetový rámec.
 * @param length Délka rámce v bajtech.
 */
static void handle_arp(const u8 *frame, u16 length) {
    const u8 *arp = frame + ETHER_HEADER_LEN;

    if (length < ETHER_HEADER_LEN + 28U
        || read_be16(arp) != 1U
        || read_be16(arp + 2U) != 0x0800U
        || arp[4] != 6U
        || arp[5] != 4U
        || read_be16(arp + 6U) != 1U
        || !bytes_equal(arp + 24U, g_guest_ip, 4U)) {
        return;
    }

    send_arp_reply(frame);
}

/**
 * @brief Zpracuje IPv4/TCP rámec v jednoduchém HTTP stavovém automatu.
 *
 * Obsluhuje navázání spojení, přijetí HTTP požadavku, odeslání odpovědi
 * s FIN a uzavření klientského spojení.
 *
 * @param frame Přijatý ethernetový rámec.
 * @param frame_length Délka rámce v bajtech.
 */
static void handle_tcp(const u8 *frame, u16 frame_length) {
    const u8 *ip = frame + ETHER_HEADER_LEN;
    const u8 *tcp;
    u16 ip_length;
    u16 tcp_length;
    u16 destination_port;
    u16 source_port;
    u16 payload_length;
    u8 ip_header_length;
    u8 tcp_header_length;
    u8 flags;
    u32 sequence;
    u32 acknowledgement;

    if (frame_length < ETHER_HEADER_LEN + IPV4_HEADER_LEN
        || (ip[0] >> 4) != 4U
        || ip[9] != 6U
        || !bytes_equal(ip + 16U, g_guest_ip, 4U)) {
        return;
    }

    ip_header_length = (u8)((ip[0] & 0x0FU) * 4U);
    ip_length = read_be16(ip + 2U);
    if (ip_header_length < IPV4_HEADER_LEN
        || ip_length < (u16)(ip_header_length + TCP_HEADER_LEN)
        || frame_length < (u16)(ETHER_HEADER_LEN + ip_length)) {
        return;
    }

    tcp = ip + ip_header_length;
    tcp_header_length = (u8)((tcp[12] >> 4) * 4U);
    if (tcp_header_length < TCP_HEADER_LEN || ip_length < (u16)(ip_header_length + tcp_header_length)) {
        return;
    }

    source_port = read_be16(tcp);
    destination_port = read_be16(tcp + 2U);
    if (destination_port != g_http_port) {
        return;
    }

    tcp_length = (u16)(ip_length - ip_header_length);
    payload_length = (u16)(tcp_length - tcp_header_length);
    sequence = read_be32(tcp + 4U);
    acknowledgement = read_be32(tcp + 8U);
    flags = tcp[13];

    if ((flags & TCP_SYN) != 0U && (flags & TCP_ACK) == 0U) {
        aster_memcpy(g_client_mac, frame + 6U, 6U);
        aster_memcpy(g_client_ip, ip + 12U, 4U);
        g_client_port = source_port;
        g_client_next_seq = sequence + 1U;
        g_server_next_seq = 0x41535445U;
        g_connection = CONNECTION_SYN_RECEIVED;
        send_tcp(TCP_SYN | TCP_ACK, 0, 0);
        ++g_server_next_seq;
        return;
    }

    if (g_connection == CONNECTION_CLOSED
        || source_port != g_client_port
        || !bytes_equal(frame + 6U, g_client_mac, 6U)
        || !bytes_equal(ip + 12U, g_client_ip, 4U)) {
        return;
    }

    if ((flags & TCP_RST) != 0U) {
        g_connection = CONNECTION_CLOSED;
        return;
    }

    if (g_connection == CONNECTION_SYN_RECEIVED
        && (flags & TCP_ACK) != 0U
        && acknowledgement == g_server_next_seq) {
        g_connection = CONNECTION_ESTABLISHED;
    }

    if (payload_length != 0U && g_connection == CONNECTION_ESTABLISHED
        && sequence == g_client_next_seq) {
        u16 response_length;

        g_client_next_seq += payload_length;
        response_length = http_build_response(tcp + tcp_header_length, payload_length);
        send_tcp(TCP_ACK | TCP_PSH | TCP_FIN, g_http_response, response_length);
        g_server_next_seq += (u32)response_length + 1U;
        g_connection = CONNECTION_FIN_SENT;
        return;
    }

    if ((flags & TCP_FIN) != 0U && sequence == g_client_next_seq) {
        ++g_client_next_seq;
        send_tcp(TCP_ACK, 0, 0);
        g_connection = CONNECTION_CLOSED;
    }
}

/**
 * @brief Rozešle ethernetový rámec obsluze ARP nebo IPv4.
 *
 * @param frame Přijatý ethernetový rámec.
 * @param length Délka rámce v bajtech.
 */
static void handle_frame(const u8 *frame, u16 length) {
    u16 ether_type;

    if (length < ETHER_HEADER_LEN) {
        return;
    }

    ether_type = read_be16(frame + 12U);
    if (ether_type == 0x0806U) {
        handle_arp(frame, length);
    } else if (ether_type == 0x0800U) {
        handle_tcp(frame, length);
    }
}

/**
 * @brief Přečte a zpracuje jeden rámec z kruhového bufferu NE2000.
 *
 * @return 1, pokud byl rámec zpracován; jinak 0.
 */
static int ne2k_receive_one(void) {
    u8 current;
    u8 packet_page;
    u8 next_page;
    u8 header[4];
    u16 packet_length;
    u16 frame_length;

    outb(NE2K_BASE + NE2K_CMD, NE2K_CR_STA | NE2K_CR_RD2 | NE2K_CR_PS0);
    current = inb(NE2K_BASE + NE2K_CURR);
    outb(NE2K_BASE + NE2K_CMD, NE2K_CR_STA | NE2K_CR_RD2);

    packet_page = (u8)(inb(NE2K_BASE + NE2K_BNRY) + 1U);
    if (packet_page == NE2K_RX_STOP) {
        packet_page = NE2K_RX_START;
    }

    if (packet_page == current) {
        return 0;
    }

    ne2k_dma_read(packet_page, 0, header, sizeof(header));
    next_page = header[1];
    packet_length = read_le16(header + 2U);
    if (next_page < NE2K_RX_START || next_page >= NE2K_RX_STOP
        || packet_length < 4U || packet_length > FRAME_MAX + 4U) {
        outb(NE2K_BASE + NE2K_BNRY, (u8)(current == NE2K_RX_START ? NE2K_RX_STOP - 1U : current - 1U));
        return 0;
    }

    frame_length = (u16)(packet_length - 4U);
    ne2k_dma_read(packet_page, 4U, g_rx_frame, frame_length);
    outb(NE2K_BASE + NE2K_BNRY, (u8)(next_page == NE2K_RX_START ? NE2K_RX_STOP - 1U : next_page - 1U));
    handle_frame(g_rx_frame, frame_length);
    return 1;
}

/**
 * @brief Inicializuje emulovanou ISA kartu NE2000.
 *
 * @return 1 při úspěšném načtení platné MAC adresy; jinak 0.
 */
int network_init(void) {
    usize index;
    u8 mac_prom[12];
    int mac_is_all_zero = 1;
    int mac_is_all_ff = 1;

    outb(NE2K_BASE + NE2K_RESET, inb(NE2K_BASE + NE2K_RESET));
    outb(NE2K_BASE + NE2K_CMD, NE2K_CR_STP | NE2K_CR_RD2);
    outb(NE2K_BASE + NE2K_DCR, 0x49U);
    outb(NE2K_BASE + NE2K_RBCR0, 0U);
    outb(NE2K_BASE + NE2K_RBCR1, 0U);
    outb(NE2K_BASE + NE2K_RCR, 0x20U);
    outb(NE2K_BASE + NE2K_TCR, 0x02U);
    outb(NE2K_BASE + NE2K_TPSR, NE2K_TX_START);
    outb(NE2K_BASE + NE2K_PSTART, NE2K_RX_START);
    outb(NE2K_BASE + NE2K_PSTOP, NE2K_RX_STOP);
    outb(NE2K_BASE + NE2K_BNRY, NE2K_RX_START);
    outb(NE2K_BASE + NE2K_ISR, 0xFFU);

    /* In 16-bit mode, every byte in the NE2000 PROM occupies one word. */
    ne2k_dma_read(0U, 0U, mac_prom, sizeof(mac_prom));

    outb(NE2K_BASE + NE2K_CMD, NE2K_CR_STP | NE2K_CR_RD2 | NE2K_CR_PS0);
    for (index = 0; index < sizeof(g_mac); ++index) {
        g_mac[index] = mac_prom[index * 2U];
        if (g_mac[index] != 0U) {
            mac_is_all_zero = 0;
        }
        if (g_mac[index] != 0xFFU) {
            mac_is_all_ff = 0;
        }
        outb(NE2K_BASE + NE2K_PAR0 + (u16)index, g_mac[index]);
    }
    outb(NE2K_BASE + NE2K_CURR, NE2K_RX_START + 1U);

    outb(NE2K_BASE + NE2K_CMD, NE2K_CR_STA | NE2K_CR_RD2);
    outb(NE2K_BASE + NE2K_RCR, 0x04U);
    outb(NE2K_BASE + NE2K_TCR, 0U);
    outb(NE2K_BASE + NE2K_IMR, 0U);
    outb(NE2K_BASE + NE2K_ISR, 0xFFU);

    g_connection = CONNECTION_CLOSED;
    g_http_enabled = 0;
    g_http_default_page = 0;
    g_http_root[0] = '\0';
    g_http_port = HTTP_DEFAULT_PORT;
    g_ready = !mac_is_all_zero && !mac_is_all_ff;
    return g_ready;
}

/**
 * @brief Zpracuje omezený počet přijatých ethernetových rámců.
 *
 * Funkce je volána pravidelně z obsluhy časovače.
 */
void network_poll(void) {
    unsigned int packets = 0;

    if (!g_ready) {
        return;
    }

    while (packets < 4U && ne2k_receive_one()) {
        ++packets;
    }
}

/**
 * @brief Ověří připravenost síťové karty.
 *
 * @return 1, pokud je karta inicializovaná; jinak 0.
 */
int network_is_ready(void) {
    return g_ready;
}

/**
 * @brief Spustí HTTP server pro soubor nebo adresář AsterFS.
 *
 * @param path Absolutní cesta k hostovanému souboru nebo adresáři.
 * @param port TCP port serveru.
 * @return 0 při úspěchu; -1 při neplatném vstupu nebo nepřipravené síti.
 */
int network_http_serve(const char *path, u16 port) {
    usize index = 0;
    int type;

    if (!g_ready || !path || port == 0U) {
        return -1;
    }

    type = asterfs_get_type(path);
    if (type < 0) {
        return -1;
    }

    g_http_enabled = 0;
    while (path[index] && index + 1U < sizeof(g_http_root)) {
        g_http_root[index] = path[index];
        ++index;
    }
    if (path[index] != '\0') {
        return -1;
    }

    g_http_root[index] = '\0';
    g_http_root_is_directory = type == 1;
    g_http_default_page = 0;
    g_http_port = port;
    g_connection = CONNECTION_CLOSED;
    g_http_enabled = 1;
    return 0;
}

/**
 * @brief Spustí HTTP server s vestavěnou domovskou stránkou.
 *
 * @param port TCP port serveru.
 * @return 0 při úspěchu; -1 při nepřipravené síti nebo nulovém portu.
 */
int network_http_serve_default(u16 port) {
    if (!g_ready || port == 0U) {
        return -1;
    }

    g_http_enabled = 0;
    g_http_root[0] = '\0';
    g_http_root_is_directory = 0;
    g_http_default_page = 1;
    g_http_port = port;
    g_connection = CONNECTION_CLOSED;
    g_http_enabled = 1;
    return 0;
}

/**
 * @brief Zastaví HTTP server a vymaže stav aktivního spojení.
 */
void network_http_stop(void) {
    g_http_enabled = 0;
    g_http_root[0] = '\0';
    g_http_root_is_directory = 0;
    g_http_default_page = 0;
    g_http_port = HTTP_DEFAULT_PORT;
    g_connection = CONNECTION_CLOSED;
}