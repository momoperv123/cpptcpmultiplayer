#include "server/message_queue.h"

namespace net {

void MessageQueue::push(const Packet &packet) {
  std::lock_guard<std::mutex> lock(mutex_);
  queue_.push(packet);
  condition_.notify_one();
}

Packet MessageQueue::pop() {
  std::unique_lock<std::mutex> lock(mutex_);

  condition_.wait(lock, [this] { return !queue_.empty(); });

  Packet packet = queue_.front();
  queue_.pop();
  return packet;
}

bool MessageQueue::tryPop(Packet &packet) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (queue_.empty())
    return false;

  packet = queue_.front();
  queue_.pop();
  return true;
}

bool MessageQueue::empty() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return queue_.empty();
}

size_t MessageQueue::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return queue_.size();
}

void MessageQueue::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::queue<Packet> empty;
  queue_.swap(empty);
}

void MessageQueue::notifyAll() { condition_.notify_all(); }

} // namespace net