# aster-core

Aktuální verze (`v0.11`)

Projekt vznikl jako pokus napsat vlastní operační systém od nuly. Podařilo se vytvořit základ kernelu a připravit strukturu projektu, ale vývoj jsem nakonec zastavil. Je dost nepravděpodobné, že se k němu ještě někdy vrátím.

## Co obsahuje

* 64bit x86_64 kernel
* Základ bootování
* VGA textový výstup
* Připravenou strukturu projektu
* Základy pro další vývoj

## Struktura projektu

```text
apps/           Budoucí aplikace
arch/x86_64/    Kód specifický pro architekturu
boot/           Bootovací kód
drivers/        Ovladače
include/        Hlavičkové soubory
kernel/         Zdrojový kód kernelu
apps/            Aplikace jadra systému
sysapps/         Aplikace nezávislé na jádru
```

## Sestavení

Projekt se sestaví a spustí pomocí připraveného skriptu:

```bash
./setup.sh
```

Skript provede:

* vyčištění předchozího buildu (`make clean`)
* sestavení kernelu (`make`)
* spuštění v QEMU (`make run`)

Pokud používáš vlastní cross-compiler, můžeš nastavit proměnnou `CROSS`:

```bash
CROSS=x86_64-elf- ./setup.sh
```
