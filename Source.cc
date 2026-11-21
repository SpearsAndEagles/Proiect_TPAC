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

#include "Source.h"
#include <fstream>

Define_Module(Source);

Source::Source()
{
    sendMessageEvent = nullptr;
    sendingTime = 0.0;
    maxSimTime = 0;
    numSentPackets = 0;
}

Source::~Source()
{
    cancelAndDelete(sendMessageEvent);
}

void Source::initialize()
{
    // 1. Preluam numarul de utilizatori din reteaua parinte RadioSch2
    int nrUsers = 3;
    cModule *parentNet = getParentModule()->getParentModule();
    if (parentNet && parentNet->hasPar("usersCount")) {
        nrUsers = parentNet->par("usersCount").intValue();
    }

    // 2. Parametrii pentru formula Network Load (NL)
    double netwload = par("netwload").doubleValue(); // e.g. 0.8 (80%)
    double schedulingCycle = 0.001;                 // 1 ms exprimat in secunde
    int nr_of_channels = 10;
    int nrPackets = 1;
    int packetLength = 1;

    // 3. Calculam sendingTime conform formulei:
    // sendingTime = (nrUsers * nrPackets * packetLength * schedulingCycle) / (netwload * nr_of_channels)
    sendingTime = (nrUsers * nrPackets * packetLength * schedulingCycle) / (netwload * nr_of_channels);

    // 4. Timpul maxim de simulare (ex: 1s / 1000 ms)
    maxSimTime = par("maxSimTime").doubleValue();

    numSentPackets = 0;

    int userIndex = getParentModule()->getIndex();
    EV << "Source User[" << userIndex << "]: Initializat."
       << " nrUsers=" << nrUsers
       << ", NL=" << netwload
       << ", sendingTime=" << sendingTime << "s (" << (sendingTime * 1000.0) << " ms)"
       << ", maxSimTime=" << maxSimTime << "s" << endl;

    // 5. Creem si programam primul self-message la momentul 0
    sendMessageEvent = new cMessage("sendMessageEvent");
    scheduleAt(simTime(), sendMessageEvent);
}

void Source::handleMessage(cMessage *msg)
{
    ASSERT(msg == sendMessageEvent);

    // Verificam conditia de oprire a simularii
    if (simTime() >= maxSimTime) {
        int userIndex = getParentModule()->getIndex();
        EV << "Source User[" << userIndex << "]: Timpul de simulare curent ("
           << simTime() << "s) >= maxSimTime (" << maxSimTime << "s). Oprim simularea." << endl;
        endSimulation();
        return;
    }

    // Generam si transmitem pachetul catre coada MyQ
    cMessage *job = new cMessage("job");
    send(job, "txPackets");
    numSentPackets++;

    // Reprogramam ACELASI self-message conform distributiei exponentiale cu media sendingTime
    scheduleAt(simTime() + exponential(sendingTime), sendMessageEvent);
}

void Source::finish()
{
    int userIndex = getParentModule()->getIndex();
    EV << "=== Source User[" << userIndex << "] Finish ===" << endl;
    EV << "Total pachete generate: " << numSentPackets << endl;
    EV << "Timp final simulare: " << simTime() << " s" << endl;
    EV << "sendingTime utilizat: " << sendingTime << " s (" << (sendingTime * 1000.0) << " ms)" << endl;

    // Scrierea rezultatelor intr-un fisier de statistici
    std::ofstream outFile("results_source.txt", std::ios::app);
    if (outFile.is_open()) {
        outFile << "User[" << userIndex << "]: "
                << "PacketsGenerated=" << numSentPackets << ", "
                << "SimTime=" << simTime() << ", "
                << "sendingTime=" << sendingTime << "s" << std::endl;
        outFile.close();
    }
}
