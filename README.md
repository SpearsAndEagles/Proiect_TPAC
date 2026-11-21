# Simulare OMNeT++: Planificator Radio cu Weighted Round Robin (RadioSch2)

Acest proiect conține un model de simulare a evenimentelor discrete dezvoltat în **OMNeT++**, ce reproduce comportamentul mecanismului de planificare a resurselor radio (Radio Resource Scheduling) într-o rețea celulară wireless (ex: 4G LTE / 5G).

---

## 1. Arhitectura Sistemului

Rețeaua principală se numește `RadioSch2` și conține următoarele componente:

```
                  ┌──────────────────────┐
                  │      Scheduler       │
                  └──────┬────────▲──────┘
        txScheduling[i]  │        │  rxInfo[i] (ql_info)
                         ▼        │
                  ┌──────────────────────┐
                  │      User [i]        │
                  │                      │
                  │  [Source] ──> [MyQ]  │
                  └───────────────┬──────┘
                   txPackets      │
                                  ▼
                  ┌──────────────────────┐
                  │         Sink         │
                  └──────────────────────┘
```

### Componente și Module
1. **`User` (Compus)**: Reprezintă un dispozitiv mobil utilizator (`user[0]`, `user[1]`, `user[2]`). Fiecare utilizator integrează intern două submodule:
   - **`Source` (Generatorul de trafic)**: Simulează aplicațiile care creează pachete de date.
   - **`MyQ` (Coada / Tamponul)**: Stochează pachetele în așteptarea aprobării transmisiei.
2. **`Scheduler` (Planificatorul / Stația de bază)**: "Creierul" rețelei. Rulează la intervale deterministe fixe (1 ms) și împarte cele 10 canale radio disponibile între utilizatori pe baza algoritmului **Weighted Round Robin (WRR) Oportunist**.
3. **`Sink` (Colectorul / Serverul de destinație)**: Recepționează pachetele sosite, calculează durata lor de viață (latența cap-coadă), emite semnale statistice și eliberează memoria.

---

## 2. Concepte Fundamentale

* **Utilizatori (`usersCount = 3`)**: Terminale mobile concurente pentru aceleași resurse radio.
* **Pachete (`job`)**: Unități de date generate cu `PacketLength = 1`.
* **Blocuri Radio (`nrBlocks`) și Canale (`NrOfChannels = 10`)**: 
  - La fiecare milisecundă (ciclul TTI), antena are capacitatea maximă de a transmite **10 blocuri** (1 bloc = 1 pachet).
* **Încărcarea Rețelei (Network Load - NL)**: Raportul dintre volumul de date generat și capacitatea de transfer a rețelei.

---

## 3. Formule Teoretice

Intervalul mediu de generare a pachetelor (`sendingTime`) este calculat dinamic în C++ pentru a atinge încărcarea dorită a rețelei:

$$\text{NL (Network Load)} = \frac{\text{Date Generate}}{\text{Rată de Transfer}}$$

$$\text{Date Generate} = \frac{\text{nrUsers} \times \text{nrPackets} \times \text{PacketLength}}{\text{sendingTime}}$$

$$\text{Rată de Transfer} = \frac{\text{NrOfChannels}}{\text{schedulingCycle}}$$

$$\implies \mathbf{sendingTime} = \frac{\text{nrUsers} \times \text{nrPackets} \times \text{PacketLength} \times \text{schedulingCycle}}{\text{NL} \times \text{NrOfChannels}}$$

*Exemplu configurat:*
* $\text{nrUsers} = 3$
* $\text{NrOfChannels} = 10$
* $\text{schedulingCycle} = 1\text{ ms} = 0.001\text{ s}$
* $\text{nrPackets} = 1$, $\text{PacketLength} = 1$
* $\text{NL} = 0.8$ (80% încărcare)
$$\implies \mathbf{sendingTime} = \frac{3 \times 1 \times 1 \times 0.001}{0.8 \times 10} = \mathbf{0.375\text{ ms} = 0.000375\text{ s}}$$

---

## 4. Detalii de Implementare pe Etape

### Pasul 1: Coada (`MyQ` — `myQ.cc`, `myQ.h`, `myQ.ned`)
- **Extragere controlată a pachetelor**: La primirea comenzii de la Scheduler (`rxScheduling`), citește parametrul `nrBlocks` și extrage din coadă cel mult numărul aprobat de pachete.
- **Protecție la depanare**: Folosește condiția `!queue.isEmpty()` pentru a preveni extragerea din coadă vidă.
- **Feedback către Scheduler**: Imediat după modificarea cozii, transmite lungimea curentă `ql` prin mesajul `qInfo` (parametrul `ql_info`) pe poarta `txInfo`.
- **Gestiunea memoriei**: Curățarea cozii la distrugerea modulului (`queue.clear()`).

### Pasul 2: Sursa (`Source` — `Source.cc`, `Source.h`, `Source.ned`)
- **Calcul intern `sendingTime`**: Nu depinde de setările rigide din `omnetpp.ini`; se calculează automat la `initialize()` conform formulei de Network Load.
- **Proces Poisson (Distribuție Exponențială)**: Refolosește același self-message (`sendMessageEvent`) programat la `simTime() + exponential(sendingTime)`.
- **Oprire programatică (`endSimulation`)**: Simularea se oprește automat în momentul în care `simTime() >= maxSimTime` (implicit $1.0\text{ s} = 1000\text{ ms}$).
- **Metoda `finish()`**: Centralizează numărul de pachete generate de fiecare utilizator și salvează datele în `results_source.txt`.

### Pasul 3: Planificatorul (`Scheduler` — `Scheduler.cc`, `Scheduler.h`, `Scheduler.ned`)
- **Ciclu periodic determinist**: Trezire la fiecare $1\text{ ms}$ fix (`schedulingPeriod`) prin mesajul `selfMsg`.
- **Sistem de Priorități / Ponderi (`userWeights`)**:
  - `User[0]`: pondere 3 (prioritate mare / Gold)
  - `User[1]`: pondere 2 (prioritate medie / Silver)
  - `User[2]`: pondere 1 (prioritate de bază / Bronze)
- **Algoritmul Weighted Round Robin (WRR) Oportunist**:
  - Distribuie cele 10 canale în runde succesive proporțional cu ponderea fiecărui utilizator.
  - Alocarea este limitată strict la cererea reală a cozii (`q[i] - NrBlocks[i]`).
  - **Caracter oportunist**: Dacă un utilizator prioritar nu are suficiente pachete în coadă, blocurile rămase neutilizate sunt realocate instantaneu celorlalți utilizatori, maximizând transferul util fără risipă de spectru.
  - Indexul de start (`currentRRIndex`) se rotește circular la fiecare ciclu pentru a garanta echitate în timp.
- **Metoda `finish()`**: Înregistrează ciclurile executate și totalul de blocuri alocate fiecărui utilizator în `results_scheduler.txt`.

### Modulul Destinație (`Sink` — `Sink.cc`, `Sink.h`, `Sink.ned`)
- **Calcul Durată de Viață (`lifetime`)**: 
  $$\text{lifetime} = \text{simTime()} - \text{msg->getCreationTime()}$$
- **Statistici OMNeT++**: Emite semnalul `lifetimeSignal` asociat statisticii `@statistic[lifetime]`.
- **Prevenire Memory Leaks & Delays**: Șterge fiecare mesaj imediat (`delete msg;`).
- **Metoda `finish()`**: Salvează pachetele recepționate de la fiecare utilizator în `results_sink.txt`.

---

## 5. Structura Proiectului

```
.
├── RadioSch.ned         # Rețeaua principală (RadioSch2)
├── user.ned             # Modulul compus User (Source + MyQ)
├── Source.ned           # Interfața generatorului de pachete
├── Source.h / .cc       # Implementarea generatorului (Poisson, endSimulation, finish)
├── myQ.ned              # Interfața cozii de așteptare
├── myQ.h / .cc          # Implementarea cozii (extragere nrBlocks, raportare ql_info)
├── Scheduler.ned        # Interfața planificatorului
├── Scheduler.h / .cc    # Implementarea WRR oportunist cu ponderi
├── Sink.ned             # Interfața colectorului
├── Sink.h / .cc         # Calcul lifetime, semnale statistice și eliberare memorie
├── omnetpp.ini          # Configurația de rulare (Sched1)
├── .gitignore           # Fișiere ignorate la build
└── README.md            # Documentația completă a proiectului
```

---

## 6. Rularea Simulării

Proiectul poate fi compilat și rulat fie din **OMNeT++ IDE**, fie direct din linia de comandă:

```bash
# 1. Generare Makefile
opp_makemake -f --deep

# 2. Compilare
make

# 3. Rulare simulare (mod consolă fără GUI)
opp_run -u Cmdenv -c Sched1 omnetpp.ini

# Sau rulare cu interfață grafică (Qtenv)
opp_run -u Qtenv -c Sched1 omnetpp.ini
```

### Fișiere de Rezultate Generate:
* `results_source.txt`: Numărul de pachete generate de fiecare sursă.
* `results_scheduler.txt`: Numărul de cicluri de planificare și totalul de blocuri alocate pe fiecare utilizator.
* `results_sink.txt`: Numărul de pachete sosite la destinație pentru fiecare utilizator.
