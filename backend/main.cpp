#include "L6470.h"
#include <chrono>
#include <netLink/netLink.h>

const size_t motorCount = 3;
L6470* motors[motorCount];
GPIOpin motorDriversActive;
netLink::SocketManager socketManager;
std::shared_ptr<netLink::Socket> serverSocket = socketManager.newMsgPackSocket();
std::queue<std::unique_ptr<MsgPack::Element>> commands;
std::chrono::time_point<std::chrono::system_clock> runLoopLastUpdate, runLoopNow;
float targetSpeed, srcPos[motorCount], dstPos[motorCount];
size_t polygonVertex;

void resetCommand() {
    polygonVertex = 0;
    targetSpeed = 0.0;
}

void handleCommand() {
    {
        printf("handleCommand();\n");
        if(commands.empty()) return;
        auto element = commands.front().get();
        auto mapElement = dynamic_cast<MsgPack::Map*>(element);
        if(!mapElement) goto cancel;
        auto map = mapElement->getElementsMap();
        auto iter = map.find("vertices");
        if(iter == map.end()) goto cancel;
        auto verticesElement = dynamic_cast<MsgPack::Array*>(iter->second);
        if(!verticesElement) goto cancel;
        auto verticesVector = verticesElement->getElementsVector();
        if(polygonVertex < verticesVector->size()) {
            auto vertexElement = dynamic_cast<MsgPack::Array*>((*verticesVector)[polygonVertex].get());
            if(!vertexElement) goto cancel;
            auto vertexVector = vertexElement->getElementsVector();
            if(vertexVector->size() != motorCount) goto cancel;
            for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex) {
                auto scalarElement = dynamic_cast<MsgPack::Number*>((*vertexVector)[motorIndex].get());
                if(!scalarElement) goto cancel;
                srcPos[motorIndex] = (polygonVertex == 0) ? motors[motorIndex]->getPositionInTurns() : dstPos[motorIndex];
                dstPos[motorIndex] = scalarElement->getValue<float>();
                printf("NEXT VERTEX %d %d %f %f\n", polygonVertex, motorIndex, srcPos[motorIndex], dstPos[motorIndex]);
            }
            ++polygonVertex;
        }else{
            printf("LAST VERTEX\n");
            for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
                motors[motorIndex]->setIdle(false);
            goto cancel;
        }
        iter = map.find("speed");
        if(iter == map.end()) goto cancel;
        auto speedElement = dynamic_cast<MsgPack::Number*>(iter->second);
        if(!speedElement) goto cancel;
        targetSpeed = speedElement->getValue<float>();
        return;
    }

    cancel:
    resetCommand();
    commands.pop();
}

int main(int argc, char** argv) {
    resetCommand();
    SPI bus(motorCount, 5000000);
    motorDriversActive.setIndex(7);
    motorDriversActive.setMode(1);
    motorDriversActive.setValue(1);

    for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
        motors[motorIndex] = new L6470(&bus, motorIndex);

    socketManager.onConnectRequest = [](netLink::SocketManager* manager, std::shared_ptr<netLink::Socket> serverSocket, std::shared_ptr<netLink::Socket> clientSocket) {
        std::cout << "Accepted connection from " << clientSocket->hostRemote << ":" << clientSocket->portRemote << std::endl;
        return true;
    };

    socketManager.onStatusChanged = [](netLink::SocketManager* manager, std::shared_ptr<netLink::Socket> socket, netLink::Socket::Status prev) {
        std::cout << "Status of " << socket->hostRemote << ":" << socket->portRemote << " changed from " << (int)prev << " to " << (int)(socket.get()->getStatus()) << std::endl;
    };

    socketManager.onDisconnect = [](netLink::SocketManager* manager, std::shared_ptr<netLink::Socket> socket) {
        std::cout << "Lost connection of " << socket->hostRemote << ":" << socket->portRemote << std::endl;
    };

    socketManager.onReceiveMsgPack = [&](netLink::SocketManager* manager, std::shared_ptr<netLink::Socket> socket, std::unique_ptr<MsgPack::Element> element) {
        std::cout << "Received data from " << socket->hostRemote << ":" << socket->portRemote << ": " << *element << std::endl;
        auto mapElement = dynamic_cast<MsgPack::Map*>(element.get());
        if(!mapElement) return;
        auto map = mapElement->getElementsMap();
        auto iter = map.find("type");
        if(iter == map.end()) return;
        auto typeElement = dynamic_cast<MsgPack::String*>(iter->second);
        if(!typeElement) return;
        auto type = typeElement->stdString();
        if(type == "polygon") {
            commands.push(std::move(element));
            handleCommand();
            return;
        }
        std::function<bool(L6470*)> command;
        if(type == "run") {
            if(!commands.empty()) return;
            iter = map.find("speed");
            if(iter == map.end()) return;
            auto speedElement = dynamic_cast<MsgPack::Number*>(iter->second);
            if(!speedElement) return;
            command = std::bind(&L6470::runInHz, std::placeholders::_1, speedElement->getValue<float>());
        }else if(type == "stop") {
            resetCommand();
            while(!commands.empty())
                commands.pop();
            command = std::bind(&L6470::setIdle, std::placeholders::_1, false);
        }else return;
        iter = map.find("motor");
        if(iter != map.end()) {
            auto motorElement = dynamic_cast<MsgPack::Number*>(iter->second);
            if(!motorElement) return;
            size_t motorIndex = motorElement->getValue<size_t>();
            if(motorIndex > motorCount) return;
            command(motors[motorIndex]);
        }else for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
            command(motors[motorIndex]);
    };

    runLoopLastUpdate = std::chrono::system_clock::now();
    try {
        serverSocket->initAsTcpServer("*", 3823);
        while(true) {
            runLoopNow = std::chrono::system_clock::now();
            std::chrono::duration<float> networkTimer = runLoopNow-runLoopLastUpdate;

            float vecA[motorCount], vecB[motorCount], vecC[motorCount], factorA = 0.0, factorB = 0.0;
            for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex) {
                const char* error = motors[motorIndex]->getStatus();
                if(error) {
                    for(auto& iter : serverSocket.get()->clients) {
                        netLink::MsgPackSocket& msgPackSocket = *static_cast<netLink::MsgPackSocket*>(iter.get());
                        msgPackSocket << MsgPack__Factory(MapHeader(3));
                        msgPackSocket << MsgPack::Factory("type");
                        msgPackSocket << MsgPack::Factory("error");
                        msgPackSocket << MsgPack::Factory("motor");
                        msgPackSocket << MsgPack::Factory((uint64_t)motorIndex);
                        msgPackSocket << MsgPack::Factory("message");
                        msgPackSocket << MsgPack::Factory(error);
                    }
                    goto stopServer;
                }
                motors[motorIndex]->updatePosition();
                vecA[motorIndex] = dstPos[motorIndex]-srcPos[motorIndex];
                factorA += vecA[motorIndex]*vecA[motorIndex];
                vecB[motorIndex] = dstPos[motorIndex]-motors[motorIndex]->getPositionInTurns();
                factorB += vecB[motorIndex]*vecB[motorIndex];
            }

            factorA = sqrt(factorB)/sqrt(factorA);
            factorB = 0.0;
            for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex) {
                vecC[motorIndex] = vecB[motorIndex]-vecA[motorIndex]*factorA;
                vecC[motorIndex] *= 2.0;
                vecC[motorIndex] += vecB[motorIndex];
                factorB += vecC[motorIndex]*vecC[motorIndex];
            }
            factorB = sqrt(factorB);

            if(factorB < 0.001)
                handleCommand();

            factorB = targetSpeed*factorA/factorB; // TODO: Prevent asymptote
            printf("%f %f\n", factorA, targetSpeed*factorA);
            for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex) {
                motors[motorIndex]->runInHz(vecC[motorIndex]*factorB);
                printf("%d %1.3f %1.3f %1.3f %4.3f\n", motorIndex, vecA[motorIndex], vecB[motorIndex], vecC[motorIndex]*factorB, motors[motorIndex]->getPositionInTurns());
            }

            if(networkTimer.count() > 0.01) {
                runLoopLastUpdate = runLoopNow;
                for(auto& iter : serverSocket.get()->clients) {
                    netLink::MsgPackSocket& msgPackSocket = *static_cast<netLink::MsgPackSocket*>(iter.get());
                    msgPackSocket << MsgPack__Factory(MapHeader(3));
                    msgPackSocket << MsgPack::Factory("type");
                    msgPackSocket << MsgPack::Factory("position");
                    msgPackSocket << MsgPack::Factory("timeLag");
                    msgPackSocket << MsgPack::Factory(networkTimer.count());
                    msgPackSocket << MsgPack::Factory("coords");
                    msgPackSocket << MsgPack__Factory(ArrayHeader(motorCount));
                    for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
                        msgPackSocket << MsgPack::Factory(motors[motorIndex]->getPositionInTurns());
                }
            }
            socketManager.listen();

            continue;
            stopServer: break;
        }
    }catch(netLink::Exception exc) {
        std::cout << "netLink::Exception " << (int)exc.code << " occured" << std::endl;
    }

    for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
        motors[motorIndex]->setIdle(false);
    motorDriversActive.setValue(0);
    socketManager.listen();

    return 0;
}
