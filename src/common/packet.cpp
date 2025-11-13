#include "common/packet.h"
#include <algorithm>
#include <cstring>
#include <winsock2.h>

namespace net {

Packet::Packet() : header_(0, 0) {}

Packet::Packet(uint16_t type, const std::vector<uint8_t> &data)
    : header_(0, type), data_(data) {
  updateHeader();
}

Packet::Packet(uint16_t type, const std::string &data) : header_(0, type) {
  data_.resize(data.size());
  std::copy(data.begin(), data.end(), data_.begin());
  updateHeader();
}

void Packet::setType(uint16_t type) { header_.type = type; }

void Packet::setData(const std::vector<uint8_t> &data) {
  data_ = data;
  updateHeader();
}

void Packet::setData(const std::string &data) {
  data_.resize(data.size());
  std::copy(data.begin(), data.end(), data_.begin());
  updateHeader();
}

void Packet::updateHeader() {
  // Total size = header size + data size
  header_.length = static_cast<uint32_t>(PacketHeader::SIZE + data_.size());
}

std::vector<uint8_t> Packet::serialize() const {
  std::vector<uint8_t> buffer;
  buffer.reserve(getTotalSize());

  // Write header (network byte order - big endian)
  uint32_t lengthBE = htonl(header_.length);
  uint16_t typeBE = htons(header_.type);

  buffer.insert(buffer.end(), reinterpret_cast<const uint8_t *>(&lengthBE),
                reinterpret_cast<const uint8_t *>(&lengthBE) +
                    sizeof(uint32_t));
  buffer.insert(buffer.end(), reinterpret_cast<const uint8_t *>(&typeBE),
                reinterpret_cast<const uint8_t *>(&typeBE) + sizeof(uint16_t));

  // Write data
  buffer.insert(buffer.end(), data_.begin(), data_.end());

  return buffer;
}

Packet Packet::deserialize(const std::vector<uint8_t> &buffer) {
  return deserialize(buffer.data(), buffer.size());
}

Packet Packet::deserialize(const uint8_t *buffer, size_t size) {
  Packet packet;

  // Check minimum size
  if (size < PacketHeader::SIZE)
    return packet;

  // Read header (convert from network byte order)
  uint32_t lengthBE;
  uint16_t typeBE;
  std::memcpy(&lengthBE, buffer, sizeof(uint32_t));
  std::memcpy(&typeBE, buffer + sizeof(uint32_t), sizeof(uint16_t));

  packet.header_.length = ntohl(lengthBE);
  packet.header_.type = ntohs(typeBE);

  // Validate header length
  if (packet.header_.length < PacketHeader::SIZE ||
      packet.header_.length > size)
    return packet;

  // Read data
  size_t dataSize = packet.header_.length - PacketHeader::SIZE;
  packet.data_.resize(dataSize);
  std::memcpy(packet.data_.data(), buffer + PacketHeader::SIZE, dataSize);

  return packet;
}

bool Packet::isCompletePacket(const uint8_t *buffer, size_t size) {
  // Need at least header size to read length
  if (size < PacketHeader::SIZE)
    return false;

  // Read length from header
  uint32_t lengthBE;
  std::memcpy(&lengthBE, buffer, sizeof(uint32_t));
  uint32_t length = ntohl(lengthBE);

  // Check if we have the complete packet
  return length <= size;
}

} // namespace net