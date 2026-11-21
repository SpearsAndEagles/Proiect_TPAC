//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "Scheduler.h"
#include <fstream>
#include <algorithm>

Define_Module(Scheduler);

Scheduler::Scheduler()
{
    selfMsg = nullptr;
    NrUsers = 0;
    NrOfChannels = 10;
    currentRRIndex = 0;
    totalCycles = 0;
    for (int i = 0; i < 10; i++) {
        q[i] = 0;
        NrBlocks[i] = 0;
        userWeights[i] = 1;
        totalBlocksAllocated[i] = 0;
    }
}

Scheduler::~Scheduler()
{
    cancelAndDelete(selfMsg);
}

void Scheduler::initialize()
{
    NrUsers = par("gateSize").intValue();
    if (NrUsers > 10) {
        throw cRuntimeError("Scheduler suporta cel mult 10 utilizatori, gateSize=%d", NrUsers);
    }

    if (hasPar("channelsCount")) {
        NrOfChannels = par("channelsCount").intValue();
    } else {
        NrOfChannels = 10;
    }

    // Initializam ponderile/prioritatile utilizatorilor
    // Exemplu: User 0 (prioritate 3), User 1 (prioritate 2), User 2 (prioritate 1)
    if (NrUsers >= 3) {
        userWeights[0] = 3;
        userWeights[1] = 2;
        userWeights[2] = 1;
    }
    for (int i = 3; i < NrUsers; i++) {
        userWeights[i] = 1;
    }

    for (int i = 0; i < 10; i++) {
        q[i] = 0;
        NrBlocks[i] = 0;
        totalBlocksAllocated[i] = 0;
    }

    currentRRIndex = 0;
    totalCycles = 0;

    EV << "Scheduler Initializat: NrUsers=" << NrUsers
       << ", NrOfChannels=" << NrOfChannels
       << ", schedulingPeriod=" << par("schedulingPeriod").doubleValue() << "s" << endl;
    for (int i = 0; i < NrUsers; i++) {
        EV << "  User[" << i << "] Pondere=" << userWeights[i] << endl;
    }

    selfMsg = new cMessage("selfMsg");
    scheduleAt(simTime(), selfMsg);
}

void Scheduler::handleMessage(cMessage *msg)
{
    // 1. Daca mesajul este o actualizare de stare a cozii (rxInfo de la user i)
    for (int i = 0; i < NrUsers; i++) {
        if (msg->arrivedOn("rxInfo", i)) {
            q[i] = (int)msg->par("ql_info").longValue();
            EV << "Scheduler Update: q[" << i << "] = " << q[i] << " la T=" << simTime() << endl;
            delete msg;
            return;
        }
    }

    // 2. Daca mesajul este timer-ul periodic de scheduling
    if (msg == selfMsg) {
        totalCycles++;

        // Rulam algoritmul Weighted Round Robin peste cele NrOfChannels canale
        scheduleWeightedRoundRobin();

        // Trimitem comenzile de alocare catre utilizatori
        for (int i = 0; i < NrUsers; i++) {
            if (NrBlocks[i] > 0) {
                // Actualizam estimarea interna a lungimii cozii
                q[i] = std::max(0, q[i] - NrBlocks[i]);
                totalBlocksAllocated[i] += NrBlocks[i];

                cMessage *cmd = new cMessage("cmd");
                cmd->addPar("nrBlocks");
                cmd->par("nrBlocks").setLongValue(NrBlocks[i]);
                send(cmd, "txScheduling", i);
            }
        }

        // Reprogramam ceasul la pas fix (schedulingPeriod)
        scheduleAt(simTime() + par("schedulingPeriod").doubleValue(), selfMsg);
    }
}

void Scheduler::scheduleWeightedRoundRobin()
{
    // Resetam alocarea pentru ciclul curent
    for (int i = 0; i < NrUsers; i++) {
        NrBlocks[i] = 0;
    }

    int channelsRemaining = NrOfChannels;

    // Verificam daca exista cerere in cozi
    int totalQueued = 0;
    for (int i = 0; i < NrUsers; i++) {
        totalQueued += q[i];
    }

    if (totalQueued == 0) {
        return; // Nu avem pachete de servit
    }

    // Weighted Round Robin Oportunist:
    // In fiecare runda, utilizatorii primesc blocuri proportional cu userWeights[i],
    // limitat de cererea lor (q[i] - NrBlocks[i]) si de canalele ramase.
    // Daca un utilizator cere mai putin sau si-a golit coada, blocurile ramase sunt
    // redistribuite oportunist catre ceilalti utilizatori pana la epuizarea celor 10 canale.
    bool progress = true;
    while (channelsRemaining > 0 && progress) {
        progress = false;

        for (int k = 0; k < NrUsers; k++) {
            int i = (currentRRIndex + k) % NrUsers;

            if (channelsRemaining == 0) {
                break;
            }

            int needed = q[i] - NrBlocks[i];
            if (needed > 0) {
                int weight = userWeights[i] > 0 ? userWeights[i] : 1;
                int toAllocate = std::min(needed, std::min(weight, channelsRemaining));

                if (toAllocate > 0) {
                    NrBlocks[i] += toAllocate;
                    channelsRemaining -= toAllocate;
                    progress = true;
                }
            }
        }
    }

    // Rotim indexul Round Robin pentru urmatorul ciclu pentru a asigura echitate in timp
    currentRRIndex = (currentRRIndex + 1) % NrUsers;

    EV << "Scheduler T=" << simTime() << " [Ciclu " << totalCycles << "]: Alocare=[";
    for (int i = 0; i < NrUsers; i++) {
        EV << " U" << i << ":" << NrBlocks[i] << "bl (q=" << q[i] << ",w=" << userWeights[i] << ")";
    }
    EV << " ] Canale ramase libere=" << channelsRemaining << endl;
}

void Scheduler::finish()
{
    EV << "=== Scheduler Finish ===" << endl;
    EV << "Total cicluri de scheduling executate: " << totalCycles << endl;
    for (int i = 0; i < NrUsers; i++) {
        EV << "User[" << i << "] (Pondere " << userWeights[i] << "): total blocuri alocate = "
           << totalBlocksAllocated[i] << endl;
    }

    // Salvam rezultatele intr-un fisier
    std::ofstream outFile("results_scheduler.txt");
    if (outFile.is_open()) {
        outFile << "TotalCycles=" << totalCycles << std::endl;
        for (int i = 0; i < NrUsers; i++) {
            outFile << "User[" << i << "]: Weight=" << userWeights[i]
                    << ", TotalBlocksAllocated=" << totalBlocksAllocated[i] << std::endl;
        }
        outFile.close();
    }
}
