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

#include "Sink.h"
#include <fstream>

Define_Module(Sink);

void Sink::initialize()
{
    gateSize = par("gateSize").intValue();
    totalPacketsReceived = 0;
    lifetimeSignal = registerSignal("lifetime");

    for (int i = 0; i < 10; i++) {
        packetsPerUser[i] = 0;
    }
}

void Sink::handleMessage(cMessage *msg)
{
    simtime_t lifetime = simTime() - msg->getCreationTime();
    totalPacketsReceived++;

    int userIndex = -1;
    for (int i = 0; i < gateSize; i++) {
        if (msg->arrivedOn("rxPackets", i)) {
            userIndex = i;
            break;
        }
    }

    if (userIndex >= 0 && userIndex < 10) {
        packetsPerUser[userIndex]++;
    }

    emit(lifetimeSignal, lifetime);

    EV << "Sink: Primit pachet de la User[" << userIndex
       << "] la T=" << simTime() << ", lifetime: " << lifetime << "s" << endl;

    // Stergem mesajul imediat pentru a elibera memoria si a evita acumularea de delay
    delete msg;
}

void Sink::finish()
{
    EV << "=== Sink Finish ===" << endl;
    EV << "Total pachete receptionate: " << totalPacketsReceived << endl;
    for (int i = 0; i < gateSize && i < 10; i++) {
        EV << "Pachete receptionate de la User[" << i << "]: " << packetsPerUser[i] << endl;
    }

    std::ofstream outFile("results_sink.txt");
    if (outFile.is_open()) {
        outFile << "TotalPacketsReceived=" << totalPacketsReceived << std::endl;
        for (int i = 0; i < gateSize && i < 10; i++) {
            outFile << "User[" << i << "]=" << packetsPerUser[i] << std::endl;
        }
        outFile.close();
    }
}
