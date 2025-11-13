#include "client/client.h"
#include "common/game_state.h"
#include "common/packet.h"
#include "common/serialization.h"
#include <cctype>
#include <chrono>
#include <conio.h>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

using namespace net;

std::map<uint32_t, PlayerState> players;
std::mutex playersMutex;

void printStatusBar() {
  std::lock_guard<std::mutex> lock(playersMutex);
  std::cout << "\r\033[K";
  std::cout << "============================================================"
            << std::endl;
  std::cout << "Players: ";
  bool first = true;
  for (const auto &[id, state] : players) {
    if (!first)
      std::cout << ", ";
    std::cout << "[" << id << "] " << state.username << " (" << state.score
              << " pts)";
    first = false;
  }
  std::cout << std::endl;
  std::cout << "============================================================"
            << std::endl;
}

std::string getScoreSummary() {
  std::lock_guard<std::mutex> lock(playersMutex);
  if (players.empty())
    return "Scores: (no players yet)";

  std::ostringstream oss;
  oss << "Scores: ";
  bool first = true;
  for (const auto &[id, state] : players) {
    if (!first)
      oss << " | ";
    oss << state.username << "=" << state.score;
    first = false;
  }
  return oss.str();
}

void printChatMessage(const std::string &username, const std::string &message) {
  std::cout << "[" << username << "] " << message << std::endl;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <server_address>" << std::endl;
    return 1;
  }

  const uint16_t PORT = 8000;
  const std::string SERVER_ADDRESS = argv[1];
  std::string username = (argc >= 3) ? argv[2] : "Player";

  Client client;

  if (!client.connect(SERVER_ADDRESS, PORT)) {
    std::cerr << "Failed to connect to server" << std::endl;
    return 1;
  }

  std::cout << "Connected to game server!" << std::endl;
  std::cout << "Waiting for game state..." << std::endl;

  client.setPacketCallback([&client](const Packet &packet) {
    uint16_t packetType = packet.getType();

    switch (packetType) {
    case MessageType::GAME_STATE_UPDATE: {
      static bool initialStateReceived = false;
      auto allPlayers = extractGameStateUpdate(packet);
      {
        std::lock_guard<std::mutex> lock(playersMutex);
        players.clear();
        for (const auto &player : allPlayers)
          players[player.id] = player;
      }
      std::cout << std::endl;
      if (!initialStateReceived) {
        printStatusBar();
        std::cout << "Game state received. Ready to play!" << std::endl;
        std::cout << "Type to chat, Enter to send, Ctrl+C to quit" << std::endl;
        std::cout << "Remember: cast your vote anytime with /vote <username>."
                  << std::endl;
        initialStateReceived = true;
      } else {
        printStatusBar();
        std::cout << getScoreSummary() << std::endl;
        std::cout << ">>> New round! Keep chatting and cast votes with /vote "
                     "<username>."
                  << std::endl;
      }
      break;
    }

    case MessageType::PLAYER_JOINED: {
      PlayerState newPlayer = extractPlayerState(packet);
      {
        std::lock_guard<std::mutex> lock(playersMutex);
        players[newPlayer.id] = newPlayer;
      }
      std::cout << std::endl;
      std::cout << ">>> Player [" << newPlayer.id << "] " << newPlayer.username
                << " joined the game" << std::endl;
      printStatusBar();
      break;
    }

    case MessageType::PLAYER_LEAVE: {
      PlayerState leavingPlayer = extractPlayerState(packet);
      {
        std::lock_guard<std::mutex> lock(playersMutex);
        players.erase(leavingPlayer.id);
      }
      std::cout << std::endl;
      std::cout << ">>> Player [" << leavingPlayer.id << "] "
                << leavingPlayer.username << " left the game" << std::endl;
      printStatusBar();
      break;
    }

    case MessageType::CHAT_BROADCAST: {
      ChatMessage chatMessage = extractChatMessage(packet);
      std::cout << std::endl;
      printChatMessage(chatMessage.senderUsername, chatMessage.senderMessage);
      break;
    }

    case MessageType::ROLE_ASSIGNMENT: {
      RoleAssignment assignment = extractRoleAssignment(packet);
      std::cout << std::endl;
      std::cout
          << "============================================================"
          << std::endl;
      std::cout << "ROUND STARTED" << std::endl;
      std::cout
          << "============================================================"
          << std::endl;

      if (assignment.role == PlayerRole::LIAR) {
        std::cout << "Role: LIAR" << std::endl;
        std::cout << "Topic: " << assignment.topic << std::endl;
        std::cout << "Goal: Convince others you know the word!" << std::endl;
      } else if (assignment.role == PlayerRole::GUESSER) {
        std::cout << "Role: GUESSER" << std::endl;
        std::cout << "Topic: " << assignment.topic << std::endl;
        std::cout << "Secret Word: " << assignment.secretWord << std::endl;
        std::cout << "Goal: Find the liar!" << std::endl;
      }
      std::cout
          << "============================================================"
          << std::endl;
      std::cout << "Chat is live. When you suspect someone, vote with"
                << std::endl;
      std::cout << "  /vote <username>" << std::endl;
      std::cout << getScoreSummary() << std::endl;
      printStatusBar();
      break;
    }

    case MessageType::VOTE_RESULT: {
      VoteResult result = extractVoteResult(packet);
      std::cout << std::endl;
      std::cout
          << "============================================================"
          << std::endl;
      std::cout << "VOTE RESULTS" << std::endl;
      std::cout
          << "============================================================"
          << std::endl;
      for (const auto &[targetId, voteCount] : result.tally) {
        std::string targetName = "Unknown";
        {
          std::lock_guard<std::mutex> lock(playersMutex);
          if (players.find(targetId) != players.end()) {
            targetName = players[targetId].username;
          }
        }
        std::cout << targetName << " [" << targetId << "]: " << voteCount
                  << " vote(s)" << std::endl;
      }
      std::cout
          << "------------------------------------------------------------"
          << std::endl;
      if (result.winnerId != 0) {
        std::string winnerName = "Unknown";
        {
          std::lock_guard<std::mutex> lock(playersMutex);
          if (players.find(result.winnerId) != players.end()) {
            winnerName = players[result.winnerId].username;
          }
        }
        std::cout << "Winner: " << winnerName << " [" << result.winnerId << "]"
                  << std::endl;
        std::cout << "Result: "
                  << (result.liarCaught ? "LIAR CAUGHT!" : "LIAR SURVIVED!")
                  << std::endl;
      } else {
        std::cout << "Result: No majority - no winner" << std::endl;
      }
      std::cout
          << "============================================================"
          << std::endl;
      printStatusBar();
      std::cout << std::endl
                << ">>> Scores updated! " << getScoreSummary() << std::endl;
      std::cout << "Waiting for the server to begin the next round..."
                << std::endl;
      break;
    }

    default:
      break;
    }
  });

  client.startReceiving();

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  Packet joinPacket(MessageType::PLAYER_JOIN,
                    std::vector<uint8_t>(username.begin(), username.end()));
  std::cout << "Sending PLAYER_JOIN packet with username: " << username
            << std::endl;
  if (!client.sendPacket(joinPacket)) {
    std::cerr << "Failed to send join packet - sendPacket returned false"
              << std::endl;
    client.disconnect();
    return 1;
  }
  std::cout << "PLAYER_JOIN packet sent successfully" << std::endl;

  std::string chatBuffer = "";

  while (client.isConnected()) {
    if (_kbhit()) {
      int key = _getch();

      if (key == 13 || key == 10) {
        if (!chatBuffer.empty()) {
          Packet chatPacket(MessageType::CHAT_MESSAGE, chatBuffer);
          if (!client.sendPacket(chatPacket)) {
            std::cerr << "Failed to send chat message" << std::endl;
          }
          chatBuffer.clear();
          std::cout << std::endl;
        }
      } else if (key == 8 || key == 127) {
        if (!chatBuffer.empty()) {
          chatBuffer.pop_back();
          std::cout << "\b \b";
          std::cout.flush();
        }
      } else if (key >= 32 && key <= 126) {
        chatBuffer += static_cast<char>(key);
        std::cout << static_cast<char>(key);
        std::cout.flush();
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  client.stopReceiving();
  client.disconnect();
  std::cout << "Disconnected from server" << std::endl;
  return 0;
}