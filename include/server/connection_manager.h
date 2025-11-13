#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace net {

enum class ConnectionStatus { CONNECTING, ACTIVE, IDLE, DISCONNECTING };

struct ConnectionInfo {
  uint32_t id;
  SOCKET socket;
  sockaddr_in address;
  std::string username;
  ConnectionStatus status;
  std::chrono::steady_clock::time_point lastHeartbeat;
  std::chrono::steady_clock::time_point connectedAt;

  ConnectionInfo(uint32_t id, SOCKET sock, const sockaddr_in &addr)
      : id(id), socket(sock), address(addr),
        status(ConnectionStatus::CONNECTING),
        lastHeartbeat(std::chrono::steady_clock::now()),
        connectedAt(std::chrono::steady_clock::now()) {}
};

class ConnectionManager {
public:
  ConnectionManager() = default;
  ~ConnectionManager() = default;

  bool addConnection(uint32_t id, SOCKET socket, const sockaddr_in &address);

  bool removeConnection(uint32_t id);

  std::shared_ptr<ConnectionInfo> getConnection(uint32_t id) const;

  bool hasConnection(uint32_t id) const;

  bool setStatus(uint32_t id, ConnectionStatus status);

  bool setUsername(uint32_t id, const std::string &username);

  bool updateHeartbeat(uint32_t id);

  std::vector<uint32_t> getActiveConnections() const;

  std::vector<std::shared_ptr<ConnectionInfo>> getAllConnections() const;

  size_t getConnectionCount() const;

  size_t getActiveConnectionCount() const;

  std::shared_ptr<ConnectionInfo>
  findConnectionByUsername(const std::string &username) const;

  void cleanupInactiveConnections();

  void clearAllConnections();

private:
  mutable std::mutex mutex_;
  std::unordered_map<uint32_t, std::shared_ptr<ConnectionInfo>> connections_;
};

} // namespace net
