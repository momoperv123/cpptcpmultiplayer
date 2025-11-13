#include "common/serialization.h"
#include <cstring>
#include <winsock2.h>

namespace net {

std::vector<uint8_t> serializePlayerState(const PlayerState &state) {
  std::vector<uint8_t> data;

  size_t usernameSize = state.username.size();
  data.reserve(sizeof(uint32_t) * 4 + usernameSize);

  uint32_t idBE = htonl(state.id);
  data.insert(data.end(), reinterpret_cast<const uint8_t *>(&idBE),
              reinterpret_cast<const uint8_t *>(&idBE) + sizeof(uint32_t));

  uint32_t roleBE = htonl(static_cast<uint32_t>(state.role));
  data.insert(data.end(), reinterpret_cast<const uint8_t *>(&roleBE),
              reinterpret_cast<const uint8_t *>(&roleBE) + sizeof(uint32_t));

  uint32_t scoreBE = htonl(static_cast<uint32_t>(state.score));
  data.insert(data.end(), reinterpret_cast<const uint8_t *>(&scoreBE),
              reinterpret_cast<const uint8_t *>(&scoreBE) + sizeof(uint32_t));

  uint32_t usernameLenBE = htonl(static_cast<uint32_t>(usernameSize));
  data.insert(data.end(), reinterpret_cast<const uint8_t *>(&usernameLenBE),
              reinterpret_cast<const uint8_t *>(&usernameLenBE) +
                  sizeof(uint32_t));

  if (usernameSize > 0)
    data.insert(data.end(), state.username.begin(), state.username.end());

  return data;
}

PlayerState deserializePlayerState(const uint8_t *data, size_t size) {
  PlayerState state;

  if (size < sizeof(uint32_t) * 4)
    return state;

  size_t offset = 0;

  uint32_t idBE;
  std::memcpy(&idBE, data + offset, sizeof(uint32_t));
  state.id = ntohl(idBE);
  offset += sizeof(uint32_t);

  uint32_t roleBE;
  std::memcpy(&roleBE, data + offset, sizeof(uint32_t));
  state.role = static_cast<PlayerRole>(ntohl(roleBE));
  offset += sizeof(uint32_t);

  uint32_t scoreBE;
  std::memcpy(&scoreBE, data + offset, sizeof(uint32_t));
  state.score = static_cast<int>(ntohl(scoreBE));
  offset += sizeof(uint32_t);

  if (size < offset + sizeof(uint32_t))
    return state;

  uint32_t usernameLenBE;
  std::memcpy(&usernameLenBE, data + offset, sizeof(uint32_t));
  size_t usernameLen = ntohl(usernameLenBE);
  offset += sizeof(uint32_t);

  if (size >= offset + usernameLen && usernameLen > 0)
    state.username =
        std::string(reinterpret_cast<const char *>(data + offset), usernameLen);

  return state;
}

Packet createPlayerStatePacket(uint16_t type, const PlayerState &state) {
  auto data = serializePlayerState(state);
  return Packet(type, data);
}

Packet createGameStateUpdatePacket(const std::vector<PlayerState> &states) {
  std::vector<uint8_t> data;

  uint32_t count = static_cast<uint32_t>(states.size());
  uint32_t countBE = htonl(count);
  data.insert(data.end(), reinterpret_cast<const uint8_t *>(&countBE),
              reinterpret_cast<const uint8_t *>(&countBE) + sizeof(uint32_t));

  for (const auto &state : states) {
    auto stateData = serializePlayerState(state);
    data.insert(data.end(), stateData.begin(), stateData.end());
  }

  return Packet(MessageType::GAME_STATE_UPDATE, data);
}

PlayerState extractPlayerState(const Packet &packet) {
  return deserializePlayerState(packet.getData().data(),
                                packet.getData().size());
}

std::vector<PlayerState> extractGameStateUpdate(const Packet &packet) {
  std::vector<PlayerState> states;
  const auto &data = packet.getData();

  if (data.size() < sizeof(uint32_t))
    return states;

  uint32_t countBE;
  std::memcpy(&countBE, data.data(), sizeof(uint32_t));
  uint32_t count = ntohl(countBE);
  size_t offset = sizeof(uint32_t);

  for (uint32_t i = 0; i < count; ++i) {
    if (data.size() < offset + sizeof(uint32_t) * 4)
      break;

    uint32_t usernameLenBE;
    if (data.size() < offset + sizeof(uint32_t) * 4)
      break;
    std::memcpy(&usernameLenBE, data.data() + offset + sizeof(uint32_t) * 3,
                sizeof(uint32_t));
    uint32_t usernameLen = ntohl(usernameLenBE);

    size_t playerSize = sizeof(uint32_t) * 4 + usernameLen;
    if (data.size() < offset + playerSize)
      break;

    PlayerState state =
        deserializePlayerState(data.data() + offset, playerSize);
    states.push_back(state);
    offset += playerSize;
  }

  return states;
}

std::vector<uint8_t> serializeChatMessage(const ChatMessage &message) {
  std::vector<uint8_t> data;

  size_t usernameSize = message.senderUsername.size();
  size_t messageSize = message.senderMessage.size();
  data.reserve(sizeof(uint32_t) * 3 + usernameSize + messageSize);

  uint32_t senderIdBE = htonl(message.senderId);
  data.insert(data.end(), reinterpret_cast<const uint8_t *>(&senderIdBE),
              reinterpret_cast<const uint8_t *>(&senderIdBE) +
                  sizeof(uint32_t));

  uint32_t usernameLenBE = htonl(static_cast<uint32_t>(usernameSize));
  data.insert(data.end(), reinterpret_cast<const uint8_t *>(&usernameLenBE),
              reinterpret_cast<const uint8_t *>(&usernameLenBE) +
                  sizeof(uint32_t));

  if (usernameSize > 0)
    data.insert(data.end(), message.senderUsername.begin(),
                message.senderUsername.end());

  uint32_t messageLenBE = htonl(static_cast<uint32_t>(messageSize));
  data.insert(data.end(), reinterpret_cast<const uint8_t *>(&messageLenBE),
              reinterpret_cast<const uint8_t *>(&messageLenBE) +
                  sizeof(uint32_t));

  if (messageSize > 0)
    data.insert(data.end(), message.senderMessage.begin(),
                message.senderMessage.end());

  return data;
}

ChatMessage deserializeChatMessage(const uint8_t *data, size_t size) {
  ChatMessage message;

  if (size < sizeof(uint32_t) * 3)
    return message;

  size_t offset = 0;

  uint32_t senderIdBE;
  std::memcpy(&senderIdBE, data + offset, sizeof(uint32_t));
  message.senderId = ntohl(senderIdBE);
  offset += sizeof(uint32_t);

  uint32_t usernameLenBE;
  std::memcpy(&usernameLenBE, data + offset, sizeof(uint32_t));
  size_t usernameLen = ntohl(usernameLenBE);
  offset += sizeof(uint32_t);

  if (usernameLen > 0 && offset + usernameLen <= size) {
    message.senderUsername =
        std::string(reinterpret_cast<const char *>(data + offset), usernameLen);
    offset += usernameLen;
  }

  if (offset + sizeof(uint32_t) > size)
    return message;
  uint32_t messageLenBE;
  std::memcpy(&messageLenBE, data + offset, sizeof(uint32_t));
  size_t messageLen = ntohl(messageLenBE);
  offset += sizeof(uint32_t);

  if (messageLen > 0 && offset + messageLen <= size)
    message.senderMessage.assign(reinterpret_cast<const char *>(data + offset),
                                 messageLen);

  return message;
}

Packet createChatMessagePacket(const ChatMessage &message) {
  auto data = serializeChatMessage(message);
  return Packet(MessageType::CHAT_BROADCAST, data);
}

ChatMessage extractChatMessage(const Packet &packet) {
  return deserializeChatMessage(packet.getData().data(),
                                packet.getData().size());
}

std::vector<uint8_t> serializeRoleAssignment(const RoleAssignment &assignment) {
  std::vector<uint8_t> data;

  size_t topicSize = assignment.topic.size();
  size_t wordSize = assignment.secretWord.size();
  data.reserve(sizeof(uint32_t) * 5 + topicSize + wordSize);

  uint32_t playerIdBE = htonl(assignment.playerId);
  data.insert(data.end(), reinterpret_cast<const uint8_t *>(&playerIdBE),
              reinterpret_cast<const uint8_t *>(&playerIdBE) +
                  sizeof(uint32_t));

  uint32_t roleBE = htonl(static_cast<uint32_t>(assignment.role));
  data.insert(data.end(), reinterpret_cast<const uint8_t *>(&roleBE),
              reinterpret_cast<const uint8_t *>(&roleBE) + sizeof(uint32_t));

  uint32_t topicLenBE = htonl(static_cast<uint32_t>(topicSize));
  data.insert(data.end(), reinterpret_cast<const uint8_t *>(&topicLenBE),
              reinterpret_cast<const uint8_t *>(&topicLenBE) +
                  sizeof(uint32_t));

  if (topicSize > 0)
    data.insert(data.end(), assignment.topic.begin(), assignment.topic.end());

  uint32_t wordLenBE = htonl(static_cast<uint32_t>(wordSize));
  data.insert(data.end(), reinterpret_cast<const uint8_t *>(&wordLenBE),
              reinterpret_cast<const uint8_t *>(&wordLenBE) + sizeof(uint32_t));

  if (wordSize > 0)
    data.insert(data.end(), assignment.secretWord.begin(),
                assignment.secretWord.end());

  return data;
}

RoleAssignment deserializeRoleAssignment(const uint8_t *data, size_t size) {
  RoleAssignment assignment;

  if (size < sizeof(uint32_t) * 3)
    return assignment;

  size_t offset = 0;

  uint32_t playerIdBE;
  std::memcpy(&playerIdBE, data + offset, sizeof(uint32_t));
  assignment.playerId = ntohl(playerIdBE);
  offset += sizeof(uint32_t);

  uint32_t roleBE;
  std::memcpy(&roleBE, data + offset, sizeof(uint32_t));
  assignment.role = static_cast<PlayerRole>(ntohl(roleBE));
  offset += sizeof(uint32_t);

  if (size < offset + sizeof(uint32_t))
    return assignment;

  uint32_t topicLenBE;
  std::memcpy(&topicLenBE, data + offset, sizeof(uint32_t));
  size_t topicLen = ntohl(topicLenBE);
  offset += sizeof(uint32_t);

  if (topicLen > 0 && offset + topicLen <= size) {
    assignment.topic =
        std::string(reinterpret_cast<const char *>(data + offset), topicLen);
    offset += topicLen;
  }

  if (offset + sizeof(uint32_t) > size)
    return assignment;

  uint32_t wordLenBE;
  std::memcpy(&wordLenBE, data + offset, sizeof(uint32_t));
  size_t wordLen = ntohl(wordLenBE);
  offset += sizeof(uint32_t);

  if (wordLen > 0 && offset + wordLen <= size) {
    assignment.secretWord =
        std::string(reinterpret_cast<const char *>(data + offset), wordLen);
  }

  return assignment;
}

Packet createRoleAssignmentPacket(const RoleAssignment &assignment) {
  auto data = serializeRoleAssignment(assignment);
  return Packet(MessageType::ROLE_ASSIGNMENT, data);
}

RoleAssignment extractRoleAssignment(const Packet &packet) {
  return deserializeRoleAssignment(packet.getData().data(),
                                   packet.getData().size());
}

std::vector<uint8_t> serializeVoteCommand(const VoteCommand &vote) {
  std::vector<uint8_t> data;
  data.reserve(sizeof(uint32_t) * 2);

  uint32_t voterBE = htonl(vote.voterId);
  data.insert(data.end(), reinterpret_cast<const uint8_t *>(&voterBE),
              reinterpret_cast<const uint8_t *>(&voterBE) + sizeof(uint32_t));

  uint32_t targetBE = htonl(vote.targetId);
  data.insert(data.end(), reinterpret_cast<const uint8_t *>(&targetBE),
              reinterpret_cast<const uint8_t *>(&targetBE) + sizeof(uint32_t));

  return data;
}

VoteCommand deserializeVoteCommand(const uint8_t *data, size_t size) {
  VoteCommand vote;

  if (size < sizeof(uint32_t) * 2)
    return vote;

  uint32_t voterBE;
  std::memcpy(&voterBE, data, sizeof(uint32_t));
  vote.voterId = ntohl(voterBE);

  uint32_t targetBE;
  std::memcpy(&targetBE, data + sizeof(uint32_t), sizeof(uint32_t));
  vote.targetId = ntohl(targetBE);

  return vote;
}

Packet createVoteCommandPacket(const VoteCommand &vote) {
  auto data = serializeVoteCommand(vote);
  return Packet(MessageType::VOTE_COMMAND, data);
}

VoteCommand extractVoteCommand(const Packet &packet) {
  return deserializeVoteCommand(packet.getData().data(),
                                packet.getData().size());
}

std::vector<uint8_t> serializeVoteResult(const VoteResult &result) {
  std::vector<uint8_t> data;

  uint32_t count = static_cast<uint32_t>(result.tally.size());
  data.reserve(sizeof(uint32_t) * (3 + count * 2));

  uint32_t countBE = htonl(count);
  data.insert(data.end(), reinterpret_cast<const uint8_t *>(&countBE),
              reinterpret_cast<const uint8_t *>(&countBE) + sizeof(uint32_t));

  for (const auto &[targetId, voteCount] : result.tally) {
    uint32_t targetBE = htonl(targetId);
    data.insert(data.end(), reinterpret_cast<const uint8_t *>(&targetBE),
                reinterpret_cast<const uint8_t *>(&targetBE) +
                    sizeof(uint32_t));

    uint32_t votesBE = htonl(voteCount);
    data.insert(data.end(), reinterpret_cast<const uint8_t *>(&votesBE),
                reinterpret_cast<const uint8_t *>(&votesBE) + sizeof(uint32_t));
  }

  uint32_t winnerBE = htonl(result.winnerId);
  data.insert(data.end(), reinterpret_cast<const uint8_t *>(&winnerBE),
              reinterpret_cast<const uint8_t *>(&winnerBE) + sizeof(uint32_t));

  uint32_t caughtBE = htonl(result.liarCaught ? 1 : 0);
  data.insert(data.end(), reinterpret_cast<const uint8_t *>(&caughtBE),
              reinterpret_cast<const uint8_t *>(&caughtBE) + sizeof(uint32_t));

  return data;
}

VoteResult deserializeVoteResult(const uint8_t *data, size_t size) {
  VoteResult result;

  if (size < sizeof(uint32_t) * 3)
    return result;

  size_t offset = 0;

  uint32_t countBE;
  std::memcpy(&countBE, data + offset, sizeof(uint32_t));
  uint32_t count = ntohl(countBE);
  offset += sizeof(uint32_t);

  for (uint32_t i = 0; i < count; ++i) {
    if (size < offset + sizeof(uint32_t) * 2)
      break;

    uint32_t targetBE;
    std::memcpy(&targetBE, data + offset, sizeof(uint32_t));
    uint32_t targetId = ntohl(targetBE);
    offset += sizeof(uint32_t);

    uint32_t votesBE;
    std::memcpy(&votesBE, data + offset, sizeof(uint32_t));
    uint32_t voteCount = ntohl(votesBE);
    offset += sizeof(uint32_t);

    result.tally[targetId] = voteCount;
  }

  if (size < offset + sizeof(uint32_t) * 2)
    return result;

  uint32_t winnerBE;
  std::memcpy(&winnerBE, data + offset, sizeof(uint32_t));
  result.winnerId = ntohl(winnerBE);
  offset += sizeof(uint32_t);

  uint32_t caughtBE;
  std::memcpy(&caughtBE, data + offset, sizeof(uint32_t));
  result.liarCaught = (ntohl(caughtBE) != 0);

  return result;
}

Packet createVoteResultPacket(const VoteResult &result) {
  auto data = serializeVoteResult(result);
  return Packet(MessageType::VOTE_RESULT, data);
}

VoteResult extractVoteResult(const Packet &packet) {
  return deserializeVoteResult(packet.getData().data(),
                               packet.getData().size());
}

} // namespace net