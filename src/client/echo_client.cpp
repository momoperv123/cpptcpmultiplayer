#include "client/client.h"
#include "common/packet.h"
#include <iostream>
#include <string>

using namespace net;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <server_address>" << std::endl;
    return 1;
  }

  const uint16_t PORT = 8000;
  const std::string SERVER_ADDRESS = argv[1];

  Client client;

  if (!client.connect(SERVER_ADDRESS, PORT)) {
    std::cerr << "Failed to connect to server" << std::endl;
    return 1;
  }

  std::cout << "Disconnect with 'exit', 'quit', 'end', 'e', or 'q'" << std::endl
            << std::endl;

  std::string input;
  std::string exitCommands[] = {"exit", "quit", "end", "e", "q"};

  while (client.isConnected() && std::getline(std::cin, input)) {
    bool shouldExit = false;
    for (const auto &cmd : exitCommands) {
      if (input == cmd) {
        shouldExit = true;
        break;
      }
    }

    if (shouldExit) {
      std::cout << "Disconnecting from server..." << std::endl;
      break;
    }

    Packet packet(MessageType::ECHO, input);

    if (!client.sendPacket(packet)) {
      std::cerr << "Failed to send packet" << std::endl;
      break;
    }

    Packet receivedPacket;
    if (client.receivePacket(receivedPacket)) {
      std::string echoMessage(receivedPacket.getData().begin(),
                              receivedPacket.getData().end());
      std::cout << "Echo: " << echoMessage << std::endl;
    } else {
      std::cout << "Server disconnected" << std::endl;
      break;
    }
  }

  client.disconnect();
  return 0;
}