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

    bool parseMsgPack(MsgPack::Element* vectorElement) {
        auto vectorElement = dynamic_cast<MsgPack::Array*>(vectorElement);
        if(!vectorElement)
            return false;
        auto vectorVector = vectorElement->getElementsVector();
        if(vectorVector->size() != motorCount)
            return false;
        for(size_t i = 0; i < motorCount; ++i) {
            auto scalarElement = dynamic_cast<MsgPack::Number*>((*vectorVector)[i].get());
            if(!scalarElement)
                return false;
            coords[i] = scalarElement->getValue<float>();
        }
        std::cout << *this << std::endl;
        return true;
    }

    Vector normalized() const {
        return *this*(1.0/length());
    }

    float squaredLength() const {
        return dot(*this);
    }

    float length() const {
        return sqrt(squaredLength());
    }

    float dot(const Vector& other) const {
        float result = 0.0;
        for(size_t i = 0; i < motorCount; ++i)
            result += coords[i]*other.coords[i];
        return result;
    }

    float dotNormalized(const Vector& other) const {
        return dot(other)/(length()*other.length());
    }

    float angleTo(const Vector& other) const {
        return acos(dotNormalized(other));
    }

    Vector operator-(const Vector& other) const {
        Vector result;
        for(size_t i = 0; i < motorCount; ++i)
            result.coords[i] = coords[i]-other.coords[i];
        return result;
    }

    Vector operator+(const Vector& other) const {
        Vector result;
        for(size_t i = 0; i < motorCount; ++i)
            result.coords[i] = coords[i]+other.coords[i];
        return result;
    }

    Vector operator*(float factor) const {
        Vector result;
        for(size_t i = 0; i < motorCount; ++i)
            result.coords[i] = coords[i]*factor;
        return result;
    }

    friend std::ostream& operator<<(std::ostream& stream, const Vector& vector) {
        for(size_t i = 0; i < motorCount; ++i) {
            if(i > 0)
                stream << " ";
            stream << vector.coords[i];
        }
        return stream;
    }
};

struct Vertex {
    float speed;
    Vector pos;
};
std::deque<Vertex> vertices;
Vector position;

void resetMachine() {
    vertices.clear();
    for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex) {
        motors[motorIndex]->setIdle(false);
        motors[motorIndex]->resetHome();
    }
}

void stopMotors() {
    for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
        motors[motorIndex]->stop(false);
}

void readMotors() {
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
            resetMachine();
            return;
        }
        motors[motorIndex]->updatePosition();
        position.coords[motorIndex] = motors[motorIndex]->getPositionInTurns();
    }
}

void writeMotors() {
    if(vertices.empty()) {
        std::cout << "no vertices" << std::endl;
        stopMotors();
        return;
    }

    Vector targetPos = vertices[0].pos,
           targetDiff = targetPos-position;
    float speed = vertices[0].speed,
          targetDist = targetDiff.length(),
          timeLeft = targetDist/speed;
    const finalProximity = 0.01,
          slowDown = 0.1;

    if(targetDist == 0) {
        std::cout << "reached vertex" << std::endl;
        vertices.pop_front();
        return;
    } else if(timeLeft < finalProximity) {
        std::cout << "finalizing vertex" << std::endl;
        for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
            motors[motorIndex]->goToInTurns(targetPos.coords[motorIndex]);
        return;
    } else if(timeLeft <= slowDown) {
        std::cout << "closing in on vertex" << std::endl;
        speed *= timeLeft/slowDown;
    }

    std::cout << targetDist << " " << speed << " " << timeLeft << std::endl;
    Vector velocity = targetDiff.normalized()*speed;
    for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex) {
        std::cout << motorIndex << " " << position.coords[motorIndex] << " " velocity.coords[motorIndex] << << std::endl;
        motors[motorIndex]->runInHz(velocity.coords[motorIndex]);
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
    resetMachine();
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
    resetMachine();

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
        if(type == "polygon") {
            auto iter = map.find("speed");
            if(iter == map.end())
                return;
            auto speedElement = dynamic_cast<MsgPack::Number*>(iter->second);
            if(!speedElement)
                return;
            targetSpeed = speedElement->getValue<float>();
            iter = map.find("vertices");
            if(iter == map.end())
                return;
            auto verticesElement = dynamic_cast<MsgPack::Array*>(iter->second);
            if(!verticesElement)
                return;
            auto verticesVector = verticesElement->getElementsVector();
            for(size_t vertexIndex = 0; vertexIndex < verticesVector->size(); ++vertexIndex) {
                std::cout << "vertex " << vertexIndex << " " << targetSpeed << " ";
                Vertex vertex;
                vertex.speed = targetSpeed;
                vertex.pos.parseMsgPack((*verticesVector)[vertexIndex].get());
                vertices.push_back(vertex);
            }
            return;
        } else if(type == "interrupt") {
            vertices.clear();
            Vector targetPos = position;
            targetPos.coords[2] += 10;
            vertices.push_back(targetPos);
            return;
        } else if(type == "reset") {
            resetMachine();
            return;
        }
        std::function<bool(L6470*)> command;
        if(type == "run") {
            if(!vertices.empty())
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
            float secondsSinceLastUpdate = std::chrono::duration<float>(runLoopNow-runLoopLastUpdate).count();

            readMotors();
            writeMotors();

            std::cout << "secondsSinceLastUpdate " << secondsSinceLastUpdate << std::endl;
            if(secondsSinceLastUpdate > 0.01) {
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
