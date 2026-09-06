#pragma once

#include <QString>

// Dark theme derived from the RRP Systems logo (teal square + blue orbit),
// applied app-wide so the softphone reads as one coherent, branded window
// instead of default-grey Qt widgets — loosely modeled after the compact,
// dark softphone look (3CXPhone/MicroSip) the PRD asks for.
namespace Theme {
constexpr auto kBackground = "#14181D";
constexpr auto kPanel = "#1C2128";
constexpr auto kButton = "#232A33";
constexpr auto kButtonHover = "#2B333D";
constexpr auto kBorder = "#2B333D";
constexpr auto kTextPrimary = "#E8ECEF";
constexpr auto kTextSecondary = "#8B96A3";
constexpr auto kAccentTeal = "#2C9E82";  // RRP logo square
constexpr auto kAccentBlue = "#2E9FDB";  // RRP logo orbit / "Systems" wordmark
constexpr auto kSuccessGreen = "#2E9C4C";      // call action
constexpr auto kSuccessGreenMuted = "#1F4A2E"; // call action, nothing to dial
constexpr auto kDangerRed = "#C0392B";         // hangup

QString styleSheet();
}
