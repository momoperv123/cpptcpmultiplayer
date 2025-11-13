#pragma once

#include "common/game_state.h"
#include "common/packet.h"
#include <cstring>
#include <vector>
#include <winsock2.h>

namespace net {

struct ChatMessage {
  uint32_t senderId;
  std::string senderUsername;
  std::string senderMessage;

  ChatMessage() : senderId(0) {}
  ChatMessage(uint32_t id, const std::string &username,
              const std::string &message)
      : senderId(id), senderUsername(username), senderMessage(message) {}
};

struct RoleAssignment {
  uint32_t playerId;
  PlayerRole role;
  std::string topic;      // Liar sees this
  std::string secretWord; // Guessers see this

  RoleAssignment() : playerId(0), role(PlayerRole::NONE) {}
  RoleAssignment(uint32_t id, PlayerRole r, const std::string &t,
                 const std::string &w)
      : playerId(id), role(r), topic(t), secretWord(w) {}
};

std::vector<uint8_t> serializePlayerState(const PlayerState &state);

PlayerState deserializePlayerState(const uint8_t *data, size_t size);

Packet createPlayerStatePacket(uint16_t type, const PlayerState &state);

Packet createGameStateUpdatePacket(const std::vector<PlayerState> &states);

PlayerState extractPlayerState(const Packet &packet);

std::vector<PlayerState> extractGameStateUpdate(const Packet &packet);

std::vector<uint8_t> serializeChatMessage(const ChatMessage &message);

ChatMessage deserializeChatMessage(const uint8_t *data, size_t size);

Packet createChatMessagePacket(const ChatMessage &message);

ChatMessage extractChatMessage(const Packet &packet);

std::vector<uint8_t> serializeRoleAssignment(const RoleAssignment &assignment);
RoleAssignment deserializeRoleAssignment(const uint8_t *data, size_t size);
Packet createRoleAssignmentPacket(const RoleAssignment &assignment);
RoleAssignment extractRoleAssignment(const Packet &packet);

struct VoteCommand {
  uint32_t voterId;
  uint32_t targetId;

  VoteCommand() : voterId(0), targetId(0) {}
  VoteCommand(uint32_t voter, uint32_t target)
      : voterId(voter), targetId(target) {}
};

struct VoteResult {
  std::unordered_map<uint32_t, uint32_t> tally; // targetId -> vote count
  uint32_t winnerId;                            // 0 if no majority
  bool liarCaught;

  VoteResult() : winnerId(0), liarCaught(false) {}
};

std::vector<uint8_t> serializeVoteCommand(const VoteCommand &vote);
VoteCommand deserializeVoteCommand(const uint8_t *data, size_t size);
Packet createVoteCommandPacket(const VoteCommand &vote);
VoteCommand extractVoteCommand(const Packet &packet);

std::vector<uint8_t> serializeVoteResult(const VoteResult &result);
VoteResult deserializeVoteResult(const uint8_t *data, size_t size);
Packet createVoteResultPacket(const VoteResult &result);
VoteResult extractVoteResult(const Packet &packet);

} // namespace net