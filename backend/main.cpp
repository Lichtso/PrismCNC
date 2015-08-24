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
        printf("handleCommand(); 1\n");
        if(commands.empty()) return;
        auto element = commands.front().get();
        auto mapElement = dynamic_cast<MsgPack::Map*>(element);
        if(!mapElement) goto cancel;
        printf("handleCommand(); 2\n");
        auto map = mapElement->getElementsMap();
        auto iter = map.find("vertices");
        if(iter == map.end()) goto cancel;
        auto verticesElement = dynamic_cast<MsgPack::Array*>(iter->second);
        if(!verticesElement) goto cancel;
        printf("handleCommand(); 3\n");
        auto verticesVector = verticesElement->getElementsVector();
        if(verticesVector->size() != motorCount) goto cancel;
        printf("handleCommand(); 4\n");

        if(polygonVertex < verticesVector->size()) {
            printf("handleCommand(); 5\n");
            auto vertexElement = dynamic_cast<MsgPack::Array*>((*verticesVector)[polygonVertex].get());
            if(!vertexElement) goto cancel;
            auto vertexVector = vertexElement->getElementsVector();
            if(vertexVector->size() != motorCount) goto cancel;
            if(polygonVertex == 0)
                printf("FIRST VERTEX\n");
            else
                printf("NEXT VERTEX\n");
            for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex) {
                auto scalarElement = dynamic_cast<MsgPack::Number*>((*vertexVector)[motorIndex].get());
                if(!scalarElement) goto cancel;
                srcPos[motorCount] = (polygonVertex == 0) ? motors[motorIndex]->getPosition() : dstPos[motorCount];
                dstPos[motorCount] = scalarElement->getValue<float>();
            }
            ++polygonVertex;
        }else{
            printf("LAST VERTEX\n");
            for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
                motors[motorIndex]->setIdle(false);
            goto cancel;
        }
        printf("handleCommand(); 6\n");
        iter = map.find("speed");
        if(iter == map.end()) goto cancel;
        auto speedElement = dynamic_cast<MsgPack::Number*>(iter->second);
        if(!speedElement) goto cancel;
        printf("handleCommand(); 7\n");
        float duration = 0.0;
        for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex) {
            float diff = dstPos[motorCount]-srcPos[motorCount];
            duration += diff*diff;
        }
        printf("handleCommand(); 8\n");
        duration = sqrt(duration)/speedElement->getValue<float>();
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
        for(bool serverRunning = true; serverRunning; ) {
            runLoopNow = std::chrono::system_clock::now();
            std::chrono::duration<float> timeLeft = dstTime-runLoopNow;
            if(!commands.empty() && timeLeft.count() <= 0)
                handleCommand();

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
                    serverRunning = false;
                    break;
                }
                motors[motorIndex]->updatePosition();
                if(!commands.empty()) {
                    float speed = (dstPos[motorIndex]-motors[motorIndex]->getPosition())/timeLeft.count();
                    motors[motorIndex]->runInHz(speed);
                    printf("%d %f %f\n", motorIndex, speed, motors[motorIndex]->getSpeedInHz());
                }
            }

            std::chrono::duration<float> timeLag = runLoopNow-runLoopLastUpdate;
            if(timeLag.count() > 0.01) {
                runLoopLastUpdate = runLoopNow;
                for(auto& iter : serverSocket.get()->clients) {
                    netLink::MsgPackSocket& msgPackSocket = *static_cast<netLink::MsgPackSocket*>(iter.get());
                    msgPackSocket << MsgPack__Factory(MapHeader(3));
                    msgPackSocket << MsgPack::Factory("type");
                    msgPackSocket << MsgPack::Factory("position");
                    msgPackSocket << MsgPack::Factory("timeLag");
                    msgPackSocket << MsgPack::Factory(timeLag.count());
                    msgPackSocket << MsgPack::Factory("coords");
                    msgPackSocket << MsgPack__Factory(ArrayHeader(motorCount));
                    for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
                        msgPackSocket << MsgPack::Factory(motors[motorIndex]->getPosition());
                }
            }
            socketManager.listen();
        }
    }catch(netLink::Exception exc) {
        std::cout << "netLink::Exception " << (int)exc.code << " occured" << std::endl;
    }

    for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
        motors[motorIndex]->setIdle(false);
    motorDriversActive.setValue(0);

    return 0;
}
