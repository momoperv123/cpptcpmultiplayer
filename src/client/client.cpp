#include "client/client.h"
#include <iostream>
#include <thread>

namespace net {

Client::Client()
    : socket_(INVALID_SOCKET), connected_(false), receiving_(false) {}

Client::~Client() { disconnect(); }

bool Client::initializeWinsock() {
  WSADATA wsaData;
  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    std::cerr << "WSAStartup failed: " << result << std::endl;
    return false;
  }
  return true;
}

void Client::cleanupWinsock() { WSACleanup(); }

bool Client::connect(const std::string &serverAddress, uint16_t port) {
  if (connected_)
    return false;

  if (!initializeWinsock())
    return false;

  socket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_ == INVALID_SOCKET) {
    std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
    cleanupWinsock();
    return false;
  }

  sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(port);

  if (inet_pton(AF_INET, serverAddress.c_str(), &serverAddr.sin_addr) <= 0) {
    std::cerr << "Invalid server address: " << serverAddress << std::endl;
    closesocket(socket_);
    cleanupWinsock();
    return false;
  }

  if (::connect(socket_, (sockaddr *)&serverAddr, sizeof(serverAddr)) ==
      SOCKET_ERROR) {
    std::cerr << "Connection failed: " << WSAGetLastError() << std::endl;
    closesocket(socket_);
    cleanupWinsock();
    return false;
  }

  connected_ = true;
  std::cout << "Connected to server at " << serverAddress << ":" << port
            << std::endl;

  return true;
}

void Client::disconnect() {
  if (!connected_)
    return;

  stopReceiving();

  connected_ = false;

  if (socket_ != INVALID_SOCKET) {
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
  }

  cleanupWinsock();
  std::cout << "Disconnected from server" << std::endl;
}

bool Client::sendPacket(const Packet &packet) {
  if (!connected_) {
    std::cerr << "sendPacket failed: not connected" << std::endl;
    return false;
  }

  std::vector<uint8_t> data = packet.serialize();

  int bytesSent = send(socket_, reinterpret_cast<const char *>(data.data()),
                       static_cast<int>(data.size()), 0);

  if (bytesSent == SOCKET_ERROR) {
    int error = WSAGetLastError();
    std::cerr
        << "sendPacket failed: send() returned SOCKET_ERROR, WSAGetLastError: "
        << error << std::endl;
    return false;
  }

  if (bytesSent != static_cast<int>(data.size())) {
    std::cerr << "sendPacket warning: only sent " << bytesSent << " of "
              << data.size() << " bytes" << std::endl;
  }

  return true;
}

bool Client::receivePacket(Packet &packet) {
  if (!connected_)
    return false;

  std::vector<uint8_t> receiveBuffer;
  receiveBuffer.resize(BUFFER_SIZE);

  while (connected_) {
    // Check if we have a complete packet in the buffer
    if (!packetBuffer_.empty() &&
        Packet::isCompletePacket(packetBuffer_.data(), packetBuffer_.size())) {
      packet = Packet::deserialize(packetBuffer_.data(), packetBuffer_.size());
      size_t packetSize = packet.getTotalSize();

      // Remove the processed packet from the buffer
      packetBuffer_.erase(packetBuffer_.begin(),
                          packetBuffer_.begin() + packetSize);

      return packet.getType() != 0;
    }

    int bytesReceived =
        recv(socket_, reinterpret_cast<char *>(receiveBuffer.data()),
             BUFFER_SIZE, 0);

    if (bytesReceived > 0) {
      packetBuffer_.insert(packetBuffer_.end(), receiveBuffer.data(),
                           receiveBuffer.data() + bytesReceived);

      if (Packet::isCompletePacket(packetBuffer_.data(),
                                   packetBuffer_.size())) {
        packet =
            Packet::deserialize(packetBuffer_.data(), packetBuffer_.size());
        size_t packetSize = packet.getTotalSize();

        // Remove the processed packet from the buffer
        packetBuffer_.erase(packetBuffer_.begin(),
                            packetBuffer_.begin() + packetSize);

        return packet.getType() != 0;
      }
    } else if (bytesReceived == 0) {
      connected_ = false;
      return false;
    } else {
      int error = WSAGetLastError();
      if (error != WSAECONNRESET)
        std::cerr << "Receive failed: " << error << std::endl;
      connected_ = false;
      return false;
    }
  }

  return false;
}

bool Client::tryReceivePacket(Packet &packet) {
  if (!connected_)
    return false;

  if (!packetBuffer_.empty() &&
      Packet::isCompletePacket(packetBuffer_.data(), packetBuffer_.size())) {
    packet = Packet::deserialize(packetBuffer_.data(), packetBuffer_.size());
    size_t packetSize = packet.getTotalSize();
    packetBuffer_.erase(packetBuffer_.begin(),
                        packetBuffer_.begin() + packetSize);
    return packet.getType() != 0;
  }

  // Try to receive without blocking (non-blocking socket would be needed)
  return false;
}

void Client::setPacketCallback(PacketCallback callback) {
  packetCallback_ = callback;
}

void Client::startReceiving() {
  if (receiving_ || !connected_)
    return;

  receiving_ = true;
  receivingThread_ = std::thread(&Client::receivingThread, this);
}

void Client::stopReceiving() {
  if (!receiving_)
    return;

  receiving_ = false;

  if (receivingThread_.joinable()) {
    receivingThread_.join();
  }
}

void Client::receivingThread() {
  Packet packet;
  while (receiving_ && connected_) {
    if (receivePacket(packet)) {
      if (packetCallback_)
        packetCallback_(packet);
    }
  }
}

} // namespace net