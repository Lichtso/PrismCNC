#include "L6470.h"
#include <chrono>
#include <netLink/netLink.h>

const size_t motorCount = 3;
L6470* motors[motorCount];
GPIOpin motorDriversActive;
netLink::SocketManager socketManager;
std::shared_ptr<netLink::Socket> serverSocket = socketManager.newMsgPackSocket();
std::queue<std::unique_ptr<MsgPack::Element>> commands;
std::chrono::time_point<std::chrono::system_clock> runLoopLastUpdate, runLoopNow, dstTime;
float srcPos[motorCount], dstPos[motorCount];
size_t polygonVertex = 0;

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
        float duration = 0.0;
        for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex) {
            float diff = dstPos[motorIndex]-srcPos[motorIndex];
            duration += diff*diff;
        }
        duration = sqrt(duration);
        printf("handleCommand duration: %f ", duration);
        duration /= speedElement->getValue<float>();
        printf("%f\n", duration);
        dstTime = runLoopNow+std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<float>(duration));
        return;
    }

    cancel:
    printf("handleCommand(); cancel\n");
    commands.pop();
    polygonVertex = 0;
}

int main(int argc, char** argv) {
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
            bool posMatch = true;
            std::chrono::duration<float> networkTimer = runLoopNow-runLoopLastUpdate,
                                         timeLeft = dstTime-runLoopNow;

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
                posMatch &= motors[motorIndex]->isAtPositionInTurns(dst);
                if(!commands.empty()) {
                    float dst = dstPos[motorIndex], diff = dst-motors[motorIndex]->getPositionInTurns();
                    if(timeLeft.count() > 0.05) {
                        float speed = diff/timeLeft.count();
                        motors[motorIndex]->runInHz(speed);
                        printf("R %d %1.3f %1.3f %4.3f\n", motorIndex, speed, motors[motorIndex]->getSpeedInHz(), motors[motorIndex]->getPositionInTurns());
                    }else{
                        motors[motorIndex]->goToInTurns(dst);
                        printf("G %d %1.3f %1.3f %4.3f\n", motorIndex, dst, motors[motorIndex]->getSpeedInHz(), motors[motorIndex]->getPositionInTurns());
                    }
                }
            }

            if(!commands.empty() && posMatch)
                handleCommand();

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

    return 0;
}
