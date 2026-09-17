// TweakCatalogue.h - The list of changes this application knows how to make.
//
// Every entry is a documented Windows setting that Microsoft exposes through
// the Settings app, Control Panel or a shipped command line tool. Nothing here
// is an undocumented hack, and every entry can be put back.
//
// The string ids are the contract between the UI rows and the engine.
#pragma once

#include <QString>
#include <QVector>

class Tweak;

namespace TweakId {
// Optimise
inline constexpr char CleanTempFiles[] = "optimise.cleanTemp";
inline constexpr char StartupPrograms[] = "optimise.startupPrograms";
inline constexpr char OptimiseDrives[] = "optimise.drives";
inline constexpr char ClearBrowserCache[] = "optimise.browserCache";
inline constexpr char DisableServices[] = "optimise.services";
inline constexpr char MemoryTrim[] = "optimise.memoryTrim";
inline constexpr char WebSearch[] = "optimise.webSearch";

// Enhance
inline constexpr char BoostResponsiveness[] = "enhance.responsiveness";
inline constexpr char VisualEffects[] = "enhance.visualEffects";
inline constexpr char AnimationLatency[] = "enhance.menuDelay";
inline constexpr char ForegroundPriority[] = "enhance.foregroundPriority";
inline constexpr char Transparency[] = "enhance.transparency";
inline constexpr char AppTimeouts[] = "enhance.appTimeouts";
inline constexpr char MouseInputLag[] = "enhance.mouseInput";

// Perform
inline constexpr char HighPerformanceMode[] = "perform.highPerformance";
inline constexpr char CpuPriority[] = "perform.cpuPriority";
inline constexpr char BackgroundApps[] = "perform.backgroundApps";
inline constexpr char NetworkThrottling[] = "perform.networkThrottling";
inline constexpr char GamePriority[] = "perform.gamePriority";
inline constexpr char DisableTelemetry[] = "perform.telemetry";
inline constexpr char NvidiaProfile[] = "perform.nvidiaProfile";

// Retired ids, kept only so an older backup can still be reverted. Each one is
// now applied as part of the group named beside it.
//   "enhance.powerPlan"                -> enhance.responsiveness
//   "enhance.fastShutdown"             -> enhance.appTimeouts
//   "perform.startupDelay"             -> optimise.startupPrograms
//   "perform.powerThrottling"          -> perform.highPerformance
//   "perform.gpuScheduling"            -> perform.cpuPriority
//   "perform.gameMode"                 -> perform.gamePriority
//   "perform.fullscreenOptimisations"  -> perform.gamePriority
//   "perform.networkAdapter"           -> perform.networkThrottling
} // namespace TweakId

namespace TweakCatalogue {

/// Builds every tweak the app supports. Ownership passes to the caller.
QVector<Tweak *> createAll();

} // namespace TweakCatalogue
