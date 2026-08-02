# aster-core

Aktualni verze je definovana jako `ASTER_CORE_VERSION_TAG` v [include/aster_version.h](include/aster_version.h).

Projekt vznikl jako pokus napsat vlastní operační systém od nuly. Podařilo se vytvořit funkční kernel s bootováním, správou paměti, procesy, souborovým systémem, shellem a základními nástroji.

## Co obsahuje

* 64bit x86_64 kernel vlastní architektury
* Bootování z vlastního stage2 bootloaderu (FAT-like struktura na disketovém obraze)
* VGA textový výstup (80×25 znaků, barvy)
* Správa paměti – stránkování, alokace bloků (kmalloc)
* Procesy a plánovač (round-robin s prioritami)
* Syscall vrstva (aster_api)
* Přerušení (IDT, IRQ, klávesnice, timer)
* Klávesnicový ovladač (čtení stisků, keyboard_readline)
* Sériový ovladač (základní výstup)
* Timer (programovatelná frekvence, čekání timer_sleep_ms)
* ATA PIO storage driver s AsterFS (vlastní souborový systém)
* Bootlog – jednotné stavové výpisy při bootu s volitelným splash screenem
* Rozdělení kódu do samostatných modulů (boot, auth, shell, editor, fm, fs_utils, statusbar, io_ports)
* Implementaci RING 3 pro budoucí přepsání aplikací pro běh na RING 3

### Shell a vestavěné nástroje

* Interaktivní shell s parsováním příkazů a readline
* Práce se soubory a složkami – ls, cd, makdir, remdir, copdir, movdir, makfile, remfile, copfile, movfile, cat, write
* Textový editor (edit) – šipky, Ctrl+S uložení, ESC konec
* File manager (fm) – procházení adresářů, náhled, editace, mazání
* Správa uživatelů – useradd, passwdch, přihlašovací obrazovka, auto-login
* Podpora sysapps (aplikace linkované do kernelu)
* Spouštění jednoduchých C-like skriptů (./script)

### Instalace na disk

Po spuštění je možné systém nainstalovat příkazem install – vytvoří adresářovou strukturu (/etc, /home, /bin, /var, /tmp), založí uživatele a připraví konfigurační soubory.

## Sit a HTTP server

`make run` spousti QEMU s user-mode NATem, emulovanou ISA kartou NE2000 a
presmerovanim `127.0.0.1:8080` hosta na port `8080` guestu. Kernel ma pro tuto
konfiguraci statickou adresu `10.0.2.15` a poskytuje jednoduchy HTTP server.

Ve Windows spust build a QEMU pres Ubuntu WSL:

```sh
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/Users/pavelkalas/Desktop/aster-core && make'
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/Users/pavelkalas/Desktop/aster-core && make run'
```

Po nabootovani nastav obsah serveru v shellu. Port musi byt stejny jako
`HTTP_PORT`, se kterym QEMU spustis:

```text
httpserve wwwroot 8080
httpserve index.htm 8080
httpserve ... 80
```

Adresar poskytuje `home.htm` na URL `/`; soubor se take poskytuje pouze na URL
`/`. Stejne funguje kazda podadresarova URL, napr. `/test/` hleda
`test/home.htm`. Pokud v adresari `home.htm` chybi, zobrazi se jeho obsah
s odkazy na soubory, napr. `/test/index.htm`. Soubor `.block-content-view`
v danem adresari tento vypis zablokuje odpovedi `403 Forbidden`. Neexistujici
cesta vrati `404 Not Found`. Pak otevri v prohlizeci na hostu
`http://localhost:8080`. Jiny port lze zvolit napr. `make run HTTP_PORT=8081`,
pak v guestu spustit `httpserve wwwroot 8081` a otevrit
`http://localhost:8081`.

Specialni zdroj `...` nezavisi na souborovem systemu a zobrazi vestavenou
domovskou stranku serveru. Napriklad po `httpserve ... 80` otevri
`http://localhost` (QEMU musi byt spusteno s `HTTP_PORT=80`).

Aktualni AsterFS omezuje jeden hostovany soubor na 512 B.

Tato prvni verze implementuje ARP, IPv4 a TCP pouze v rozsahu potrebnem pro
vestaveny HTTP server. QEMU NAT je pripraveny pro pristup guestu ven, ale
kernel zatim nema DHCP, DNS ani obecne TCP klienty.

## Plány

* Rozšíření síťového stacku o DHCP, DNS a klientské protokoly
* Desktopové prostředí (grafický výstup, okenní systém)
* Další sysapps a uživatelské nástroje
* Stabilizace souborového systému a podpora větších disků
