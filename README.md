# ICP Project – Nástroj pro vizuální editaci a běh interpretovaných konečných automatů

- **Autoři:** Martin Ševčík (xsevcim00), Jakub Lůčný (xlucnyj00)

- **Instituce:** VUT FIT 2025
 
## Přehled projektu

Tento nástroj kombinuje editor stavových diagramů (FSM Editor) a živý monitor běžícího interpretu.

| Oblast                | Funkcionalita                                                                                                                                                       |
| --------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Modelování**        | interaktivní přidávání stavů, přechodů, proměnných, vstupů a výstupů; okamžité ověření sémantických chyb; vizuální layout se samodoplňováním pozic                  |
| **Akce & Stráže**     | zápis výrazů v podmnožině JavaScriptu (díky `QJSEngine`) – k dispozici jsou helpery `defined()`, `valueof()`, `atoi()` a `elapsed()`                                |
| **Zpožděné přechody** | pevné (`@ 5000`) i proměnné (`@ timeout`) zpoždění; plánování zajišťuje interní `Scheduler`                                                                         |
| **Generování & běh**  | jedním tlačítkem *Build & Run* se vygeneruje JSON, spustí CLI interpret (`fsm_runtime`) a GUI se k němu připojí přes UDP                                            |
| **Live monitoring**   | zvýraznění aktuálního stavu, tabulky posledních hodnot vstupů/ proměnných/ výstupů, log změn, možnost asynchronně „injektovat“ nové vstupy nebo nastavovat proměnné |



## Architektura

### Class diagram

![class](/diagrams/class.png)


* **GUI vrstva** (adresář `ui_qt/`):

  * **`MainWindow`** centralizuje editaci, vizualizaci i monitorování.
  * Vykreslování diagramů zajišťují `StateItem` a `TransitionItem` (soubor `graphics/fsmgraphicsitems.cpp`).

* **Core** (`core_fsm/`):

  * **Domain objekty** `State`, `Transition`, `Variable`, `Automaton`.
  * **`script_engine.cpp`** — injektuje C++ kontext do `QJSEngine`, provádí akce a stráže.
  * **`Scheduler`** (uvnitř `automaton.cpp`) plánuje zpožděné přechody.

* **Persistence** (`persistence_bridge.cpp`) převádí JSON ↔ in‑memory DTO (`FsmDocument`).

* **IO Bridge** (`io/udp_channel.cpp`) — net‐agnostická obálka nad BSD sockety; nepřerušuje GUI vlákno.

### Core funkcionalita

![core](/diagrams/core.png)

- Stavový diagram samotného interpretu (fronta vstupů → výběr přechodu → on‑enter akce → plánování timerů → čekání)

### GUI funkcionalita

![gui](/diagrams/gui.png)
- Vysvětluje režimy Editor ⇆ Monitor, signal‑slot propojení a životní cyklus Build & Run

### App GUI (Project tree editing)
![app](/diagrams/app.png)

### App GUI (Monitoring)
![app2](/diagrams/app2.png)


### 3.  Komunikace GUI ↔ Interpret

* Protokol je **čistý JSON přes UDP** (localhost, port 45454 ↔ 45455).
* Interpreter pravidelně vysílá paket typu `"state"`:

```jsonc
{
  "type":   "<string>",        // typ zprávy, např. "state"
  "seq":    "<integer>",         // pořadové číslo, inkrement od 0/1
  "ts":     "<integer>",         // timestamp v ms od 1.1.1970 UTC
  "state":  "<string>",        // akt. stav automatu
  "inputs": {  },             // mapování vstupů → hodnoty
  "vars":   {  },             // mapování proměnných → hodnoty
  "outputs":{  }              // mapování výstupů → hodnoty
}
```

---

## Formát souboru *.fsm.json*

- Příklad automatu
```jsonc
{
  "name": "TOF",
  "inputs":  ["in","set_to","req_rt"],
  "outputs": ["out","rt"],
  "variables": [
    { "name":"timeout", "type":"int", "init":5000 }
  ],
  "states": [
    {
      "id": "IDLE",
      "initial": true,
      "onEnter": "output(\"out\",0); output(\"rt\",0);"
    },

  ],
  "transitions": [
    {
      "from":    "IDLE",
      "to":      "ACTIVE",
      "trigger": "in",
      "guard":   "atoi(valueof(\"in\")) == 1"
    },
    {
      "from":     "TIMING",
      "to":       "IDLE",
      "delay_ms": "timeout"    // proměnné zpoždění
    }
  ]
}

```

Validator v `persistence_bridge.cpp` kontroluje, zda `trigger` existuje mezi vstupy, jestli stráž odkazuje pouze na deklarované symboly apod. Při nesrovnalosti zobrazí GUI **žlutý warning bar**.

---

## Přeložení a běh

1. **Závislosti**

   * **Qt ≥ 5.15** (Widgets + QJS)
   * C++17 kompilátor (GCC‑11 / Clang‑14)
   * `nlohmann/json` (vložený jako single‑header)

2. **Build**

```bash
$ make            # překlad GUI i CLI interpretu
$ make doxygen    # volitelné – HTML dokumentace do ./doc/
```

3. **Spuštění**

```bash
$ make run        # ekvivalent otevření ui_qt/bin/icp_fsm_editor
```

*V GUI* klikni **New →** nakresli automat → **Build & Run**.  Editor uloží temporární JSON, spustí `fsm_runtime`, otevře UDP kanál a začne vizualizovat live stav.


