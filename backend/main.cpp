#include "L6470.h"
//#include <functional>
#include <netLink/netLink.h>

const size_t motorCount = 3;

struct ControlPoint {
    float coord[3];
};

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

    socketManager.onReceiveMsgPack = [](netLink::SocketManager* manager, std::shared_ptr<netLink::Socket> socket, std::unique_ptr<MsgPack::Element> element) {
        std::cout << "Received data from " << socket->hostRemote << ":" << socket->portRemote << ": " << *element << std::endl;
        auto mapElement = dynamic_cast<MsgPack::Map*>(element.get());
        if(!mapElement) return;
        auto map = mapElement->getElementsMap();
        auto typeIter = map.find("type");
        if(typeIter == map.end()) return;
        auto typeElement = dynamic_cast<MsgPack::String*>(typeIter.second);
        if(!typeElement) return;
        std::string type = typeElement->stdString();
        std::function<bool(L6470*)> command;
        if(type == "run")
            command = std::bind(&L6470::run, std::placeholders::_1, speed, dir);
        else if(type == "stop")
            command = std::bind(&L6470::stop, std::placeholders::_1, false);
        else if(type == "idle")
            command = std::setIdle(&L6470::stop, std::placeholders::_1, false);
        else return;
        auto motorIter = map.find("type");
        if(motorIter != map.end()) {
            auto motorElement = dynamic_cast<MsgPack::Number*>(motorIter.second);
            if(!motorElement) return;
            size_t motorIndex = motorElement->template getValue<size_t>();
            command(motors[motorIndex]);
        }else for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
            command(motors[motorIndex]);
    };

    try {
        serverSocket->initAsTcpServer("*", 3823);
        for(bool serverRunning = true; serverRunning; ) {
            for(size_t i = 0; i < motorCount; ++i) {
                const char* error = motors[i]->getStatus();
                if(error) {
                    for(auto& iter : serverSocket.get()->clients) {
                        netLink::MsgPackSocket& msgPackSocket = *static_cast<netLink::MsgPackSocket*>(iter.get());
                        msgPackSocket << MsgPack__Factory(MapHeader(3));
                        msgPackSocket << MsgPack::Factory("type");
                        msgPackSocket << MsgPack::Factory("error");
                        msgPackSocket << MsgPack::Factory("motor");
                        msgPackSocket << MsgPack::Factory(i);
                        msgPackSocket << MsgPack::Factory("message");
                        msgPackSocket << MsgPack::Factory(error);
                    }
                    serverRunning = false;
                    break;
                }
                motors[i]->updatePosition();
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
