cmake_minimum_required(VERSION 3.19)

project(NextGenTweaks
    VERSION 0.1.0
    DESCRIPTION "NEXTGEN TWEAKS - PC optimisation suite"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Svg Network)

# OpenSSL for offline Ed25519 licence-key verification (same toolchain OpenSSL
# the Qt kit ships). The licensing backend lives under authentication/.
find_package(OpenSSL REQUIRED)

# Turns on AUTOMOC / AUTOUIC and sensible Qt defaults.
qt_standard_project_setup()

# qt_standard_project_setup() deliberately leaves AUTORCC off, so enable it here
# for resources.qrc (logo + generated SVG icons).
set(CMAKE_AUTORCC ON)

set(NEXTGEN_SOURCES
    src/main.cpp

    src/Theme.h
    src/Theme.cpp
    src/ThemeManager.h
    src/ThemeManager.cpp
    src/IconProvider.h
    src/IconProvider.cpp
    src/AuthService.h
    src/AuthService.cpp
    src/AppController.h
    src/AppController.cpp
    src/SystemMetricsService.h
    src/SystemMetricsService.cpp
    src/SettingsStore.h
    src/SettingsStore.cpp
    src/StartupManager.h
    src/StartupManager.cpp

    # System modification engine
    src/ElevationHelper.h
    src/ElevationHelper.cpp
    src/RestorePointService.h
    src/RestorePointService.cpp
    src/TweakBackupStore.h
    src/TweakBackupStore.cpp
    src/Tweak.h
    src/Tweak.cpp
    src/SystemTweaks.h
    src/SystemTweaks.cpp
    src/NetworkAdapterInfo.h
    src/NetworkAdapterInfo.cpp
    src/GpuTweaks.h
    src/GpuTweaks.cpp
    src/HardwareSerials.h
    src/HardwareSerials.cpp
    src/TweakCatalogue.h
    src/TweakCatalogue.cpp
    src/TweakEngine.h
    src/TweakEngine.cpp
    src/ConfirmChangesDialog.h
    src/ConfirmChangesDialog.cpp

    src/IconButton.h
    src/IconButton.cpp
    src/ModernLineEdit.h
    src/ModernLineEdit.cpp
    src/ModernButton.h
    src/ModernButton.cpp
    src/ModernCheckBox.h
    src/ModernCheckBox.cpp
    src/LinkLabel.h
    src/LinkLabel.cpp
    src/OrDivider.h
    src/OrDivider.cpp
    src/PanelSeparator.h
    src/PanelSeparator.cpp
    src/MenuCard.h
    src/MenuCard.cpp
    src/AvatarBadge.h
    src/AvatarBadge.cpp
    src/AccountMenu.h
    src/AccountMenu.cpp
    src/NavList.h
    src/NavList.cpp
    src/UserCard.h
    src/UserCard.cpp
    src/StatusStrip.h
    src/StatusStrip.cpp
    src/ToggleSwitch.h
    src/ToggleSwitch.cpp
    src/Badge.h
    src/Badge.cpp
    src/OptionRow.h
    src/OptionRow.cpp
    src/SectionCard.h
    src/SectionCard.cpp
    src/DonutGauge.h
    src/DonutGauge.cpp
    src/Sparkline.h
    src/Sparkline.cpp
    src/MetricCard.h
    src/MetricCard.cpp
    src/StatTile.h
    src/StatTile.cpp
    src/ScoreBox.h
    src/ScoreBox.cpp
    src/ComparisonChart.h
    src/ComparisonChart.cpp
    src/PageHeader.h
    src/PageHeader.cpp
    src/LiveMonitorChart.h
    src/LiveMonitorChart.cpp
    src/StarRating.h
    src/StarRating.cpp
    src/SummaryCard.h
    src/SummaryCard.cpp
    src/DetailRow.h
    src/DetailRow.cpp
    src/ModernComboBox.h
    src/ColorPickerDialog.h
    src/ColorPickerDialog.cpp
    src/ModernComboBox.cpp

    src/FramelessWindow.h
    src/FramelessWindow.cpp
    src/WindowStyle.h
    src/SingleInstanceGuard.h
    src/SingleInstanceGuard.cpp
    src/NukeGuard.h
    src/NukeGuard.cpp
    src/Updater.h
    src/Updater.cpp
    src/RootContainer.h
    src/RootContainer.cpp
    src/RoundedCornersEffect.h
    src/RoundedCornersEffect.cpp
    src/CustomTitleBar.h
    src/CustomTitleBar.cpp
    src/BrandingPanel.h
    src/BrandingPanel.cpp
    src/LoginPanel.h
    src/LoginPanel.cpp
    src/LoginWindow.h
    src/LoginWindow.cpp
    src/LicenseClient.h
    src/LicenseClient.cpp
    src/AdminClient.h
    src/AdminClient.cpp
    src/ClientCommandPoller.h
    src/ClientCommandPoller.cpp
    src/HardwareId.h
    src/HardwareId.cpp
    src/DiscordWebhook.h
    src/DiscordWebhook.cpp
    src/LicensePublicKey.h
    src/CreateAccountDialog.h
    src/CreateAccountDialog.cpp
    src/LicenseActivationDialog.h
    src/LicenseActivationDialog.cpp
    src/FramelessDialog.h
    src/FramelessDialog.cpp
    src/MessageDialog.h
    src/MessageDialog.cpp
    src/SocialLinksBar.h
    src/SocialLinksBar.cpp

    # Shared licence core (offline signature verification only).
    authentication/libnglicense/src/Crypto.cpp
    authentication/libnglicense/src/License.cpp
    src/MainMenuPanel.h
    src/MainMenuPanel.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/SectionPage.h
    src/SectionPage.cpp
    src/OptimisePage.h
    src/OptimisePage.cpp
    src/EnhancePage.h
    src/EnhancePage.cpp
    src/PerformPage.h
    src/PerformPage.cpp
    src/SystemInfoPage.h
    src/SystemInfoPage.cpp
    src/SettingsPage.h
    src/SettingsPage.cpp
    src/AdminPage.h
    src/AdminPage.cpp
)

# Logo + generated SVG icons, compiled into the binary (no runtime file lookups).
set(NEXTGEN_RESOURCES
    resources.qrc
)

if(WIN32)
    set(NEXTGEN_WIN_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/assets/app/app.rc)
endif()

qt_add_executable(NextGenTweaks
    WIN32
    MANUAL_FINALIZATION
    ${NEXTGEN_SOURCES}
    ${NEXTGEN_RESOURCES}
    ${NEXTGEN_WIN_RESOURCES}
)

# Every logo asset present on disk is embedded automatically, so dropping a
# higher resolution nextgen_tweaks_logo@2x.png / @3x.png, or an SVG version,
# into assets/logo is all that is needed - LogoView picks the sharpest one.
file(GLOB NEXTGEN_LOGO_FILES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/logo/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/logo/*.svg"
)

if(NOT NEXTGEN_LOGO_FILES)
    message(WARNING "No logo asset found in assets/logo - the branding panel will show a placeholder.")
endif()

qt_add_resources(NextGenTweaks "nextgen_logo"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES ${NEXTGEN_LOGO_FILES}
)

# Every SVG in assets/icons is embedded automatically too, so a newly drawn icon
# works the moment it is saved. Listing them by hand used to mean a new icon
# silently failed to load at runtime.
file(GLOB NEXTGEN_ICON_FILES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/*.svg"
)

if(NOT NEXTGEN_ICON_FILES)
    message(FATAL_ERROR "No SVG icons found in assets/icons - the UI cannot render.")
endif()

qt_add_resources(NextGenTweaks "nextgen_icons"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES ${NEXTGEN_ICON_FILES}
)

target_include_directories(NextGenTweaks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/authentication/libnglicense/include)

target_link_libraries(NextGenTweaks PRIVATE
    Qt6::Widgets
    Qt6::Svg
    Qt6::Network
    OpenSSL::Crypto
)

if(WIN32)
    # iphlpapi: GetIfTable2() network counters.
    # psapi:    EmptyWorkingSet() for the memory trim.
    # shell32:  ShellExecuteEx() for the elevated relaunch.
    # pdh:      GPU-engine utilisation performance counters.
    # dwmapi:   DwmSetWindowAttribute() for the Windows 11 corner policy.
    target_link_libraries(NextGenTweaks PRIVATE iphlpapi psapi shell32 pdh dwmapi)
endif()

if(MSVC)
    target_compile_options(NextGenTweaks PRIVATE /W4 /permissive-)
else()
    target_compile_options(NextGenTweaks PRIVATE -Wall -Wextra)
endif()

set_target_properties(NextGenTweaks PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

qt_finalize_executable(NextGenTweaks)

install(TARGETS NextGenTweaks
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin
)
