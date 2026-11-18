# TrimFlow

**Context-Aware Subprocess Memory & EcoQoS Optimizer for Windows 10/11**

`TrimFlow` is an ultra-lightweight (~80 KB RAM footprint, zero external dependencies) native C++17/Win32 system tray and CLI utility that **dynamically discovers** multi-process and memory-heavy desktop applications running in the user's Windows session—including any browser, Electron/CEF/WebView2 app, IDE, background updater, or media client—and reclaims idle physical memory without inducing page-fault stutters in active user workflows or games.

---

## 1. Why?

Modern desktop and web applications rely on multi-process architectures (such as Chromium's site-isolation model and Electron's helper hierarchy) that spawn 15 to 60+ OS subprocesses per application. Even when background tabs, extension workers, or minimized launcher windows (`steamwebhelper.exe`, `Spotify.exe`, `Discord.exe`) are completely idle, their V8 heaps and uncommitted page tables remain pinned in the active physical Working Set—often consuming **2 GB to 4+ GB of physical RAM**.

Conventional "RAM cleaner" utilities blindly invoke `EmptyWorkingSet` across **every** process on the system simultaneously, including the foreground game, active browser compositor, and audio playback threads. This forces the Windows Virtual Memory Manager (VMM) to immediately fault active pages back from disk or standby lists, causing micro-stutters and frame-time spikes.

---

## 2. How TrimFlow Works

Rather than treating an application as a monolithic block or flushing global system memory indiscriminately, `TrimFlow` evaluates **individual subprocesses** through a three-stage heuristic pipeline before cooperating with the Windows kernel:

1. **Foreground & Window Visibility Protection (`InspectWindows`)**
   * Identifies the active foreground process (`GetForegroundWindow`) and enumerates all visible, non-minimized top-level windows (`EnumWindows`).
   * Subprocesses owning active visible UI surfaces are protected from working-set eviction so window switching and UI rendering remain instantaneous (`0 ms` repaint latency).

2. **CPU Delta Telemetry Sampling (`GetProcessTimes`)**
   * Samples kernel and user-mode execution time across a `1.2-second` telemetry window for every candidate subprocess.
   * Any subprocess exceeding `0.25%` single-core utilization—such as an active audio decoder (`Spotify.exe`), WebRTC voice thread (`Discord.exe`), video playback tab, or GPU compositor—is marked **Active**, skipped from trimming, and restored to `MEMORY_PRIORITY_NORMAL` (`5`).

3. **Kernel-Cooperative Subprocess Demotion**
   * For subprocesses verified to be **idle** (`< 0.25%` CPU and not in the foreground):
     1. **Page Priority Demotion (`ProcessMemoryPriority` → `MEMORY_PRIORITY_VERY_LOW`):** Signals the Windows 10/11 Memory Manager to prioritize these pages for in-RAM compression inside the kernel **Memory Compression Store** rather than evicting active foreground or game pages.
     2. **Execution Throttling (`ProcessPowerThrottling` → `EcoQoS`):** Enables Windows Efficiency Mode (`PROCESS_POWER_THROTTLING_EXECUTION_SPEED`), scheduling background V8 timers and garbage-collection ticks onto efficient cores at lowest frequency so they do not pollute L3 cache or interrupt game render threads.
     3. **Targeted Working-Set Trim (`K32EmptyWorkingSet`):** Reclaims unused physical pages exclusively from idle background child processes.

---

## 3. Real-World Benchmark

Measured on Windows 11 with 15 `brave.exe` subprocesses, 7 `Spotify.exe` subprocesses, 5 `msedge.exe` subprocesses, 6 `msedgewebview2.exe` subprocesses, and 2 `SnippingTool.exe` subprocesses:

| Application | Subprocesses | Trimmed | Active | Before (MB) | After (MB) | Reclaimed (MB) |
|---|---:|---:|---:|---:|---:|---:|
| `brave.exe` | 15 | 15 | 0 | 2,072.5 MB | 29.8 MB | **-2,042.7 MB** |
| `Spotify.exe` | 7 | 7 | 0 | 528.8 MB | 9.0 MB | **-519.8 MB** |
| `msedge.exe` | 5 | 5 | 0 | 226.2 MB | 0.0 MB | **-226.2 MB** |
| `SnippingTool.exe` | 2 | 2 | 0 | 174.8 MB | 0.0 MB | **-174.8 MB** |
| `msedgewebview2.exe` | 6 | 6 | 0 | 128.6 MB | 2.5 MB | **-126.1 MB** |
| **Total** | **35** | **35** | **0** | **3,130.9 MB** | **41.2 MB** | **-3,089.7 MB** |

* **`TrimFlow.exe` Own Footprint:** `~0.15 MB` (`150 KB`) Private Working Set, `0.00%` idle CPU.

---

## 4. Features

* **System Tray Indicator:** Displays a live emerald gauge of system memory load with a tooltip reporting used/total RAM and cumulative session memory reclaimed.
* **Auto-Optimize Background Subprocesses:** Periodically sweeps idle subprocesses at a configurable interval (`15s`, `30s`, or `60s`).
* **Game-Boost Mode:** Automatically detects when a fullscreen game is running and switches to aggressive background trimming across non-game launchers (`steamwebhelper.exe`, `epicwebhelper.exe`, browsers, and chat clients).
* **Visible UI Window Protection:** Keeps non-minimized browser/app UI processes warm while compressing idle background tab renderers.
* **Per-Application Exclusion Toggles:** Enable or exclude individual applications directly from the tray menu; settings persist in `HKCU\Software\TrimFlow`.
* **Command-Line Interface:** Run one-shot optimizations or inspect current memory usage from scripts or terminals.

---

## 5. Command-Line Usage

```powershell
TrimFlow.exe            # Launch the background system tray optimizer
TrimFlow.exe --once     # Run a single optimization pass, print the Before/After table, and exit
TrimFlow.exe --status   # Print current memory usage of managed applications without modifying state
TrimFlow.exe --quit     # Gracefully close the running tray instance
```

---

## 6. Building from Source

Requires [MinGW-w64](https://www.mingw-w64.org/) (`g++` on `PATH`, e.g., via MSYS2):

```powershell
build.bat   # Windows Command Prompt / PowerShell
make        # MSYS2 / GNU Make
```

Produces a standalone, statically linked `TrimFlow.exe` with no external runtime dependencies.

---

## 7. Safety & Anti-Cheat Compliance

* **Zero Code Injection / Zero Memory Modification:** `TrimFlow` uses exclusively documented user-mode Win32 APIs (`SetProcessInformation` and `K32EmptyWorkingSet`). It never reads, writes, or hooks foreign virtual address spaces.
* **Dynamic Session Discovery & System/Anti-Cheat Protection:** Rather than relying on a hardcoded application list, `TrimFlow` dynamically inspects all processes in the current user session (`ProcessIdToSessionId`), automatically excluding critical OS kernel/session daemons (`kSystemProtected`), anti-cheat services (`Vanguard`, `EasyAntiCheat`, `BattlEye`), and the active foreground fullscreen game.

---

## 8. License

Released under the [MIT License](LICENSE).
