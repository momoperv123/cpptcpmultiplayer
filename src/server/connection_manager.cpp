#include "server/connection_manager.h"
#include <algorithm>

namespace net {

bool ConnectionManager::addConnection(uint32_t id, SOCKET socket,
                                      const sockaddr_in &address) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (connections_.find(id) != connections_.end())
    return false;

  auto info = std::make_shared<ConnectionInfo>(id, socket, address);
  connections_[id] = info;
  return true;
}

bool ConnectionManager::removeConnection(uint32_t id) {
  std::lock_guard<std::mutex> lock(mutex_);
  return connections_.erase(id) > 0;
}

std::shared_ptr<ConnectionInfo>
ConnectionManager::getConnection(uint32_t id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = connections_.find(id);
  return it != connections_.end() ? it->second : nullptr;
}

bool ConnectionManager::hasConnection(uint32_t id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return connections_.find(id) != connections_.end();
}

bool ConnectionManager::setStatus(uint32_t id, ConnectionStatus status) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = connections_.find(id);
  if (it != connections_.end()) {
    it->second->status = status;
    return true;
  }
  return false;
}

bool ConnectionManager::setUsername(uint32_t id, const std::string &username) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = connections_.find(id);
  if (it != connections_.end()) {
    it->second->username = username;
    return true;
  }
  return false;
}

bool ConnectionManager::updateHeartbeat(uint32_t id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = connections_.find(id);
  if (it != connections_.end()) {
    it->second->lastHeartbeat = std::chrono::steady_clock::now();
    return true;
  }

  return false;
}

std::vector<uint32_t> ConnectionManager::getActiveConnections() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<uint32_t> ids;
  ids.reserve(connections_.size());

  for (const auto &[id, info] : connections_) {
    if (info->status == ConnectionStatus::ACTIVE)
      ids.push_back(id);
  }

  return ids;
}

std::vector<std::shared_ptr<ConnectionInfo>>
ConnectionManager::getAllConnections() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::shared_ptr<ConnectionInfo>> result;
  result.reserve(connections_.size());

  for (const auto &pair : connections_)
    result.push_back(pair.second);
  return result;
}

size_t ConnectionManager::getConnectionCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return connections_.size();
}

size_t ConnectionManager::getActiveConnectionCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  size_t count = 0;
  for (const auto &pair : connections_) {
    if (pair.second->status == ConnectionStatus::ACTIVE)
      count++;
  }
  return count;
}

std::shared_ptr<ConnectionInfo>
ConnectionManager::findConnectionByUsername(const std::string &username) const {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto &pair : connections_) {
    if (pair.second->username == username)
      return pair.second;
  }

  return nullptr;
}

void ConnectionManager::cleanupInactiveConnections() {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = connections_.begin();

  while (it != connections_.end()) {
    if (it->second->status == ConnectionStatus::DISCONNECTING)
      it = connections_.erase(it);
    else
      ++it;
  }
}

void ConnectionManager::clearAllConnections() {
  std::lock_guard<std::mutex> lock(mutex_);
  connections_.clear();
}

} // namespace net