#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace net {

enum class PlayerRole { NONE = 0, LIAR = 1, GUESSER = 2 };

struct PlayerState {
  uint32_t id;
  std::string username;
  PlayerRole role;
  int score;

  PlayerState() : id(0), role(PlayerRole::NONE), score(0) {}
  PlayerState(uint32_t id, const std::string &username,
              PlayerRole role = PlayerRole::NONE, int score = 0)
      : id(id), username(username), role(role), score(score) {}
};

class GameState {
public:
  GameState();
  ~GameState() = default;

  bool addPlayer(uint32_t id, const std::string &username = "");

  bool removePlayer(uint32_t id);

  PlayerState getPlayerState(uint32_t id) const;

  bool hasPlayer(uint32_t id) const;

  std::vector<PlayerState> getAllPlayerStates() const;

  std::vector<uint32_t> getAllPlayerIds() const;

  size_t getPlayerCount() const;

  void clearAllPlayers();

  bool canStartRound() const;
  void startNewRound();
  void clearRound();

  std::string getCurrentTopic() const;
  std::string getCurrentWord() const;
  uint32_t getCurrentLiarId() const;
  bool isRoundActive() const;

  bool submitVote(uint32_t voterId, uint32_t targetId);
  bool hasPlayerVoted(uint32_t playerId) const;
  std::unordered_map<uint32_t, uint32_t>
  getVoteTally() const; // targetId -> vote count
  void clearVotes();

  void calculateAndApplyScores(bool liarCaught, uint32_t votedOutId,
                               bool hasMajority);
  int getPlayerScore(uint32_t playerId) const;
  std::unordered_map<uint32_t, int> getAllScores() const;

private:
  mutable std::mutex mutex_;
  std::unordered_map<uint32_t, PlayerState> players_;

  bool roundActive_;
  std::string currentTopic_;
  std::string currentWord_;
  uint32_t currentLiarId_;

  // voterId -> targetId
  std::unordered_map<uint32_t, uint32_t> votes_;

  static const std::vector<std::pair<std::string, std::vector<std::string>>>
      TOPIC_WORDS;

  std::pair<std::string, std::string> pickRandomTopicAndWord() const;
};

} // namespace net