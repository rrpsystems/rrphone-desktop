#include "AppPackage.h"

#include <windows.h>
#include <appmodel.h>

#include <future>

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>

namespace AppPackage {

bool isPackaged() {
    UINT32 length = 0;
    // Unpackaged processes get APPMODEL_ERROR_NO_PACKAGE; packaged ones report
    // the buffer is too small for the package full name.
    return GetCurrentPackageFullName(&length, nullptr) != APPMODEL_ERROR_NO_PACKAGE;
}

namespace {

StartupState fromWinRt(winrt::Windows::ApplicationModel::StartupTaskState state) {
    using winrt::Windows::ApplicationModel::StartupTaskState;
    switch (state) {
    case StartupTaskState::Enabled:
    case StartupTaskState::EnabledByPolicy:
        return StartupState::Enabled;
    case StartupTaskState::DisabledByUser:
        return StartupState::DisabledByUser;
    case StartupTaskState::DisabledByPolicy:
        return StartupState::DisabledByPolicy;
    default:
        return StartupState::Disabled;
    }
}

// The UI thread is an STA, and C++/WinRT refuses to block on an async call
// there. The StartupTask calls take milliseconds, so they run on a short-lived
// MTA thread and the UI thread just waits for the answer.
template <typename F>
StartupState onWorker(F &&work) {
    if (!isPackaged()) {
        return StartupState::Unavailable;
    }
    return std::async(std::launch::async, [&work]() {
               try {
                   winrt::init_apartment(winrt::apartment_type::multi_threaded);
                   const auto task =
                       winrt::Windows::ApplicationModel::StartupTask::GetAsync(kStartupTaskId).get();
                   const StartupState state = work(task);
                   winrt::uninit_apartment();
                   return state;
               } catch (...) {
                   return StartupState::Unavailable;
               }
           })
        .get();
}

} // namespace

StartupState startupState() {
    return onWorker([](const winrt::Windows::ApplicationModel::StartupTask &task) {
        return fromWinRt(task.State());
    });
}

StartupState setStartupEnabled(bool enabled) {
    return onWorker([enabled](const winrt::Windows::ApplicationModel::StartupTask &task) {
        if (enabled) {
            return fromWinRt(task.RequestEnableAsync().get());
        }
        task.Disable();
        return fromWinRt(task.State());
    });
}

} // namespace AppPackage
