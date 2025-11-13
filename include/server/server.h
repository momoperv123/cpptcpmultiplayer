#pragma once

#include "common/packet.h"
#include "server/connection_manager.h"
#include "server/message_queue.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace net {

struct ClientConnection {
  uint32_t id;
  SOCKET socket;
  sockaddr_in address;
  std::thread thread;
  bool active;

  ClientConnection(uint32_t id, SOCKET sock, const sockaddr_in &addr)
      : id(id), socket(sock), address(addr), active(true) {}
};

class Server {
public:
  using PacketCallback = std::function<void(const Packet &, uint32_t clientId)>;

  Server(uint16_t port = 8000);
  ~Server();

  bool start();
  void stop();
  bool isRunning() const { return running_; }

  bool sendPacket(uint32_t clientId, const Packet &packet);
  void broadcast(const Packet &packet);
  void broadcastExcept(uint32_t excludeClientId, const Packet &packet);

  size_t getConnectionCount() const;
  void setPacketCallback(PacketCallback callback);
  bool disconnectClient(uint32_t clientId);

  MessageQueue &getMessageQueue() { return messageQueue_; }

  ConnectionManager &getConnectionManager() { return connectionManager_; }

private:
  uint16_t port_;
  SOCKET serverSocket_;
  std::atomic<bool> running_;
  std::atomic<uint32_t> nextClientId_;

  ConnectionManager connectionManager_;

  std::vector<std::shared_ptr<ClientConnection>> clientThreads_;
  mutable std::mutex clientThreadsMutex_;

  MessageQueue messageQueue_;
  PacketCallback packetCallback_;

  static constexpr size_t BUFFER_SIZE = 4096;

  void handleClient(std::shared_ptr<ClientConnection> client);
  void cleanupConnections();
  bool initializeWinsock();
  void cleanupWinsock();
};

} // namespace net