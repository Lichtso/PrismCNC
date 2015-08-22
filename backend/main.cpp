#include "L6470.h"
#include <chrono>
#include <netLink/netLink.h>

const size_t motorCount = 3;
std::chrono::time_point<std::chrono::system_clock> runLoopPrev, runLoopNow;
std::chrono::time_point<std::chrono::system_clock> srcTime, dstTime;
float srcPos[motorCount], dstPos[motorCount];
enum Mode {
    IDLE,
    MANUEL,
    AUTOMATIC
} mode;

int main(int argc, char** argv) {
    SPI bus(motorCount, 5000000);
    GPIOpin motorDriversActive;
    motorDriversActive.setIndex(7);
    motorDriversActive.setMode(1);
    motorDriversActive.setValue(1);
    L6470* motors[motorCount];
    for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
        motors[motorIndex] = new L6470(&bus, motorIndex);

    /*size_t pCount = 128;
    ControlPoint points[pCount];
    for(size_t p = 0; p < pCount; ++p) {
        float angle = (float)p/pCount*M_PI*2.0, radius = 10.0;
        ControlPoint& point = points[p];
        point.coord[0] = sin(angle)*radius;
        point.coord[1] = cos(angle)*radius;
        point.coord[2] = 0.0;
    }

    bool running = true;
    for(size_t p = 1; running && p < pCount; ++p) {
        ControlPoint &prev = points[p-1], &next = points[p];
        for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex) {
            motors[motorIndex]->run(p*20, true);
            motors[motorIndex]->getParam(L6470::ParamName::SPEED, value);
            printf("%d %d\n", i, value);
        }
        usleep(1000);
    }*/

    netLink::SocketManager socketManager;
    std::shared_ptr<netLink::Socket> serverSocket = socketManager.newMsgPackSocket();

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
        std::string type = typeElement->stdString();
        if(type == "goto") {
            iter = map.find("pos");
            if(iter == map.end()) return;
            auto posElement = dynamic_cast<MsgPack::Array*>(iter->second);
            if(!posElement) return;
            auto posVector = posElement->getElementsVector();
            if(posVector->size() != motorCount) return;
            iter = map.find("duration");
            if(iter == map.end()) return;
            auto durationElement = dynamic_cast<MsgPack::Number*>(iter->second);
            if(!durationElement) return;
            srcTime = dstTime = runLoopNow;
            dstTime += std::chrono::microseconds(durationElement->getValue<uint64_t>());
            for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex) {
                srcPos[motorIndex] = motors[motorIndex]->getPositionInMM();
                auto axisElement = dynamic_cast<MsgPack::Number*>(posVector[motorIndex].get());
                if(!axisElement) return;
                dstPos[motorIndex] = axisElement->getValue<float>();
            }
            mode = AUTOMATIC;
        }
        std::function<bool(L6470*)> command;
        if(type == "run") {
            if(mode == AUTOMATIC) return;
            iter = map.find("speed");
            if(iter == map.end()) return;
            auto speedElement = dynamic_cast<MsgPack::Number*>(iter->second);
            if(!speedElement) return;
            int64_t speed = speedElement->getValue<int64_t>();
            command = std::bind(&L6470::run, std::placeholders::_1, (uint32_t)std::abs(speed), (speed >= 0));
            mode = MANUEL;
        }else if(type == "stop") {
            command = std::bind(&L6470::stop, std::placeholders::_1, false);
            mode = MANUEL;
        }else if(type == "idle") {
            command = std::bind(&L6470::setIdle, std::placeholders::_1, false);
            mode = IDLE;
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

    runLoopPrev = std::chrono::system_clock::now();
    try {
        serverSocket->initAsTcpServer("*", 3823);
        for(bool serverRunning = true; serverRunning; ) {
            runLoopNow = std::chrono::system_clock::now();
            auto timeLeft = std::chrono::duration_cast<std::chrono::microseconds>(dstTime-runLoopNow).count();
            // auto totalTime = std::chrono::duration_cast<std::chrono::microseconds>(dstTime-srcTime).count();
            // float progress = std::min(std::max((float)timeInterval/totalTime), 0.0F) 1.0F);
            if(mode == AUTOMATIC && timeLeft <= 0) {
                mode = IDLE;
                for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
                    motors[motorIndex]->setIdle(false);
                printf("DONE\n");
            }

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
                if(mode == AUTOMATIC) {
                    // TODO: Take srcPos and srcTime into account too
                    float speed = (dstPos[motorIndex]-motors[motorIndex]->getPositionInMM())/(0.015F*timeLeft);
                    uint32_t speedUInt = std::abs(speed);
                    printf("%d %d", motorIndex, speedUInt);
                    motors[motorIndex]->run(speedUInt, speed >= 0.0F);
                    motors[motorIndex]->getParam(L6470::ParamName::SPEED, speedUInt);
                    printf(" %d\n", speedUInt);
                }
            }
            auto timeInterval = std::chrono::duration_cast<std::chrono::microseconds>(runLoopNow-runLoopPrev).count();
            if(mode != IDLE && timeInterval > 100000) {
                runLoopPrev = runLoopNow;
                for(auto& iter : serverSocket.get()->clients) {
                    netLink::MsgPackSocket& msgPackSocket = *static_cast<netLink::MsgPackSocket*>(iter.get());
                    msgPackSocket << MsgPack__Factory(MapHeader(3));
                    msgPackSocket << MsgPack::Factory("type");
                    msgPackSocket << MsgPack::Factory("position");
                    msgPackSocket << MsgPack::Factory("timeInterval");
                    msgPackSocket << MsgPack::Factory(timeInterval);
                    msgPackSocket << MsgPack::Factory("coords");
                    msgPackSocket << MsgPack__Factory(ArrayHeader(motorCount));
                    for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
                        msgPackSocket << MsgPack::Factory(motors[motorIndex]->getPositionInMM());
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
