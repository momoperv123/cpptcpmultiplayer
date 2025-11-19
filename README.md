# C++ TCP Multiplayer

## Game Rules: Liar Line

**Idea:** Fast-paced real-time chat deduction. One player lies, others expose them before time expires.

### 1. Setup

* 1 server, 3–6 players.
* Server assigns one *Liar*, others *Guessers*.
* Server picks a random **topic** (e.g., "Fruit," "City," "Animal").
* Guessers see the secret word (e.g., "Banana"); Liar only sees the topic ("Fruit").

### 2. Round Flow

* Everyone chats in the shared log; guessers challenge suspicious answers while the liar tries to blend in.
* Each player may cast exactly one vote per round with `/vote <username>`.
* Votes can be entered at any time. When every connected player has voted the server reveals the results, updates scores, and immediately starts the next round.
* If a player disconnects during a round, votes involving them are cleared and the next round is started once enough players remain.

### 3. Scoring

* Majority correctly votes out the Liar -> each correct Guesser +1 point.
* Liar survives -> Liar +2 points.
* Ties or no majority -> no points.

### 4. Rotation

* Next round, server randomly assigns a new Liar and topic.
* Game continues until predefined score (e.g., 10 pts) or round limit.

### 5. Technical Notes

* Server validates chat and vote packets.
* Chat messages are limited to 128 bytes.
* Vote packets follow the `{type, playerID, payload}` structure.
* Each player can vote once per round; subsequent attempts are ignored.
* Disconnecting clears a player’s vote, removes votes targeting them, and cancels the current round so a fresh one can begin automatically when enough players remain.

### 6. Optional Additions

* **Spectator mode:** view-only sockets.
* **Word categories:** difficulty scaling.
* **Latency compensation:** server stores clock drift offset per client.

Clean, deterministic, minimal—ready for C++ TCP implementation.

## Building

### Prerequisites
- CMake 3.15 or higher
- C++17 compatible compiler (MSVC, GCC, or Clang)
- Windows (currently uses Winsock2)

### Installing Prerequisites

#### CMake
- Download from [cmake.org](https://cmake.org/download/)
- Or use package manager: `winget install Kitware.CMake`

#### C++ Compiler - WinLibs (GCC/MinGW-w64)
- Download from [https://winlibs.com/](https://winlibs.com/)
- Recommended: Latest **Win64 (x86_64)** release version with **MSVCRT runtime** and **POSIX threads**
- Extract the archive (you'll need [7-Zip](https://www.7-zip.org/) for .7z files)
- Add the `mingw64/bin` folder to your system PATH
  - System Properties -> Environment Variables -> Edit PATH -> Add `C:\path\to\mingw64\bin`
- Restart PowerShell/terminal after adding to PATH
- Verify installation: `g++ --version`

### Build Instructions

**Debug Build:**
```powershell
mkdir build-debug
cd build-debug
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

**Release Build:**
```powershell
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

Or simply (defaults to Release):
```powershell
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

**If CMake can't find compiler:**
- Make sure the compiler is in your PATH
- Restart terminal after adding MinGW to PATH
- Try: `g++ --version` to verify installation

Executables will be in `build/bin/` (or `build-debug/bin/`):
- `echo_server.exe`
- `echo_client.exe`
- `game_server.exe`
- `game_client.exe`

## Usage

### Starting the Echo Server

```powershell
.\build\bin\echo_server.exe
```

The server listens on port 8000 by default. Press `Ctrl+C` to gracefully shutdown.

### Connecting an Echo Client

```powershell
.\build\bin\echo_client.exe 127.0.0.1
```

Type messages and they will be echoed back. Type `exit`, `quit`, `end`, `e`, or `q` to disconnect.

### Starting the Game Server

```powershell
.\build\bin\game_server.exe
```

The game server listens on port 8000. Players can connect and play. Press `Ctrl+C` to shutdown.

### Connecting a Game Client

```powershell
.\build\bin\game_client.exe 127.0.0.1 [username]
```

Type to chat, press Enter to send. Close the window or press `Ctrl+C` to quit.

### Testing Multiple Clients

Open multiple terminal windows and connect multiple clients to test concurrent connections.

#### Quick smoke-check

1. Three clients join, verify round starts automatically and each role is announced.
2. Everyone votes the same target -> scores for guessers increment.
3. Repeat with the liar surviving (split vote) -> liar score increments by 2.
4. Disconnect one player mid-round -> round should reset and wait until at least three players remain.
5. Rejoin with a new client and confirm roles/votes work in the next round.
   
---

## Troubleshooting

### Port already in use
If you see "Bind failed", another process is using port 8000. Either:
- Stop the other process
- Change `PORT` constant in the source code

### Client can't connect
- Ensure server is running first
- Verify server IP address (use `127.0.0.1` for localhost)
- Check Windows Firewall settings
