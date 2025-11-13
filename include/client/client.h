#pragma once

#include "common/packet.h"
#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace net {

class Client {
public:
  using PacketCallback = std::function<void(const Packet &)>;

  Client();
  ~Client();

  bool connect(const std::string &serverAddress, uint16_t port);

  void disconnect();

  bool isConnected() const { return connected_; }

  bool sendPacket(const Packet &packet);

  bool receivePacket(Packet &packet);

  bool tryReceivePacket(Packet &packet);

  void setPacketCallback(PacketCallback callback);

  void startReceiving();

  void stopReceiving();

private:
  SOCKET socket_;

  std::atomic<bool> connected_;

  std::atomic<bool> receiving_;
  std::thread receivingThread_;

  PacketCallback packetCallback_;

  static constexpr size_t BUFFER_SIZE = 4096;

  std::vector<uint8_t> packetBuffer_;

  bool initializeWinsock();

  void cleanupWinsock();

  void receivingThread();
};

} // namespace net