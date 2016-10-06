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

    bool parseMsgPack(std::map<std::string, MsgPack::Element*>& vertexMap, const char* name) {
        auto iter = vertexMap.find(name);
        if(iter == vertexMap.end())
            return false;
        auto vectorElement = dynamic_cast<MsgPack::Array*>(iter->second);
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
        std::cout << "vector " << name << " " << *this << std::endl;
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
    Vector prev, pos, next;
};

std::deque<Vertex> vertices;
float curveParam = -1.0;
Vector position;

Vector bezierPointAt(float param) {
    Vertex& prev = vertices[0];
    Vertex& next = vertices[1];
    float coParam = 1.0-param,
          paramSquared = param*param,
          coParamSquared = coParam*coParam;
    return prev.pos*(coParamSquared*coParam)+prev.next*(3.0*coParamSquared*param)+next.prev*(3.0*coParam*paramSquared)+next.pos*(param*paramSquared);
}

Vector bezierTangentAt(float param) {
    Vertex& prev = vertices[0];
    Vertex& next = vertices[1];
    float coParam = 1.0-param,
          paramSquared = param*param,
          coParamSquared = coParam*coParam;
    return ((prev.next-prev.pos)*coParamSquared+(next.prev-prev.next)*(2.0*coParam*param)+(next.pos-next.prev)*paramSquared); //.normalized();
}

float findParamAtDistance(float distance) {
    float lowerParam = curveParam, upperParam = 1.0;
    for(unsigned int i = 0; i < 16; ++i) {
        float middleParam = (lowerParam+upperParam)*0.5,
              distanceA = (bezierPointAt((lowerParam+middleParam)*0.5)-position).squaredLength(),
              distanceB = (bezierPointAt((middleParam+upperParam)*0.5)-position).squaredLength();
        if(abs(distanceA-distance) < abs(distanceB-distance))
            upperParam = middleParam;
        else
            lowerParam = middleParam;
    }
    return (lowerParam+upperParam)*0.5;
}

void resetCommand() {
    vertices.clear();
    curveParam = -1.0;
}

void releaseMotors() {
    resetCommand();
    for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
        motors[motorIndex]->setIdle(false);
}

void stopMotors() {
    resetCommand();
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
            releaseMotors();
            return;
        }
        motors[motorIndex]->updatePosition();
        position.coords[motorIndex] = motors[motorIndex]->getPositionInTurns();
    }
}

void writeMotors() {
    std::cout << "writeMotors " << vertices.size() << " " << curveParam << std::endl;
    if(vertices.empty()) {
        std::cout << "no vertices" << std::endl;
        stopMotors();
        return;
    }
    unsigned int targetIndex = (curveParam == -1.0 || vertices.size() == 1) ? 0 : 1;
    float speed = vertices[targetIndex].speed,
          endDistance = (vertices[targetIndex].pos-position).length();
    bool firstOrLast = (curveParam == -1.0 || vertices.size() <= 2);
    if(endDistance < 0.01) {
        std::cout << "reached vertex" << std::endl;
        if(curveParam != -1.0)
            vertices.pop_front();
        curveParam = (vertices.size() < 2) ? -1.0 : 0.0;
        if(firstOrLast)
            speed = 0;
        return;
    } else if(endDistance < 1.0) {
        float rushFactor = (firstOrLast)
            ? 0.0
            : (vertices[1].next-vertices[1].pos).dotNormalized(vertices[2].pos-vertices[2].prev);
        if(rushFactor != rushFactor || rushFactor < 0.1)
            rushFactor = 0.1;
        std::cout << "closing in on vertex " << rushFactor << std::endl;
        speed *= endDistance*endDistance*(3.0-2.0*endDistance)*(1.0-rushFactor)+rushFactor;
        std::cout << "speed " << speed << std::endl;
    }

    Vector targetPoint;
    if(targetIndex) {
        curveParam = findParamAtDistance(0.0);
        std::cout << "curveParam " << curveParam << std::endl;
        float targetParam = findParamAtDistance(1.0);
        targetPoint = bezierPointAt(targetParam);
        std::cout << "targetParam " << targetParam << std::endl;
    } else
        targetPoint = vertices[0].pos;

    Vector velocity = (targetPoint-position).normalized()*speed;
    for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex) {
        std::cout << motorIndex;
        std::cout << " v: " << velocity.coords[motorIndex];
        std::cout << " p: " << position.coords[motorIndex];
        std::cout << " t: " << target.coords[motorIndex] << std::endl;
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
        msgPackSocket << MsgPack::Factory("curveParam");
        msgPackSocket << MsgPack::Factory(curveParam);
        msgPackSocket << MsgPack::Factory("position");
        msgPackSocket << MsgPack__Factory(ArrayHeader(motorCount));
        for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
            msgPackSocket << MsgPack::Factory(motors[motorIndex]->getPositionInTurns());
    }
}

void onExit(int sig) {
    releaseMotors();
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
    releaseMotors();

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
            stopMotors();
            vertices.resize(verticesVector->size());
            for(size_t vertexIndex = 0; vertexIndex < verticesVector->size(); ++vertexIndex) {
                Vertex& vertex = vertices[vertexIndex];
                std::cout << "vertexIndex " << vertexIndex << std::endl;
                auto vertexElement = dynamic_cast<MsgPack::Map*>((*verticesVector)[vertexIndex].get());
                auto vertexMap = vertexElement->getElementsMap();
                iter = vertexMap.find("speed");
                if(iter == vertexMap.end())
                    return;
                auto scalarElement = dynamic_cast<MsgPack::Number*>(iter->second);
                if(!scalarElement)
                    return;
                vertex.speed = scalarElement->getValue<float>();
                vertex.prev.parseMsgPack(vertexMap, "prev");
                vertex.pos.parseMsgPack(vertexMap, "pos");
                vertex.next.parseMsgPack(vertexMap, "next");
            }
            return;
        } else if(type == "release") {
            releaseMotors();
            return;
        } else if(type == "stop") {
            stopMotors();
            return;
        } else if(type == "origin") {
            if(curveParam >= 0.0)
                return;
            for(size_t motorIndex = 0; motorIndex < motorCount; ++motorIndex)
                motors[motorIndex]->resetHome();
            return;
        }
        std::function<bool(L6470*)> command;
        if(type == "run") {
            if(curveParam >= 0.0)
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
