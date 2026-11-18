# Community Launch Posts & Benchmark Writeups

Below are ready-to-paste, concrete technical posts tailored to each community. Every post leads with **verifiable numbers**, **why naive RAM cleaners fail**, and **how TrimFlow's architecture avoids page-fault stutters**.

---

## 1. `r/cpp` & Hacker News (`Show HN`)

**Title:**
> Show HN: TrimFlow – Context-aware Win32/C++17 subprocess memory & EcoQoS manager (80 KB RAM, zero dependencies)

**Body:**
Modern Windows desktops easily spend 4–8 GB of physical RAM on idle background Chromium, Electron, WebView2, and IDE helper trees (VS Code, Cursor, Brave/Chrome, Discord, Spotify, Slack).

Most "RAM cleaner" utilities just run `EmptyWorkingSet` across every process on a 5-second timer. That causes two real problems:
1. It evicts the hot UI, GPU compositor (`--type=gpu-process`), and audio pages (`audio.mojom.audioservice`) of the window you are actively using, causing frame drops and audio crackles.
2. It hammers dormant background processes that haven't allocated a single new page, burning CPU cycles on redundant page-table walks.

I built **TrimFlow** (`~70 KB` single static C++17 binary, `~80 KB` Private Working Set in tray) to handle this properly:

- **In-memory PEB command-line classification (`NtQueryInformationProcess`)**: Reads remote process command lines to classify subprocesses into `BrowserMain`, `GpuProcess`, `NetworkService`, `AudioService`, `Renderer`, `Utility`, and `Crashpad`.
- **Foreground-only hot-path protection**: The app you are actively typing in has its main UI thread, GPU compositor, audio/network services, and largest active renderer completely exempted from hard working-set eviction. Covered or minimized apps have their background renderers and extension hosts trimmed and placed into Windows 11 **EcoQoS** (`PROCESS_POWER_THROTTLING_EXECUTION_SPEED`) + `BELOW_NORMAL_PRIORITY_CLASS`.
- **Instant focus promotion (`SetWinEventHook`)**: Hooks `EVENT_SYSTEM_FOREGROUND` (`WINEVENT_OUTOFCONTEXT`) so the moment you `Alt+Tab` into a background window, all of its child processes are promoted back to normal QoS/priority in `< 1 ms`.
- **`+8 MB` growth hysteresis**: After trimming a PID, TrimFlow records its post-trim working set baseline and refuses to touch it again until it grows by at least `+8 MB`.

### Real-World Benchmark (16 GB Windows 11 Desktop)

| Application Group | Processes | Before TrimFlow | After TrimFlow | Reclaimed |
| :--- | :---: | :---: | :---: | :---: |
| **VS Code (`code.exe`)** (backgrounded) | 13 | 1,846.4 MB | 80.0 MB | **-1,766.4 MB (-95.7%)** |
| **Antigravity IDE** | 29 | 1,701.2 MB | 538.9 MB | **-1,162.3 MB (-68.3%)** |
| **Brave Browser** | 16 | 936.9 MB | 200.1 MB | **-736.8 MB (-78.6%)** |
| **Spotify** (audio playing) | 7 | 228.5 MB | 32.0 MB | **-196.5 MB (-86.0%)** |
| **TrimFlow.exe itself** | 1 | — | **0.08 MB** | — |

Total system memory usage dropped from **79% → 51%** (~3.8 GB physical RAM moved to the Windows Standby List for instant zero-disk soft-fault reuse).

Repo (MIT License, pure C++17/Win32, 50+ unit tests, zero external dependencies):
https://github.com/vncwr/TrimFlow

---

## 2. `r/Windows11` & `r/pcmasterrace`

**Title:**
> I built an open-source, 80 KB native C++ tray utility that reclaimed 3.8 GB of RAM from background VS Code, Brave, and Electron apps without breaking audio or UI smoothness

**Body:**
If you keep a browser, VS Code/Cursor, Discord, and Spotify open at the same time on a 16 GB PC, you've probably noticed Windows sitting at 75–85% RAM usage even when 90% of those windows are covered in the background.

### Why traditional "RAM Boosters" suck
Traditional RAM cleaners blindly force every process on your PC to dump its memory every few seconds. That forces your active window's GPU process and audio service to fault back in from page tables, causing micro-stutters while scrolling or gaming.

### How TrimFlow works differently
1. **Knows which window you're actually looking at**: The foreground window's main UI, GPU compositor, and active tab stay untouched in fast RAM.
2. **Reads subprocess roles**: It inspects each background helper (`--type=renderer`, `--type=gpu-process`, `audio.mojom.audioservice`) so background audio (like Spotify or a YouTube Music tab) never glitches.
3. **EcoQoS + Hysteresis**: When an app is covered or minimized, TrimFlow trims its idle background helpers once, puts them in Windows 11 Efficiency Mode (EcoQoS), and **leaves them alone** until they grow by at least `+8 MB`. The instant you `Alt+Tab` back to that window, TrimFlow restores full speed in under 1 millisecond.
4. **Tiny footprint**: Single `.exe`, `~70 KB` on disk, uses `0.08 MB` (80 KB) of RAM in the system tray.

### Install in one command (Scoop) or grab the portable `.exe`
```powershell
scoop install https://raw.githubusercontent.com/vncwr/TrimFlow/main/packaging/scoop/trimflow.json
```

GitHub Repo + Source + Benchmarks: https://github.com/vncwr/TrimFlow

---

## 3. `r/opensource` (Contributor Invite Post)

**Title:**
> [C++17 / Win32] Looking for contributors to TrimFlow — a tiny (80 KB RAM) context-aware memory & EcoQoS manager for Windows 11

**Body:**
Hi `r/opensource`! I recently open-sourced **TrimFlow**, a zero-dependency C++17 utility that tames multi-process memory bloat (Electron, Chromium, WebView2, IDEs) on Windows while protecting foreground UI, GPU compositors, and audio streams.

To make contributing approachable without needing deep Win32 kernel knowledge, the classification and hysteresis brain is isolated in a pure C++17 header (`src/heuristics.h`) with a fast unit test suite (`test.bat`) that compiles and runs in under 2 seconds.

We just opened several well-scoped `good first issue` and `help wanted` tickets for `v1.1`:
- `--diagnose` CLI flag to print per-process role classifications and decision reasons
- `--json` output mode for `--status` and `--once`
- Optional `trimflow.ini` configuration file & `--validate-config` checker
- Table-driven benchmark scenarios for additional multi-process apps (Slack, Teams, Firefox, JetBrains IDEs)
- Dark/light theme adaptation and localization support for the tray menu & startup toast

Repo: https://github.com/vncwr/TrimFlow
Contributing Guide: https://github.com/vncwr/TrimFlow/blob/main/CONTRIBUTING.md
