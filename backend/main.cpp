#include "L6470.h"
#include <chrono>
#include <signal.h>
#include <netLink/netLink.h>

const size_t motorCount = 3;
L6470* motors[motorCount];
GPIOpin motorDriversActive;
netLink::SocketManager socketManager;
std::shared_ptr<netLink::Socket> serverSocket = socketManager.newMsgPackSocket();
std::chrono::time_point<std::chrono::system_clock> runLoopLastUpdate, runLoopNow;

struct Vector {
    float coords[motorCount];

    Vector normalized() {
        return *this*(1.0/length());
    }

    float length() {
        return sqrt(dot(*this));
    }

    float dot(const Vector& other) {
        float result = 0.0;
        for(size_t i = 0; i < motorCount; ++i)
            result += coords[i]*other.coords[i];
        return result;
    }

    Vector operator-(const Vector& other) {
        Vector result;
        for(size_t i = 0; i < motorCount; ++i)
            result.coords[i] = coords[i]-other.coords[i];
        return result;
    }

    Vector operator+(const Vector& other) {
        Vector result;
        for(size_t i = 0; i < motorCount; ++i)
            result.coords[i] = coords[i]+other.coords[i];
        return result;
    }

    Vector operator*(float factor) {
        Vector result;
        for(size_t i = 0; i < motorCount; ++i)
            result.coords[i] = coords[i]*factor;
        return result;
    }
};

struct Vertex {
    float speed;
    Vector prev, pos, next;
};

std::vector<Vertex> vertices;
bool running = false;
Vector position;

void calculateTangent(float param) {
    if(vertices.size() < 2)
        return;
    Vertex& prev = vertices[0];
    Vertex& next = vertices[1];
    float coParam = 1.0-param,
          paramSquared = param*param,
          coParamSquared = coParam*coParam,
          a = coParam*coParam,
          b = 2.0*coParam*param,
          c = param*param;
    Vector tangent = ((prev.next-prev.pos)*a+(next.prev-prev.next)*b+(next.pos-next.prev)*c).normalized();
}

bool parseVector(Vector& vector, std::map<std::string, Element*>::iterator* iter) {
    auto vectorElement = dynamic_cast<MsgPack::Map*>(iter->second);
    if(!vectorElement)
        return false;
    auto vectorVector = verticesElement->getElementsVector();
    if(vectorVector->size() != motorCount)
        return false;
    for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex) {
        auto scalarElement = dynamic_cast<MsgPack::Number*>((*vectorVector)[motorIndex].get());
        if(!scalarElement)
            return false;
        vector.coords[motorIndex] = scalarElement->getValue<float>();
    }
    return true;
}

void resetCommand() {
    vertices.clear();
    running = false;
}

void stopRunning() {
    resetCommand();
    for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
        motors[motorIndex]->setIdle(false);
}

void motorUpdate() {
    for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex) {
        const char* error = motors[motorIndex]->getStatus();
        if(error) {
            for(auto& iter : serverSocket.get()->clients) {
                netLink::MsgPackSocket& msgPackSocket = *static_cast<netLink::MsgPackSocket*>(iter.get());
                msgPackSocket << MsgPack__Factory(MapHeader(3));
                msgPackSocket << MsgPack::Factory("type");
                msgPackSocket << MsgPack::Factory("exception");
                msgPackSocket << MsgPack::Factory("motor");
                msgPackSocket << MsgPack::Factory((uint64_t)motorIndex);
                msgPackSocket << MsgPack::Factory("message");
                msgPackSocket << MsgPack::Factory(error);
            }
            stopRunning();
            return;
        }
        motors[motorIndex]->updatePosition();
        position.coords[motorIndex] = motors[motorIndex]->getPositionInTurns();
    }
}

void networkUpdate() {
    for(auto& iter : serverSocket.get()->clients) {
        netLink::MsgPackSocket& msgPackSocket = *static_cast<netLink::MsgPackSocket*>(iter.get());
        msgPackSocket << MsgPack__Factory(MapHeader(5));
        msgPackSocket << MsgPack::Factory("type");
        msgPackSocket << MsgPack::Factory("status");
        msgPackSocket << MsgPack::Factory("verticesLeft");
        msgPackSocket << MsgPack::Factory(static_cast<uint64_t>(vertices.size()));
        msgPackSocket << MsgPack::Factory("position");
        msgPackSocket << MsgPack__Factory(ArrayHeader(motorCount));
        for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
            msgPackSocket << MsgPack::Factory(motors[motorIndex]->getPositionInTurns());
    }
}

void onExit(int sig) {
    stopRunning();
    motorDriversActive.setValue(0);
    socketManager.listen();
    exit(0);
}

int main(int argc, char** argv) {
    signal(SIGINT, onExit);

    SPI bus(motorCount, 5000000);
    motorDriversActive.setIndex(7);
    motorDriversActive.setMode(1);
    motorDriversActive.setValue(1);
    for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
        motors[motorIndex] = new L6470(&bus, motorIndex);
    stopRunning();

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
        if(!mapElement)
            return;
        auto map = mapElement->getElementsMap();
        auto iter = map.find("type");
        if(iter == map.end())
            return;
        auto typeElement = dynamic_cast<MsgPack::String*>(iter->second);
        if(!typeElement)
            return;
        auto type = typeElement->stdString();
        if(type == "curve") {
            auto iter = map.find("vertices");
            if(iter == map.end())
                return;
            auto verticesElement = dynamic_cast<MsgPack::Array*>(iter->second);
            if(!verticesElement)
                return;
            auto verticesVector = verticesElement->getElementsVector();
            for(size_t vertexIndex = 0; vertexIndex < verticesVector->size(); ++vertexIndex) {
                Vertex vertex;
                auto vertexMap = dynamic_cast<MsgPack::Map*>((*verticesVector)[vertexIndex]);
                iter = vertexMap.find("speed");
                if(iter == vertexMap.end())
                    return;
                auto scalarElement = dynamic_cast<MsgPack::Number*>(iter.get());
                if(!scalarElement)
                    return;
                vertex.speed = scalarElement->getValue<float>();
                iter = vertexMap.find("prev");
                if(iter != vertexMap.end())
                    parseVector(vertex.prev, iter);
                iter = vertexMap.find("pos");
                if(iter != vertexMap.end())
                    parseVector(vertex.pos, iter);
                iter = vertexMap.find("next");
                if(iter != vertexMap.end())
                    parseVector(vertex.next, iter);
            }
            running = true;
            return;
        } /*else if(type == "interrupt") {
            return;
        } else if(type == "resume") {
            return;
        } */ else if(type == "reset") {
            stopRunning();
            return;
        } else if(type == "origin") {
            if(running)
                return;
            for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
                motors[motorIndex]->resetHome();
            return;
        }
        std::function<bool(L6470*)> command;
        if(type == "run") {
            if(running)
                return;
            iter = map.find("speed");
            if(iter == map.end())
                return;
            auto speedElement = dynamic_cast<MsgPack::Number*>(iter->second);
            if(!speedElement)
                return;
            command = std::bind(&L6470::runInHz, std::placeholders::_1, speedElement->getValue<float>());
        } else
            return;
        iter = map.find("motor");
        if(iter != map.end()) {
            auto motorElement = dynamic_cast<MsgPack::Number*>(iter->second);
            if(!motorElement)
                return;
            size_t motorIndex = motorElement->getValue<size_t>();
            if(motorIndex > motorCount)
                return;
            command(motors[motorIndex]);
        } else for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
            command(motors[motorIndex]);
    };

    runLoopLastUpdate = std::chrono::system_clock::now();
    try {
        serverSocket->initAsTcpServer("*", 3823);
        while(true) {
            runLoopNow = std::chrono::system_clock::now();
            std::chrono::duration<float> networkTimer = runLoopNow-runLoopLastUpdate;

            motorUpdate();

            if(running) {
                // motors[motorIndex]->runInHz(directedSpeed*factorA);
            }

            if(networkTimer.count() > 0.01) {
                runLoopLastUpdate = runLoopNow;
                networkUpdate();
            }

            socketManager.listen();
        }
    } catch(netLink::Exception exc) {
        std::cout << "netLink::Exception " << (int)exc.code << " occured" << std::endl;
    }

    return 0;
}
