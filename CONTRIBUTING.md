# Contributing to TrimFlow

Thank you for your interest in contributing to **TrimFlow**!

## Core Design Principles

1. **Zero Stutter in Active Workflows:** Never evict pages from the active foreground process, active audio/network services (`audio.mojom.AudioService`, `network.mojom.NetworkService`), or visible UI/GPU compositors.
2. **Strict Anti-Cheat & OS Safety:** Never use DLL injection, remote threads, or foreign virtual-memory reads (`ReadProcessMemory`). Only use documented user-mode Win32 APIs (`SetProcessInformation`, `SetPriorityClass`, `K32EmptyWorkingSet`, and `NtQueryInformationProcess` with `PROCESS_QUERY_LIMITED_INFORMATION`).
3. **Minimal Self-Footprint:** Keep the binary dependency-free (`C++17` + Win32 only) and maintain a steady-state Private Working Set under `250 KB`.

## Development Workflow

1. Build the binary:
   ```powershell
   .\build.bat
   ```
2. Run the unit test suite before submitting a pull request:
   ```powershell
   .\test.bat
   ```
3. Verify CLI status and one-shot execution:
   ```powershell
   .\TrimFlow.exe --status
   .\TrimFlow.exe --once
   ```
