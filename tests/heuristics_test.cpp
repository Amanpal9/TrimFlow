// Unit and behavioral test suite for TrimFlow heuristics & subprocess classification.
#include "../src/heuristics.h"

#include <cstdio>
#include <cstdlib>

namespace {

int g_passed = 0;
int g_failed = 0;

void Expect(bool cond, const char* expr, int line) {
    if (cond) {
        g_passed++;
    } else {
        g_failed++;
        printf("[FAIL] Line %d: %s\n", line, expr);
    }
}

#define EXPECT_TRUE(x)  Expect((x), #x, __LINE__)
#define EXPECT_FALSE(x) Expect(!(x), "!(" #x ")", __LINE__)
#define EXPECT_EQ(a, b) Expect((a) == (b), #a " == " #b, __LINE__)

void TestSystemAndAntiCheatProtection() {
    using trimflow::IsSystemProtectedProcess;
    // Critical Windows processes must be protected
    EXPECT_TRUE(IsSystemProtectedProcess(L"System"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"Memory Compression"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"csrss.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"dwm.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"explorer.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"audiodg.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"svchost.exe"));

    // Anti-cheat processes must never be touched
    EXPECT_TRUE(IsSystemProtectedProcess(L"vgc.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"vgtray.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"EasyAntiCheat.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"EasyAntiCheat_EOS.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"BEService.exe"));

    // Self-protection
    EXPECT_TRUE(IsSystemProtectedProcess(L"TrimFlow.exe"));
    EXPECT_TRUE(IsSystemProtectedProcess(L"WirelessBattery.exe"));

    // User-space desktop apps should be eligible for discovery
    EXPECT_FALSE(IsSystemProtectedProcess(L"brave.exe"));
    EXPECT_FALSE(IsSystemProtectedProcess(L"chrome.exe"));
    EXPECT_FALSE(IsSystemProtectedProcess(L"spotify.exe"));
    EXPECT_FALSE(IsSystemProtectedProcess(L"discord.exe"));
    EXPECT_FALSE(IsSystemProtectedProcess(L"steamwebhelper.exe"));
    EXPECT_FALSE(IsSystemProtectedProcess(L"code.exe"));
}

void TestSubprocessRoleClassification() {
    using trimflow::ClassifyCommandLine;
    using trimflow::SubprocRole;

    // Single-process apps are always MainOrStandalone
    EXPECT_EQ(ClassifyCommandLine(L"\"C:\\App\\foo.exe\" --type=renderer", 1), SubprocRole::MainOrStandalone);

    // Root browser process (no --type= flag)
    EXPECT_EQ(ClassifyCommandLine(L"\"C:\\Program Files\\BraveSoftware\\Brave-Browser\\Application\\brave.exe\"", 15),
              SubprocRole::MainOrStandalone);

    // GPU compositor subprocess
    EXPECT_EQ(ClassifyCommandLine(L"\"brave.exe\" --type=gpu-process --gpu-preferences=KAAA", 15),
              SubprocRole::GpuCompositor);

    // Audio & Network services (must be protected unconditionally)
    EXPECT_EQ(ClassifyCommandLine(L"\"Spotify.exe\" --type=utility --utility-sub-type=audio.mojom.AudioService", 7),
              SubprocRole::AudioOrNetwork);
    EXPECT_EQ(ClassifyCommandLine(L"\"msedge.exe\" --type=utility --utility-sub-type=network.mojom.NetworkService", 12),
              SubprocRole::AudioOrNetwork);

    // Chromium renderer & utility workers
    EXPECT_EQ(ClassifyCommandLine(L"\"brave.exe\" --type=renderer --lang=en-US --num-raster-threads=4", 20),
              SubprocRole::RendererOrWorker);
    EXPECT_EQ(ClassifyCommandLine(L"\"Discord.exe\" --type=utility --utility-sub-type=storage.mojom.StorageService", 6),
              SubprocRole::RendererOrWorker);

    // Firefox content tab subprocess
    EXPECT_EQ(ClassifyCommandLine(L"\"C:\\Program Files\\Mozilla Firefox\\firefox.exe\" -contentproc -childID 8 -isForBrowser", 10),
              SubprocRole::RendererOrWorker);
}

void TestDynamicGroupQualification() {
    using trimflow::ShouldQualifyGroup;
    constexpr SIZE_T MB = 1024ULL * 1024ULL;

    // Multi-process group >= 15 MB qualifies in both modes
    EXPECT_TRUE(ShouldQualifyGroup(12, 400 * MB, false, false, false, false));
    EXPECT_TRUE(ShouldQualifyGroup(12, 400 * MB, true,  false, false, false));

    // Tiny multi-process group (< 15 MB) is skipped
    EXPECT_FALSE(ShouldQualifyGroup(2, 8 * MB, false, false, false, false));

    // Single-process background app >= 25 MB qualifies in default mode, skipped in multi-only mode
    EXPECT_TRUE(ShouldQualifyGroup(1, 50 * MB, false, false, false, false));
    EXPECT_FALSE(ShouldQualifyGroup(1, 50 * MB, true,  false, false, false));

    // Active fullscreen single-process game is NEVER targeted
    EXPECT_FALSE(ShouldQualifyGroup(1, 4096 * MB, false, false, true, true));

    // Background multi-process browser while fullscreen game runs IS targeted
    EXPECT_TRUE(ShouldQualifyGroup(15, 1200 * MB, false, false, true, false));
}

void TestAntiThrashingHysteresis() {
    using trimflow::ShouldReTrimWithHysteresis;
    constexpr SIZE_T MB = 1024ULL * 1024ULL;

    // First-ever trim (lastTrimTick == 0) always runs
    EXPECT_TRUE(ShouldReTrimWithHysteresis(0, 120 * MB, 0, false));

    // Already trimmed to 10 MB, grew only to 14 MB (< +8 MB threshold) -> skip to prevent thrashing
    EXPECT_FALSE(ShouldReTrimWithHysteresis(1000, 14 * MB, 10 * MB, false));

    // Grew from 10 MB to 20 MB (> +8 MB threshold) -> re-trim
    EXPECT_TRUE(ShouldReTrimWithHysteresis(1000, 20 * MB, 10 * MB, false));

    // Forced CLI pass always trims
    EXPECT_TRUE(ShouldReTrimWithHysteresis(1000, 11 * MB, 10 * MB, true));
}

void TestCpuDeltaThreshold() {
    using trimflow::IsCpuActiveDelta;
    // 0.1% single-core over 1000 ms (10,000 100ns units) -> idle noise
    EXPECT_FALSE(IsCpuActiveDelta(500000, 510000));

    // 1.5% boundary (150,000 100ns units) -> still idle
    EXPECT_FALSE(IsCpuActiveDelta(500000, 650000));

    // 3.0% single-core over 1000 ms (300,000 100ns units) -> active
    EXPECT_TRUE(IsCpuActiveDelta(500000, 800000));
}

}  // namespace

int main() {
    printf("Running TrimFlow unit tests...\n");
    TestSystemAndAntiCheatProtection();
    TestSubprocessRoleClassification();
    TestDynamicGroupQualification();
    TestAntiThrashingHysteresis();
    TestCpuDeltaThreshold();

    printf("Results: %d passed, %d failed.\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
