# aster-core

Aktuální verze (`v0.13`)

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

### Shell a vestavěné nástroje

* Interaktivní shell s parsováním příkazů, aliasy a readline
* Práce se soubory a složkami – ls, cd, makdir, remdir, copdir, movdir, makfile, remfile, copfile, movfile, cat, write
* Textový editor (edit) – šipky, Ctrl+S uložení, ESC konec
* File manager (fm) – procházení adresářů, náhled, editace, mazání
* Správa uživatelů – useradd, passwdch, přihlašovací obrazovka, auto-login
* Podpora sysapps (aplikace linkované do kernelu)
* Spouštění jednoduchých C-like skriptů (./script)
* Pomocné aliasy (.aliases soubor)

### Instalace na disk

Po spuštění je možné systém nainstalovat příkazem install – vytvoří adresářovou strukturu (/etc, /home, /bin, /var, /tmp), založí uživatele a připraví konfigurační soubory.

## Plány

* Podpora sítě (network stack, ovladače síťových karet)
* Desktopové prostředí (grafický výstup, okenní systém)
* Další sysapps a uživatelské nástroje
* Stabilizace souborového systému a podpora větších disků
