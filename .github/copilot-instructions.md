# Chatterino7-cyan — Copilot Instructions

Chatterino7-cyan is a C++/Qt 6 Twitch (and Kick) chat client — a fork of Chatterino 7 that adds features to integrate it with custom Youtube IRC data, including feature borrowed from Chatty. The codebase is a large Qt application built with CMake + Conan 2 on Windows/Linux/macOS.

## Build & Test (Windows)

Use the **x64 Native Tools Command Prompt for VS 2022** (or source `vcvars64.bat`). Qt must be on `PATH` (e.g. `C:\Qt\6.11.0\msvc2022_64\bin`).

```cmd
mkdir build && cd build
conan install .. -s build_type=Release -c tools.cmake.cmaketoolchain:generator="NMake Makefiles" --build=missing --output-folder=.
cmake -G"NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="conan_toolchain.cmake" -DCMAKE_PREFIX_PATH="C:\Qt\6.11.0\msvc2022_64" -DCMAKE_CXX_COMPILER="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe" -DCMAKE_C_COMPILER="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe" ..
nmake
```

Key CMake flags:
- `-DBUILD_TESTS=ON` — enable tests (off by default)
- `-DBUILD_BENCHMARKS=ON` — enable benchmarks
- `-DCHATTERINO_PLUGINS=ON` — enable Lua plugin support
- `-DCHATTERINO_SPELLCHECK=ON` — enable Hunspell spell-check
- `-DCHATTERINO_NO_AVIF_PLUGIN=ON` — if static `libavif` is unavailable

**Run tests:**
```cmd
ctest --repeat until-pass:4 --output-on-failure
```

See [BUILDING_ON_WINDOWS.md](../BUILDING_ON_WINDOWS.md) for the full guide. Linux/macOS guides are at [`BUILDING_ON_LINUX.md`](../BUILDING_ON_LINUX.md) and [`BUILDING_ON_MAC.md`](../BUILDING_ON_MAC.md).

## Architecture

```
src/
  common/       # Core utilities, enums, network, settings, credentials
  controllers/  # Business-logic: accounts, emotes, highlights, filters, commands, plugins
  providers/    # Platform integrations
    seventv/    # 7TV REST API, EventAPI (WebSocket), emotes, badges, paints, personal emotes
    twitch/     # Twitch IRC/PubSub/EventSub, channel, user, message builder
    kick/       # Kick.com account, channel, chat server, message builder
    bttv/       # BetterTTV emotes
    ffz/        # FrankerFaceZ emotes and badges
    irc/        # Generic IRC
    emoji/      # Emoji rendering
    pronouns/   # User pronouns
  singletons/   # App-wide singletons: fonts, paths, resources, themes, window manager
  widgets/      # All Qt UI: windows, splits, dialogs, settings pages, tooltips
  messages/     # Message model and rendering pipeline
  util/         # General-purpose helpers
  debug/        # Debug utilities
tests/          # Google Test suite (50+ files, mirrors src/ structure)
mocks/          # Mock headers used by tests
benchmarks/     # Google Benchmark suite
lib/            # Vendored third-party libraries (see below)
resources/      # App resources (icons, themes, etc.)
```

### 7TV-Specific Subsystem (`src/providers/seventv/`)

- **SeventvAPI** — REST client for the 7TV API
- **SeventvEmotes** — channel and global emote management
- **SeventvBadges** / **SeventvPersonalEmotes** — cosmetics
- **SeventvPaints** + `paints/` — paint cosmetics (LinearGradient, RadialGradient, Url, DropShadow variants)
- **SeventvEventAPI** (`eventapi/`) — WebSocket event subscriptions for live updates

7TV is integrated into `TwitchChannel`, `TwitchIrcServer`, and `KickChannel`.

### Key Third-Party Libraries

| Library | Purpose |
|---------|---------|
| `lib/signals/` | Pajlada typed signal/slot system (used instead of Qt signals in most places) |
| `lib/settings/` | Pajlada settings serialization |
| `lib/sol2/` + `lib/lua/` | Lua 5.4 plugin engine |
| `lib/libcommuni/` | Qt IRC protocol library |
| `lib/qtkeychain/` | Secure credential storage |
| `lib/twitch-eventsub-ws/` | Twitch EventSub WebSocket |
| `lib/googletest/` | Test framework |
| `lib/rapidjson/` | JSON parsing |
| `lib/miniaudio/` | Audio playback |
| `lib/WinToast/` | Windows toast notifications |

## Code Conventions

Derived from [CONTRIBUTING.md](../CONTRIBUTING.md):

- **Naming:** `camelCase` for functions and public members; `camelCase_` (trailing underscore) for **private** members; `PascalCase` for types.
- **No `get` prefix** on getters — use `name()` not `getName()`. Use `is`/`has` prefix for booleans.
- **Always use `this->`** to access instance members inside methods.
- **Initialize POD types with `{}`** — e.g. `int count{};`, `bool flag_{};`, `QWidget *ptr{};`.
- **No C-style casts** — use `static_cast<T>()`, `dynamic_cast<T>()`, or `T(value)` for constructors.
- **Pass parameters by lowest sufficient level of indirection** — prefer `Channel&` over `ChannelPtr&` when ownership isn't needed.
- **Smart pointers:** `std::unique_ptr` for single ownership, `std::shared_ptr` for shared. For `QObject`s, prefer the parent/child tree; use `deleteLater()` instead of `delete`.
- **Signals:** Use `pajlada::Signals::Signal<T>` and `SignalHolder` for non-Qt signals. Qt signals/slots are used where Qt requires it.
- **Comments:** Only where logic isn't self-evident. Don't repeat what the signature already says.
- **Usage strings in commands:** Follow POSIX convention — `Usage: /cmd <required> [optional].`

## Code Style

Auto-formatted by [`clang-format`](.clang-format) (Google-based style, 4-space indent, 80-column limit). Set up format-on-save in your editor.

Static analysis: `clang-tidy` (see CI workflow [`.github/workflows/clang-tidy.yml`](workflows/clang-tidy.yml)).

Run the formatting check locally:
```cmd
clang-format --dry-run --Werror src/**/*.cpp src/**/*.hpp
```

## Testing

Tests live in `tests/src/` and mirror the `src/` layout. Mock objects are in `mocks/include/mocks/`. Tests use Google Test (`gtest`/`gmock`).

When adding or modifying functionality, add or update the corresponding test file under `tests/src/`. Build with `-DBUILD_TESTS=ON` and run `ctest`.

## 7TV Fork Specifics

- Issues with 7TV features (paints, personal emotes, EventAPI) → [SevenTV/chatterino7 issues](https://github.com/SevenTV/chatterino7/issues)
- Issues with core Chatterino features → [Chatterino/chatterino2 issues](https://github.com/Chatterino/chatterino2/issues)
- Upstream Chatterino 2 changes are periodically merged; be careful when modifying shared code

## Cyan Fork Specifics

- Upstream Chatterino 7 changes are periodically merged; be careful when modifying shared code

## Changelog

When implementing a new feature or notable fix, add an entry to [`CHANGELOG.c7.md`](../CHANGELOG.c7.md) under the `## Unversioned` section at the top of the file. Follow the existing style:

- **New feature:** `- Added <description>.`
- **Minor improvement:** `- Minor: <description>.`
- **Bug fix:** `- Bugfix: <description>.`
- **Dev/infra change:** `- Dev: <description>.`

Keep entries concise (one line). If the change corresponds to a PR, append the PR number as `(#123)`.