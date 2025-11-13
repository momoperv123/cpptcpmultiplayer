#include "server/server.h"
#include <algorithm>
#include <iostream>

namespace net {

Server::Server(uint16_t port)
    : port_(port), serverSocket_(INVALID_SOCKET), running_(false),
      nextClientId_(1) {}

Server::~Server() { stop(); }

bool Server::initializeWinsock() {
  WSADATA wsaData;
  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    std::cerr << "WSAStartup failed: " << result << std::endl;
    return false;
  }
  return true;
}

void Server::cleanupWinsock() { WSACleanup(); }

bool Server::start() {
  if (running_)
    return false;
  if (!initializeWinsock())
    return false;

  serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket_ == INVALID_SOCKET) {
    std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
    cleanupWinsock();
    return false;
  }

  sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(port_);

  if (bind(serverSocket_, (sockaddr *)&serverAddr, sizeof(serverAddr)) ==
      SOCKET_ERROR) {
    std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
    closesocket(serverSocket_);
    cleanupWinsock();
    return false;
  }

  if (listen(serverSocket_, SOMAXCONN) == SOCKET_ERROR) {
    std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
    closesocket(serverSocket_);
    cleanupWinsock();
    return false;
  }

  running_ = true;
  std::cout << "Server is listening on port " << port_ << std::endl;

  while (running_) {
    sockaddr_in clientAddr{};
    int clientAddrSize = sizeof(clientAddr);
    SOCKET clientSocket =
        accept(serverSocket_, (sockaddr *)&clientAddr, &clientAddrSize);

    if (clientSocket == INVALID_SOCKET) {
      if (running_)
        std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
      continue;
    }

    uint32_t clientId = nextClientId_++;

    if (!connectionManager_.addConnection(clientId, clientSocket, clientAddr)) {
      std::cerr << "Failed to add client to connection manager" << std::endl;
      closesocket(clientSocket);
      continue;
    }

    connectionManager_.setStatus(clientId, ConnectionStatus::ACTIVE);

    auto client =
        std::make_shared<ClientConnection>(clientId, clientSocket, clientAddr);

    {
      std::lock_guard<std::mutex> lock(clientThreadsMutex_);
      clientThreads_.push_back(client);
    }

    client->thread = std::thread(&Server::handleClient, this, client);

    std::cout << "Client connected from " << inet_ntoa(clientAddr.sin_addr)
              << ":" << ntohs(clientAddr.sin_port)
              << " [Thread ID: " << std::this_thread::get_id() << "]"
              << std::endl;
  }

  return true;
}

void Server::stop() {
  if (!running_)
    return;
  running_ = false;

  if (serverSocket_ != INVALID_SOCKET) {
    closesocket(serverSocket_);
    serverSocket_ = INVALID_SOCKET;
  }

  auto allConnections = connectionManager_.getAllConnections();
  for (const auto &conn : allConnections) {
    connectionManager_.setStatus(conn->id, ConnectionStatus::DISCONNECTING);
    closesocket(conn->socket);
  }

  {
    std::lock_guard<std::mutex> lock(clientThreadsMutex_);
    for (auto &thread : clientThreads_) {
      if (thread->thread.joinable())
        thread->thread.join();
    }
    clientThreads_.clear();
  }

  connectionManager_.clearAllConnections();
  cleanupWinsock();
  std::cout << "Server shutdown complete" << std::endl;
}

bool Server::sendPacket(uint32_t clientId, const Packet &packet) {
  auto connInfo = connectionManager_.getConnection(clientId);

  if (!connInfo || connInfo->status != ConnectionStatus::ACTIVE)
    return false;

  std::vector<uint8_t> data = packet.serialize();
  int result =
      send(connInfo->socket, reinterpret_cast<const char *>(data.data()),
           static_cast<int>(data.size()), 0);

  if (result == SOCKET_ERROR) {
    int error = WSAGetLastError();
    if (error == WSAECONNRESET || error == WSAENOTCONN)
      disconnectClient(clientId);
    return false;
  }

  return result == static_cast<int>(data.size());
}

void Server::broadcast(const Packet &packet) {
  auto activeConnections = connectionManager_.getActiveConnections();
  for (uint32_t connId : activeConnections)
    sendPacket(connId, packet);
}

void Server::broadcastExcept(uint32_t excludeClientId, const Packet &packet) {
  auto activeConnections = connectionManager_.getActiveConnections();
  for (uint32_t connId : activeConnections) {
    if (connId != excludeClientId)
      sendPacket(connId, packet);
  }
}

size_t Server::getConnectionCount() const {
  return connectionManager_.getConnectionCount();
}

void Server::setPacketCallback(PacketCallback callback) {
  packetCallback_ = callback;
}

bool Server::disconnectClient(uint32_t clientId) {
  auto connInfo = connectionManager_.getConnection(clientId);
  if (!connInfo)
    return false;

  connectionManager_.setStatus(clientId, ConnectionStatus::DISCONNECTING);
  closesocket(connInfo->socket);

  {
    std::lock_guard<std::mutex> lock(clientThreadsMutex_);
    for (auto &thread : clientThreads_) {
      if (thread->id == clientId) {
        thread->active = false;
        break;
      }
    }
  }

  return true;
}

void Server::cleanupConnections() {
  connectionManager_.cleanupInactiveConnections();

  std::lock_guard<std::mutex> lock(clientThreadsMutex_);
  clientThreads_.erase(
      std::remove_if(clientThreads_.begin(), clientThreads_.end(),
                     [](const std::shared_ptr<ClientConnection> &c) {
                       return !c->active;
                     }),
      clientThreads_.end());
}

void Server::handleClient(std::shared_ptr<ClientConnection> client) {
  std::vector<uint8_t> receiveBuffer;
  receiveBuffer.resize(BUFFER_SIZE);

  // Buffer for incomplete packets
  std::vector<uint8_t> packetBuffer;

  while (running_ && client->active) {
    int bytesReceived =
        recv(client->socket, reinterpret_cast<char *>(receiveBuffer.data()),
             BUFFER_SIZE, 0);

    if (bytesReceived > 0) {
      connectionManager_.updateHeartbeat(client->id);
      packetBuffer.insert(packetBuffer.end(), receiveBuffer.data(),
                          receiveBuffer.data() + bytesReceived);
      // Process received data - handle partial packets
      size_t offset = 0;
      while (offset < packetBuffer.size()) {
        if (!Packet::isCompletePacket(packetBuffer.data() + offset,
                                      packetBuffer.size() - offset))
          break;

        Packet packet = Packet::deserialize(packetBuffer.data() + offset,
                                            packetBuffer.size() - offset);

        if (packet.getType() != 0) {
          messageQueue_.push(packet);

          if (packetCallback_) {
            packetCallback_(packet, client->id);
          } else {
            std::cerr
                << "Warning: Packet received but no callback set [Client: "
                << client->id << ", Type: " << packet.getType() << "]"
                << std::endl;
          }
        }

        offset += packet.getTotalSize();
      }

      if (offset > 0)
        packetBuffer.erase(packetBuffer.begin(), packetBuffer.begin() + offset);

    } else if (bytesReceived == 0) {
      std::cout << "Client disconnected [ID: " << client->id << "]"
                << std::endl;
      break;
    } else {
      int error = WSAGetLastError();
      if (error != WSAECONNRESET && error != WSAEWOULDBLOCK) {
        std::cerr << "Receive failed [ID: " << client->id << "]: " << error
                  << std::endl;
      }
      if (error == WSAECONNRESET) {
        std::cout << "Connection reset by client [ID: " << client->id << "]"
                  << std::endl;
      }
      break;
    }
  }

  closesocket(client->socket);
  client->active = false;

  connectionManager_.setStatus(client->id, ConnectionStatus::DISCONNECTING);
  connectionManager_.removeConnection(client->id);

  if (packetCallback_) {
    Packet leavePacket(MessageType::PLAYER_LEAVE, std::vector<uint8_t>());
    packetCallback_(leavePacket, client->id);
  }
}

} // namespace net