#include "common/game_state.h"
#include <algorithm>
#include <iostream>
#include <random>

namespace net {

const std::vector<std::pair<std::string, std::vector<std::string>>>
    GameState::TOPIC_WORDS = {
        {"Fruit",
         {"Apple", "Banana", "Orange", "Grape", "Strawberry", "Watermelon",
          "Pineapple", "Mango"}},
        {"City",
         {"Paris", "Tokyo", "London", "New York", "Sydney", "Berlin", "Rome",
          "Moscow"}},
        {"Animal",
         {"Dog", "Cat", "Elephant", "Lion", "Tiger", "Bear", "Wolf", "Rabbit"}},
        {"Color",
         {"Red", "Blue", "Green", "Yellow", "Purple", "Orange", "Pink",
          "Black"}},
        {"Food",
         {"Pizza", "Burger", "Pasta", "Sushi", "Taco", "Salad", "Soup",
          "Sandwich"}},
        {"Sport",
         {"Soccer", "Basketball", "Tennis", "Swimming", "Running", "Cycling",
          "Golf", "Baseball"}}};

GameState::GameState()
    : roundActive_(false), currentTopic_(), currentWord_(), currentLiarId_(0) {}

bool GameState::addPlayer(uint32_t id, const std::string &username) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (players_.find(id) != players_.end())
    return false;

  players_[id] = PlayerState(id, username);
  return true;
}

bool GameState::removePlayer(uint32_t id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = players_.find(id);
  if (it == players_.end())
    return false;

  votes_.erase(id);

  for (auto voteIt = votes_.begin(); voteIt != votes_.end();) {
    if (voteIt->second == id)
      voteIt = votes_.erase(voteIt);
    else
      ++voteIt;
  }

  players_.erase(it);

  if (currentLiarId_ == id)
    currentLiarId_ = 0;

  return true;
}

PlayerState GameState::getPlayerState(uint32_t id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = players_.find(id);
  return it != players_.end() ? it->second : PlayerState();
}

bool GameState::hasPlayer(uint32_t id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return players_.find(id) != players_.end();
}

std::vector<PlayerState> GameState::getAllPlayerStates() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<PlayerState> states;
  states.reserve(players_.size());
  for (const auto &pair : players_)
    states.push_back(pair.second);
  return states;
}

std::vector<uint32_t> GameState::getAllPlayerIds() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<uint32_t> ids;
  ids.reserve(players_.size());
  for (const auto &pair : players_)
    ids.push_back(pair.first);
  return ids;
}

size_t GameState::getPlayerCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return players_.size();
}

void GameState::clearAllPlayers() {
  std::lock_guard<std::mutex> lock(mutex_);
  players_.clear();
  clearRound();
}

bool GameState::canStartRound() const {
  std::lock_guard<std::mutex> lock(mutex_);
  size_t count = players_.size();
  return count >= 3 && count <= 6;
}

void GameState::startNewRound() {
  std::lock_guard<std::mutex> lock(mutex_);

  std::cout << "[GameState] startNewRound() called, player count: "
            << players_.size() << std::endl;

  if (players_.size() < 3 || players_.size() > 6) {
    std::cerr << "[GameState] startNewRound() aborted: invalid player count"
              << std::endl;
    return;
  }

  std::cout << "[GameState] Clearing previous round..." << std::endl;
  clearRound();

  std::cout << "[GameState] Picking random topic and word..." << std::endl;
  auto [topic, word] = pickRandomTopicAndWord();
  currentTopic_ = topic;
  currentWord_ = word;
  std::cout << "[GameState] Selected topic: " << topic << ", word: " << word
            << std::endl;

  std::vector<uint32_t> playerIds;
  playerIds.reserve(players_.size());
  for (const auto &pair : players_) {
    playerIds.push_back(pair.first);
  }

  std::cout << "[GameState] Selecting random liar from " << playerIds.size()
            << " players..." << std::endl;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<size_t> dis(0, playerIds.size() - 1);
  size_t liarIndex = dis(gen);
  currentLiarId_ = playerIds[liarIndex];
  std::cout << "[GameState] Selected liar: Player [" << currentLiarId_ << "]"
            << std::endl;

  for (auto &pair : players_) {
    if (pair.first == currentLiarId_) {
      pair.second.role = PlayerRole::LIAR;
    } else {
      pair.second.role = PlayerRole::GUESSER;
    }
  }

  roundActive_ = true;
  std::cout << "[GameState] Round is now active" << std::endl;
  std::cout << "[GameState] startNewRound() completed successfully"
            << std::endl;
}

void GameState::clearRound() {
  roundActive_ = false;
  currentTopic_.clear();
  currentWord_.clear();
  currentLiarId_ = 0;
  votes_.clear();

  for (auto &pair : players_) {
    pair.second.role = PlayerRole::NONE;
  }
}

std::string GameState::getCurrentTopic() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return currentTopic_;
}

std::string GameState::getCurrentWord() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return currentWord_;
}

uint32_t GameState::getCurrentLiarId() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return currentLiarId_;
}

bool GameState::isRoundActive() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return roundActive_;
}

std::pair<std::string, std::string> GameState::pickRandomTopicAndWord() const {
  if (TOPIC_WORDS.empty()) {
    return {"", ""};
  }

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<size_t> topicDis(0, TOPIC_WORDS.size() - 1);

  size_t topicIndex = topicDis(gen);
  const auto &topicPair = TOPIC_WORDS[topicIndex];
  const std::string &topic = topicPair.first;
  const std::vector<std::string> &words = topicPair.second;

  if (words.empty()) {
    return {topic, ""};
  }

  std::uniform_int_distribution<size_t> wordDis(0, words.size() - 1);
  size_t wordIndex = wordDis(gen);
  const std::string &word = words[wordIndex];

  return {topic, word};
}

bool GameState::submitVote(uint32_t voterId, uint32_t targetId) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!roundActive_) {
    return false;
  }

  if (votes_.find(voterId) != votes_.end()) {
    return false;
  }

  if (players_.find(voterId) == players_.end() ||
      players_.find(targetId) == players_.end()) {
    return false;
  }

  votes_[voterId] = targetId;
  return true;
}

bool GameState::hasPlayerVoted(uint32_t playerId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return votes_.find(playerId) != votes_.end();
}

std::unordered_map<uint32_t, uint32_t> GameState::getVoteTally() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::unordered_map<uint32_t, uint32_t> tally;

  for (const auto &[voterId, targetId] : votes_) {
    tally[targetId]++;
  }

  return tally;
}

void GameState::clearVotes() {
  std::lock_guard<std::mutex> lock(mutex_);
  votes_.clear();
}

void GameState::calculateAndApplyScores(bool liarCaught, uint32_t votedOutId,
                                        bool hasMajority) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!roundActive_ || currentLiarId_ == 0) {
    return;
  }

  if (liarCaught && votedOutId == currentLiarId_ && hasMajority) {
    for (auto &pair : players_) {
      if (pair.second.role == PlayerRole::GUESSER) {
        if (votes_.find(pair.first) != votes_.end() &&
            votes_[pair.first] == currentLiarId_) {
          pair.second.score += 1;
        }
      }
    }
  } else if (!liarCaught && hasMajority && votedOutId != currentLiarId_) {
    if (players_.find(currentLiarId_) != players_.end()) {
      players_[currentLiarId_].score += 2;
    }
  }
}

int GameState::getPlayerScore(uint32_t playerId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = players_.find(playerId);
  return it != players_.end() ? it->second.score : 0;
}

std::unordered_map<uint32_t, int> GameState::getAllScores() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::unordered_map<uint32_t, int> scores;
  for (const auto &pair : players_) {
    scores[pair.first] = pair.second.score;
  }
  return scores;
}

} // namespace net