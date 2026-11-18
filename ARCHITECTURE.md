# Architecture & Technical Reference

This document details the internal design of **TrimFlow**, its interaction with the Windows 10/11 Virtual Memory Manager (VMM) and Scheduler, and its safety guarantees.

---

## 1. Windows Virtual Memory & Working Set Mechanics

Each user-mode process on Windows maintains a **Working Set**—the subset of its virtual address space pages currently resident in physical RAM.

When an application such as Chromium (`brave.exe`, `chrome.exe`, `msedge.exe`), Firefox (`firefox.exe`), or Electron/CEF (`Spotify.exe`, `Discord.exe`, `steamwebhelper.exe`, `Code.exe`) runs:
* It spawns a **Browser / Main UI Process**, a **GPU Compositor Process**, **Audio & Network Service Processes**, and numerous **Renderer / Utility Subprocesses**.
* Background renderer subprocesses retain large V8/SpiderMonkey JavaScript heaps, decoded image caches, and DOM arenas in their physical Working Set long after the user has switched to another tab or minimized the window.

---

## 2. Five-Stage Context-Aware Optimization Pipeline

`TrimFlow` avoids foreground page-fault contention through five cooperative mechanisms in [src/main.cpp](src/main.cpp):

### Stage 1: Dynamic Session Discovery & Window Topology (`InspectWindows`)
* Enumerates all processes in the current user session (`ProcessIdToSessionId`), excluding critical OS kernel/session daemons (`kSystemProtected`) and anti-cheat services.
* Identifies the foreground process (`GetForegroundWindow`), detects fullscreen games (`MonitorFromWindow` bounds check), and builds the set of visible, non-minimized top-level windows (`EnumWindows`).

### Stage 2: Zero-Injection Subprocess Role Classification (`NtQueryInformationProcess`)
* Queries `ProcessCommandLineInformation` (`60`) via `NtQueryInformationProcess` using only `PROCESS_QUERY_LIMITED_INFORMATION` (no DLL injection or foreign virtual-memory reads).
* Classifies each subprocess in a multi-process tree into one of four architectural roles (`SubprocRole`):
  1. **`AudioOrNetwork` (`audio.mojom.AudioService`, `network.mojom.NetworkService`):** Unconditionally protected from throttling and trimming so background audio, WebRTC voice calls, and file downloads never drop packets or glitch.
  2. **`GpuCompositor` (`--type=gpu-process`):** Protected whenever any window of the parent application is visible on screen, ensuring 144Hz/240Hz frame pacing while scrolling.
  3. **`MainOrStandalone` (root process without `--type=`):** Protected while visible on screen; compressed when the application is minimized to the tray/taskbar.
  4. **`RendererOrWorker` (`--type=renderer`, `-contentproc`, `--type=utility`):** Background tabs and extension workers—always eligible for compression unless actively executing on the CPU (`> 1.5%` single-core utilization).

### Stage 3: Event-Driven Focus Promotion & Minimize Compression (`SetWinEventHook`)
* Registers out-of-context hooks (`WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS`) for:
  * **`EVENT_SYSTEM_FOREGROUND` & `EVENT_SYSTEM_MINIMIZEEND`:** Immediately promotes a newly focused or un-minimized process back to `NORMAL_PRIORITY_CLASS`, `MEMORY_PRIORITY_NORMAL` (`5`), and disables `EcoQoS` in `< 1 ms`.
  * **`EVENT_SYSTEM_MINIMIZESTART`:** Schedules a debounced optimization pass `1.5 seconds` after the user minimizes an application, reclaiming its UI and GPU working set immediately.

### Stage 4: Hysteresis & Anti-Thrashing (`ProcHistory`)
* Tracks `lastPostTrimBytes` and `lastTrimTick` per `PID`.
* Once an idle subprocess has been trimmed, `TrimFlow` skips redundant `K32EmptyWorkingSet` calls on subsequent sweeps unless the subprocess's Working Set has grown by at least **`+8 MB`** (`kReTrimGrowthBytes`). This eliminates unnecessary page-table churn and keeps `TrimFlow` at `0.00%` CPU.

### Stage 5: Kernel-Cooperative Demotion & Clean Exit Restoration
* For idle subprocesses:
  1. `SetProcessInformation(..., ProcessMemoryPriority)` → `MEMORY_PRIORITY_VERY_LOW` (`1`), prioritizing pages for in-RAM compression inside the Windows `MemCompression` store.
  2. `SetProcessInformation(..., ProcessPowerThrottling)` → `PROCESS_POWER_THROTTLING_EXECUTION_SPEED` (**EcoQoS**), confining background timers to Efficiency cores (`E-cores`) at minimum frequency.
  3. `SetPriorityClass(..., BELOW_NORMAL_PRIORITY_CLASS)` to prevent background garbage collection from preempting foreground threads.
  4. `K32EmptyWorkingSet()` when the `+8 MB` hysteresis threshold is exceeded.
* **Clean Exit Guarantee (`RestoreAllThrottledProcesses`):** On exit (`WM_DESTROY` / `--quit`), `TrimFlow` iterates through all tracked PIDs and restores normal memory priority, normal scheduling priority, and un-throttled execution state.

---

## 3. Self-Footprint Minimization

* Linked with `-Os -ffunction-sections -fdata-sections -Wl,--gc-sections` and a `64 KB` stack reservation (`-Wl,--stack,65536` and `STACK_SIZE_PARAM_IS_A_RESERVATION`).
* Invokes `HeapCompact(GetProcessHeap(), 0)` and `SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1)` after every optimization pass and UI interaction, maintaining a steady-state Private Working Set of **`~0.08 MB` (`80 KB`)**.
