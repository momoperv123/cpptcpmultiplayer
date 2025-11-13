#pragma once

#include "common/packet.h"
#include <condition_variable>
#include <mutex>
#include <queue>

namespace net {

class MessageQueue {
public:
  MessageQueue() = default;
  ~MessageQueue() = default;

  void push(const Packet &packet);

  Packet pop();

  bool tryPop(Packet &packet);

  bool empty() const;

  size_t size() const;

  void clear();

  void notifyAll();

private:
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::queue<Packet> queue_;
};

} // namespace net