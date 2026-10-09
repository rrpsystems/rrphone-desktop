#pragma once

// Running as an MSIX package (Microsoft Store) versus the classic installer.
//
// Inside a package, writes to HKCU\...\CurrentVersion\Run are virtualized and
// never reach Windows, so "Iniciar com o Windows" has to go through the
// package's StartupTask (declared in AppxManifest.xml, TaskId below) instead.
namespace AppPackage {

constexpr const wchar_t *kStartupTaskId = L"RRPSoftphoneStartup";

bool isPackaged();

enum class StartupState {
    Unavailable,       // not packaged, or the task is missing from the manifest
    Disabled,
    Enabled,
    DisabledByUser,    // turned off in Windows Settings > Apps > Startup: only the user can turn it back on
    DisabledByPolicy,
};

StartupState startupState();
// Returns the resulting state; enabling can be refused (DisabledByUser/Policy).
StartupState setStartupEnabled(bool enabled);

} // namespace AppPackage
