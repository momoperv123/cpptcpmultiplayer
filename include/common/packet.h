#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace MessageType {

// Echo/Chat types
constexpr uint16_t ECHO = 1;
constexpr uint16_t CHAT = 2;
constexpr uint16_t DISCONNECT = 3;
constexpr uint16_t HEARTBEAT = 4;

// Game-specific types
constexpr uint16_t PLAYER_JOIN = 10;
constexpr uint16_t PLAYER_LEAVE = 13;
constexpr uint16_t GAME_STATE_UPDATE = 14;
constexpr uint16_t PLAYER_JOINED = 15;
constexpr uint16_t CHAT_MESSAGE = 16;
constexpr uint16_t CHAT_BROADCAST = 17;
constexpr uint16_t ROUND_START = 18;
constexpr uint16_t ROLE_ASSIGNMENT = 19;
constexpr uint16_t VOTE_COMMAND = 21;
constexpr uint16_t VOTE_RESULT = 22;

} // namespace MessageType

namespace net {

struct PacketHeader {
  uint32_t length;
  uint16_t type;

  PacketHeader() : length(0), type(0) {}
  PacketHeader(uint32_t len, uint16_t t) : length(len), type(t) {}

  static constexpr size_t SIZE = sizeof(uint32_t) + sizeof(uint16_t);
};

class Packet {
public:
  Packet();
  Packet(uint16_t type, const std::vector<uint8_t> &data);
  Packet(uint16_t type, const std::string &data);

  uint16_t getType() const { return header_.type; }
  const std::vector<uint8_t> &getData() const { return data_; }
  PacketHeader getHeader() const { return header_; }
  size_t getTotalSize() const { return PacketHeader::SIZE + data_.size(); }

  void setType(uint16_t type);
  void setData(const std::vector<uint8_t> &data);
  void setData(const std::string &data);

  std::vector<uint8_t> serialize() const;

  static Packet deserialize(const std::vector<uint8_t> &buffer);
  static Packet deserialize(const uint8_t *buffer, size_t size);

  static bool isCompletePacket(const uint8_t *buffer, size_t size);

private:
  PacketHeader header_;
  std::vector<uint8_t> data_;

  void updateHeader();
};

} // namespace net