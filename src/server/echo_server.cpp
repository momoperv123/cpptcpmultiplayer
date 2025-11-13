#define WIN32_LEAN_AND_MEAN
#include "common/packet.h"
#include "server/server.h"
#include <iostream>
#include <string>
#include <windows.h>

using namespace net;

Server *g_server = nullptr;

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
  g_server = &server;

  if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE))
    std::cerr << "Failed to set console handler" << std::endl;

  server.setPacketCallback([&server](const Packet &packet, uint32_t clientId) {
    std::string message(packet.getData().begin(), packet.getData().end());
    std::cout << "Received message from client [" << clientId
              << "]: " << message << std::endl;

    server.broadcast(packet);
  });

  std::cout << "Starting server on port " << PORT << "..." << std::endl;
  std::cout << "Press Ctrl+C to shutdown" << std::endl;

  if (!server.start()) {
    std::cerr << "Failed to start server" << std::endl;
    return 1;
  }

  std::cout << "Server stopped" << std::endl;
  return 0;
}