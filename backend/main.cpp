#include "L6470.h"
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
    for(size_t i = 0; i < motorCount; ++i)
        motors[i] = new L6470(&bus, i);

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
        for(size_t i = 0; i < motorCount; ++i) {
            motors[i]->run(p*20, true);
            motors[i]->getParam(L6470::ParamName::SPEED, value);
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
    };

    try {
        serverSocket->initAsTcpServer("*", 3823);
        for(bool serverRunning = true; serverRunning; ) {
            for(size_t i = 0; i < motorCount; ++i) {
                const char* error = motors[i]->getStatus();
                if(error) {
                    std::cout << "ERROR occured" << std::endl;
                    for(auto& iter : serverSocket.get()->clients) {
                        std::cout << "ERROR broadcasted to " << iter.get()->hostRemote << std::endl;
                        netLink::MsgPackSocket& msgPackSocket = *static_cast<netLink::MsgPackSocket*>(iter.get());
                        msgPackSocket << MsgPack__Factory(MapHeader(2));
                        msgPackSocket << MsgPack::Factory("type");
                        msgPackSocket << MsgPack::Factory("error");
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

    for(size_t i = 0; i < motorCount; ++i)
        motors[i]->setIdle(false);
    motorDriversActive.setValue(0);

    return 0;
}
