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

#include "myQ.h"

Define_Module(MyQ);

MyQ::MyQ()
{
}

MyQ::~MyQ()
{
    queue.clear();
}

void MyQ::initialize()
{
    queue.setName("queue");
}

void MyQ::handleMessage(cMessage *msg)
{
    if (msg->arrivedOn("rxPackets")) {
        // Sosire pachet de la generatorul sursa -> se insereaza in coada
        queue.insert(msg);
    } else if (msg->arrivedOn("rxScheduling")) {
        // Comanda de alocare primita de la Scheduler
        int nrBlocks = 0;
        if (msg->hasPar("nrBlocks")) {
            nrBlocks = (int)msg->par("nrBlocks").longValue();
        } else if (msg->hasPar("nrOfBlocks")) {
            nrBlocks = (int)msg->par("nrOfBlocks").longValue();
        }
        delete msg; // stergem mesajul de comanda

        // Scoatem cel mult nrBlocks pachete din coada si le transmitem catre Sink
        // La depanare folosim conditia !queue.isEmpty(); cand schedulerul e calibrat,
        // nu va aloca mai multe blocuri decat sunt disponibile.
        for (int j = 0; j < nrBlocks && !queue.isEmpty(); j++) {
            cMessage *pkt = (cMessage *)queue.pop();
            send(pkt, "txPackets");
        }
    }

    // Aflam lungimea curenta a cozii
    int ql = queue.getLength();

    // Transmitem starea cozii catre Scheduler
    cMessage *qInfo = new cMessage("qInfo");
    qInfo->addPar("ql_info");
    qInfo->par("ql_info").setLongValue(ql);

    int userIndex = getParentModule()->getIndex();
    EV << "User[" << userIndex << "] MyQ: ql = " << ql << endl;

    send(qInfo, "txInfo");
}
