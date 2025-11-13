#define WIN32_LEAN_AND_MEAN
#include "common/game_state.h"
#include "common/packet.h"
#include "common/serialization.h"
#include "server/server.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <windows.h>

using namespace net;

Server *g_server = nullptr;

namespace {

void logInfo(const std::string &msg) {
  std::cout << "[INFO] " << msg << std::endl;
}
void logWarn(const std::string &msg) {
  std::cout << "[WARN] " << msg << std::endl;
}
void logError(const std::string &msg) {
  std::cerr << "[ERROR] " << msg << std::endl;
}

} // namespace

BOOL WINAPI ConsoleHandler(DWORD dwType) {
  if (dwType == CTRL_C_EVENT && g_server) {
    std::cout << "Shutting down server..." << std::endl;
    g_server->stop();
    return TRUE;
  }

  return FALSE;
}

int main() {
  const uint16_t PORT = 8000;

  Server server(PORT);
  GameState gameState;
  g_server = &server;

  if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE))
    std::cerr << "Failed to set console handler" << std::endl;

  server.setPacketCallback([&server, &gameState](const Packet &packet,
                                                 uint32_t clientId) {
    uint16_t packetType = packet.getType();

    auto startNewRoundIfPossible = [&server, &gameState]() {
      if (!gameState.canStartRound()) {
        logInfo("Cannot start round yet - waiting for enough players");
        return;
      }
      if (gameState.isRoundActive())
        return;

      logInfo("Starting next round");

      std::string topic;
      std::string word;
      uint32_t liarId = 0;

      try {
        gameState.startNewRound();
        topic = gameState.getCurrentTopic();
        word = gameState.getCurrentWord();
        liarId = gameState.getCurrentLiarId();
      } catch (const std::exception &e) {
        logError(std::string("Exception in startNewRound(): ") + e.what());
        return;
      } catch (...) {
        logError("Unknown exception in startNewRound()");
        return;
      }

      if (liarId == 0 || topic.empty()) {
        logError("Round started but topic/liar not set properly");
        return;
      }

      logInfo("Round info -> Topic: " + topic + ", Word: " + word +
              ", Liar: Player [" + std::to_string(liarId) + "]");

      auto allPlayerStates = gameState.getAllPlayerStates();
      for (const auto &player : allPlayerStates) {
        if (player.role == PlayerRole::LIAR) {
          RoleAssignment assignment(player.id, PlayerRole::LIAR, topic, "");
          Packet rolePacket = createRoleAssignmentPacket(assignment);
          server.sendPacket(player.id, rolePacket);
        } else if (player.role == PlayerRole::GUESSER) {
          RoleAssignment assignment(player.id, PlayerRole::GUESSER, topic,
                                    word);
          Packet rolePacket = createRoleAssignmentPacket(assignment);
          server.sendPacket(player.id, rolePacket);
        }
      }
    };

    switch (packetType) {
    case MessageType::PLAYER_JOIN: {
      logInfo("Received PLAYER_JOIN packet from client [" +
              std::to_string(clientId) + "]");
      std::string username = "Player " + std::to_string(clientId);
      if (packet.getData().size() > 0)
        username =
            std::string(packet.getData().begin(), packet.getData().end());
      logInfo("Attempting to add player [" + std::to_string(clientId) +
              "] with username: " + username);
      bool added = gameState.addPlayer(clientId, username);
      if (!added) {
        logWarn("Failed to add player [" + std::to_string(clientId) +
                "] - player may already exist");
        break;
      }

      logInfo("Player [" + std::to_string(clientId) +
              "] joined the game. Current count: " +
              std::to_string(gameState.getPlayerCount()));
      logInfo(std::string("Can start round: ") +
              (gameState.canStartRound() ? "yes" : "no") + ", Round active: " +
              (gameState.isRoundActive() ? "true" : "false"));

      auto allPlayers = gameState.getAllPlayerStates();
      Packet statePacket = createGameStateUpdatePacket(allPlayers);
      server.sendPacket(clientId, statePacket);

      PlayerState newPlayer = gameState.getPlayerState(clientId);
      Packet joinPacket =
          createPlayerStatePacket(MessageType::PLAYER_JOINED, newPlayer);
      server.broadcastExcept(clientId, joinPacket);

      startNewRoundIfPossible();
      break;
    }

    case MessageType::CHAT_MESSAGE: {
      auto connInfo = server.getConnectionManager().getConnection(clientId);
      if (!connInfo) {
        std::cerr << "Warning: CHAT_MESSAGE from unknown client [" << clientId
                  << "]" << std::endl;
        break;
      }

      std::string message(packet.getData().begin(), packet.getData().end());

      while (!message.empty() &&
             (message.back() == ' ' || message.back() == '\n' ||
              message.back() == '\r')) {
        message.pop_back();
      }

      if (message.size() > 6 && message.substr(0, 6) == "/vote ") {
        if (!gameState.isRoundActive()) {
          std::cout << "Vote command received but no round is active"
                    << std::endl;
          break;
        }

        std::string targetName = message.substr(6);
        while (!targetName.empty() && (targetName.front() == ' ')) {
          targetName = targetName.substr(1);
        }
        while (!targetName.empty() &&
               (targetName.back() == ' ' || targetName.back() == '\n' ||
                targetName.back() == '\r')) {
          targetName.pop_back();
        }

        auto allPlayers = gameState.getAllPlayerStates();
        uint32_t targetId = 0;
        for (const auto &p : allPlayers) {
          if (p.username == targetName) {
            targetId = p.id;
            break;
          }
        }

        if (targetId == 0) {
          std::cout << "Vote failed: Could not find player with username '"
                    << targetName << "'" << std::endl;
          std::cout << "Available players: ";
          for (const auto &p : allPlayers) {
            std::cout << p.username << " ";
          }
          std::cout << std::endl;
          break;
        }

        if (!gameState.submitVote(clientId, targetId)) {
          std::cout << "Vote failed: Player [" << clientId
                    << "] may have already voted" << std::endl;
          break;
        }

        std::cout << "Player [" << clientId << "] voted for Player ["
                  << targetId << "] (" << targetName << ")" << std::endl;

        size_t totalPlayers = gameState.getPlayerCount();
        size_t votesCount = 0;
        auto allPlayersForVoteCount = gameState.getAllPlayerStates();
        for (const auto &p : allPlayersForVoteCount) {
          if (gameState.hasPlayerVoted(p.id)) {
            votesCount++;
          }
        }

        if (votesCount >= totalPlayers) {
          auto tally = gameState.getVoteTally();
          uint32_t liarId = gameState.getCurrentLiarId();
          uint32_t winnerId = 0;
          uint32_t maxVotes = 0;
          bool hasMajority = false;

          for (const auto &[targetId, voteCount] : tally) {
            if (voteCount > maxVotes) {
              maxVotes = voteCount;
              winnerId = targetId;
            }
          }

          hasMajority = (maxVotes > totalPlayers / 2);
          bool liarCaught = (hasMajority && winnerId == liarId);

          VoteResult result;
          result.tally = tally;
          result.winnerId = winnerId;
          result.liarCaught = liarCaught;

          Packet resultPacket = createVoteResultPacket(result);
          server.broadcast(resultPacket);

          std::cout << "All players voted! Processing results early..."
                    << std::endl;
          std::cout << "Vote Results:" << std::endl;
          for (const auto &[targetId, voteCount] : tally) {
            std::cout << "  Player [" << targetId << "]: " << voteCount
                      << " votes" << std::endl;
          }
          if (hasMajority) {
            std::cout << "Winner: Player [" << winnerId << "]" << std::endl;
            std::cout << "Liar " << (liarCaught ? "CAUGHT" : "SURVIVED")
                      << std::endl;
          } else {
            std::cout << "No majority - no winner" << std::endl;
          }

          gameState.calculateAndApplyScores(liarCaught, winnerId, hasMajority);

          auto scores = gameState.getAllScores();
          std::cout << "Scores:" << std::endl;
          for (const auto &[playerId, score] : scores) {
            auto player = gameState.getPlayerState(playerId);
            std::cout << "  " << player.username << " [" << playerId
                      << "]: " << score << " point(s)" << std::endl;
          }

          gameState.clearRound();

          auto allPlayersUpdate = gameState.getAllPlayerStates();
          Packet statePacket = createGameStateUpdatePacket(allPlayersUpdate);
          server.broadcast(statePacket);

          startNewRoundIfPossible();
        }
        break;
      }

      PlayerState player = gameState.getPlayerState(clientId);
      std::string username = (player.id != 0)
                                 ? player.username
                                 : (connInfo->username.empty()
                                        ? "Player " + std::to_string(clientId)
                                        : connInfo->username);

      ChatMessage chatMessage(clientId, username, message);
      Packet chatPacket = createChatMessagePacket(chatMessage);
      server.broadcast(chatPacket);
      break;
    }

    case MessageType::PLAYER_LEAVE: {
      PlayerState leavingPlayer = gameState.getPlayerState(clientId);
      if (!gameState.hasPlayer(clientId))
        break;

      bool roundWasActive = gameState.isRoundActive();
      bool removed = gameState.removePlayer(clientId);
      if (!removed)
        break;

      logInfo("Player [" + std::to_string(clientId) +
              "] disconnected from the game");

      if (roundWasActive) {
        logWarn(
            "Active round interrupted by player disconnect. Resetting round.");
        gameState.clearRound();
      }

      Packet leavePacket =
          createPlayerStatePacket(MessageType::PLAYER_LEAVE, leavingPlayer);
      server.broadcastExcept(clientId, leavePacket);

      auto allPlayers = gameState.getAllPlayerStates();
      Packet statePacket = createGameStateUpdatePacket(allPlayers);
      server.broadcast(statePacket);

      if (gameState.getPlayerCount() < 3) {
        logWarn(
            "Not enough players to continue. Waiting for additional players.");
      } else {
        startNewRoundIfPossible();
      }
      break;
    }

    default:
      break;
    }
  });

  std::cout << "Press Ctrl+C to shutdown" << std::endl;

  std::thread serverThread([&server]() { server.start(); });

  while (server.isRunning()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  if (serverThread.joinable())
    serverThread.join();
  gameState.clearAllPlayers();
  return 0;
}