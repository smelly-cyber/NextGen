#include "TweakCatalogue.h"

#include "GpuTweaks.h"
#include "SystemTweaks.h"
#include "Tweak.h"

#include <QObject>

namespace {

// --- Registry roots used below, spelled out once ---------------------------
const QLatin1String kDesktop("HKEY_CURRENT_USER\\Control Panel\\Desktop");
const QLatin1String kExplorerAdvanced(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced");
const QLatin1String kVisualEffects(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects");
const QLatin1String kPersonalize(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize");
const QLatin1String kDwm("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\DWM");
const QLatin1String kSearchSettings(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\SearchSettings");
const QLatin1String kBackgroundApps(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\BackgroundAccessApplications");
const QLatin1String kGameDvr("HKEY_CURRENT_USER\\System\\GameConfigStore");
const QLatin1String kPriorityControl(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\PriorityControl");
const QLatin1String kMultimediaProfile(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\"
    "SystemProfile");
const QLatin1String kGraphicsDrivers(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers");
const QLatin1String kMouse("HKEY_CURRENT_USER\\Control Panel\\Mouse");
const QLatin1String kGamesTask(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\"
    "SystemProfile\\Tasks\\Games");
const QLatin1String kDataCollection(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection");
const QLatin1String kSerialize(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Serialize");
const QLatin1String kSearchUser(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Search");
const QLatin1String kControl("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control");
const QLatin1String kPowerThrottling(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Power\\PowerThrottling");
const QLatin1String kGameBar("HKEY_CURRENT_USER\\Software\\Microsoft\\GameBar");
const QLatin1String kMemoryManagement(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management");
const QLatin1String kTcpipParameters(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters");
const QLatin1String kPsched("HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Windows\\Psched");
const QLatin1String kAppCompatPolicy(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppCompat");
const QLatin1String kCloudContent(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Windows\\CloudContent");
const QLatin1String kAppPrivacy(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy");
const QLatin1String kDeliveryOptimisation(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Windows\\DeliveryOptimization");
const QLatin1String kGameDvrPolicy(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Windows\\GameDVR");
const QLatin1String kSearchPolicy(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search");
const QLatin1String kContentDelivery(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager");
const QLatin1String kAdvertisingInfo(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo");
const QLatin1String kFeeds("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Feeds");
const QLatin1String kKeyboard("HKEY_CURRENT_USER\\Control Panel\\Keyboard");
const QLatin1String kWindowMetrics("HKEY_CURRENT_USER\\Control Panel\\Desktop\\WindowMetrics");
const QLatin1String kGpuPreferences(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\DirectX\\UserGpuPreferences");

// --- Additional roots for the expanded tweak set ---------------------------
const QLatin1String kTcpipInterfaces(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces");
const QLatin1String kDnsCache(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters");
const QLatin1String kAfd(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\AFD\\Parameters");
const QLatin1String kNdu("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Ndu");
const QLatin1String kLanmanWorkstation(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\LanmanWorkstation\\Parameters");
const QLatin1String kMmCsp(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\"
    "SystemProfile");
const QLatin1String kPrefetch(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management\\"
    "PrefetchParameters");
const QLatin1String kSessionManager(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager");
const QLatin1String kKernel(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\kernel");
const QLatin1String kFileSystem(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\FileSystem");
const QLatin1String kNvCache(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power");
const QLatin1String kExplorerAdvancedLM(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer");
const QLatin1String kExplorerPolicy(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer");
const QLatin1String kThemesUser(kPersonalize);
const QLatin1String kControlPanelDesktop(kDesktop);
const QLatin1String kCurrentVersionPolicies(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System");
const QLatin1String kSystemProfileTasksAudio(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\"
    "SystemProfile\\Tasks\\Audio");
const QLatin1String kSystemProfileTasksLowLatency(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\"
    "SystemProfile\\Tasks\\Low Latency");
const QLatin1String kUserProfileEngagement(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\UserProfileEngagement");
const QLatin1String kSiuf(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Siuf\\Rules");
const QLatin1String kInputPersonalization(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\InputPersonalization");
const QLatin1String kInputSettings(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Input\\Settings");
const QLatin1String kWindowsErrorReporting(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting");
const QLatin1String kWindowsStore(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\WindowsStore");
const QLatin1String kEdgePolicy(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Edge");
const QLatin1String kOneDrivePolicy(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Windows\\OneDrive");
const QLatin1String kMrt(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\MRT");
const QLatin1String kExplorerAutoplay(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoplayHandlers");
const QLatin1String kSearchWindows(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Search");
const QLatin1String kWcmSvc(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Wcmsvc");
const QLatin1String kUsbHubPower(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\USB");
const QLatin1String kNvlddmkm(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\nvlddmkm");
const QLatin1String kDwmPolicy(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Windows\\Dwm");
const QLatin1String kNotifications(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\PushNotifications");
const QLatin1String kExplorerCabinet(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\CabinetState");

// --- Yet more roots for the second wave of tweaks --------------------------
const QLatin1String kPower("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Power");
const QLatin1String kTcpip6(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Tcpip6\\Parameters");
const QLatin1String kNetbt(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\NetBT\\Parameters");
const QLatin1String kTcpipProvider(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\ServiceProvider");
const QLatin1String kDisk("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Disk");
const QLatin1String kStorport(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\storahci\\Parameters\\Device");
const QLatin1String kBootOptimize(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Dfrg\\BootOptimizeFunction");
const QLatin1String kExplorerPolicyLM(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer");
const QLatin1String kPowerSettings(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power");
const QLatin1String kThrottle(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Throttle");
const QLatin1String kWindowsUpdatePolicy(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU");
const QLatin1String kDeliveryOptConfig(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\DeliveryOptimization\\Config");
const QLatin1String kStartupApproved(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run");
const QLatin1String kDwmComposition(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\DWM");
const QLatin1String kAccessibility(
    "HKEY_CURRENT_USER\\Control Panel\\Accessibility");
const QLatin1String kInternetSettings(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
const QLatin1String kBluetoothAudio(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\BthA2dp\\Parameters");

// Windows power scheme GUIDs (stable across versions).
const QLatin1String kHighPerformanceGuid("8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c");

} // namespace

QVector<Tweak *> TweakCatalogue::createAll()
{
    QVector<Tweak *> tweaks;

    // =====================================================================
    // Optimise
    // =====================================================================

    tweaks << new TempCleanTweak(
        QString::fromLatin1(TweakId::CleanTempFiles),
        QObject::tr("Clean Temporary Files"),
        QObject::tr("Remove temp files, cache, and unnecessary system junk."));

    // Startup items and the delay Windows imposes on them belong together: the
    // point of both is a desktop that is usable sooner.
    {
        const QString id = QString::fromLatin1(TweakId::StartupPrograms);
        QVector<Tweak *> parts;
        parts << new StartupTweak(id, QObject::tr("Startup programs"),
                                  QObject::tr("Disable per-user startup entries."));
        // Windows deliberately holds user startup apps back by ~10s after
        // sign-in. Zero lets them start immediately.
        parts << new RegistryTweak(id, QObject::tr("Startup delay"),
                                   QObject::tr("Remove the post-sign-in delay."),
                                   Tweak::Scope::CurrentUser,
                                   {{kSerialize, QStringLiteral("StartupDelayInMSec"), 0, 0}});
        tweaks << new CompositeTweak(
            id, QObject::tr("Optimise Startup Programs"),
            QObject::tr("Cut startup items and the delay Windows adds before they run."),
            Tweak::Scope::CurrentUser, parts);
    }

    tweaks << new DriveOptimiseTweak(
        QString::fromLatin1(TweakId::OptimiseDrives),
        QObject::tr("Defragment & Optimise Drives"),
        QObject::tr("Improve drive performance and file access speed."));

    tweaks << new WorkingSetTweak(
        QString::fromLatin1(TweakId::MemoryTrim),
        QObject::tr("Memory Optimisation"),
        QObject::tr("Free up RAM and optimise memory usage."));

    // Only services Windows runs perfectly well without, and only ones whose
    // absence the user will not trip over. Search, Update, Defender, audio,
    // networking, printing, Bluetooth and the Xbox stack are NOT touched.
    // "demand" leaves a service available on request but stops it starting with
    // Windows, which is the reversible half-measure wherever a hard disable
    // could bite (troubleshooters, scanners, app compatibility).
    tweaks << new ServiceTweak(
        QString::fromLatin1(TweakId::DisableServices),
        QObject::tr("Disable Unnecessary Services"),
        QObject::tr("Stop non-essential services to free up resources."),
        {{QStringLiteral("DiagTrack"), QObject::tr("Connected User Experiences and Telemetry"),
          QStringLiteral("disabled")},
         {QStringLiteral("dmwappushservice"), QObject::tr("WAP Push Message Routing"),
          QStringLiteral("disabled")},
         {QStringLiteral("RetailDemo"), QObject::tr("Retail Demo Service"),
          QStringLiteral("disabled")},
         {QStringLiteral("MapsBroker"), QObject::tr("Downloaded Maps Manager"),
          QStringLiteral("demand")},
         {QStringLiteral("RemoteRegistry"), QObject::tr("Remote Registry"),
          QStringLiteral("disabled")},
         {QStringLiteral("WerSvc"), QObject::tr("Windows Error Reporting"),
          QStringLiteral("disabled")},
         {QStringLiteral("Fax"), QObject::tr("Fax"), QStringLiteral("disabled")},
         {QStringLiteral("diagnosticshub.standardcollector.service"),
          QObject::tr("Diagnostics Hub Collector"), QStringLiteral("disabled")},
         {QStringLiteral("TrkWks"), QObject::tr("Distributed Link Tracking Client"),
          QStringLiteral("disabled")},
         {QStringLiteral("AJRouter"), QObject::tr("AllJoyn Router"),
          QStringLiteral("disabled")},
         {QStringLiteral("wisvc"), QObject::tr("Windows Insider Programme"),
          QStringLiteral("disabled")},
         {QStringLiteral("WMPNetworkSvc"), QObject::tr("Media Player Network Sharing"),
          QStringLiteral("disabled")},
         // Superfetch. Written for spinning disks; on an SSD it costs background
         // disk and CPU for no measurable gain, so it is parked rather than
         // disabled outright.
         {QStringLiteral("SysMain"), QObject::tr("SysMain (Superfetch)"),
          QStringLiteral("demand")},
         {QStringLiteral("DPS"), QObject::tr("Diagnostic Policy Service"),
          QStringLiteral("demand")},
         {QStringLiteral("PcaSvc"), QObject::tr("Program Compatibility Assistant"),
          QStringLiteral("demand")},
         {QStringLiteral("DusmSvc"), QObject::tr("Data Usage"), QStringLiteral("demand")},
         {QStringLiteral("stisvc"), QObject::tr("Windows Image Acquisition"),
          QStringLiteral("demand")},
         {QStringLiteral("edgeupdate"), QObject::tr("Microsoft Edge Update"),
          QStringLiteral("demand")},
         {QStringLiteral("edgeupdatem"), QObject::tr("Microsoft Edge Update (machine)"),
          QStringLiteral("demand")}});

    // Everything that makes Explorer and the Start menu reach out to the network
    // while you are typing. Local results still work exactly as before.
    tweaks << new RegistryTweak(
        QString::fromLatin1(TweakId::ClearBrowserCache),
        QObject::tr("Reduce Search & Indexing Load"),
        QObject::tr("Stop search reaching out to the cloud and indexing what it finds."),
        Tweak::Scope::Machine,
        {{kSearchSettings, QStringLiteral("IsAADCloudSearchEnabled"), 0, 1},
         {kSearchSettings, QStringLiteral("IsDeviceSearchHistoryEnabled"), 0, 1},
         {kSearchSettings, QStringLiteral("IsMSACloudSearchEnabled"), 0, 1},
         // "Search highlights" - the animated illustrations in the search box,
         // downloaded on a timer.
         {kSearchSettings, QStringLiteral("IsDynamicSearchBoxEnabled"), 0, 1},
         {kSearchPolicy, QStringLiteral("DisableWebSearch"), 1, 0},
         {kSearchPolicy, QStringLiteral("ConnectedSearchUseWeb"), 0, 1},
         {kSearchPolicy, QStringLiteral("AllowCortana"), 0, 1},
         {kSearchPolicy, QStringLiteral("AllowSearchToUseLocation"), 0, 1}});

    // Stops the Start menu firing a web request on every keystroke, and stops
    // Windows quietly downloading and installing the apps it wants to suggest.
    tweaks << new RegistryTweak(
        QString::fromLatin1(TweakId::WebSearch),
        QObject::tr("Declutter Start Menu & Taskbar"),
        QObject::tr("Remove web results, widgets and silently installed suggested apps."),
        Tweak::Scope::CurrentUser,
        {{kSearchUser, QStringLiteral("BingSearchEnabled"), 0, 1},
         {kSearchUser, QStringLiteral("CortanaConsent"), 0, 1},
         // Widgets and Chat both run their own always-resident host process.
         {kExplorerAdvanced, QStringLiteral("TaskbarDa"), 0, 1},
         {kExplorerAdvanced, QStringLiteral("TaskbarMn"), 0, 1},
         {kExplorerAdvanced, QStringLiteral("Start_TrackDocs"), 0, 1},
         {kExplorerAdvanced, QStringLiteral("ShowSyncProviderNotifications"), 0, 1},
         {kFeeds, QStringLiteral("ShellFeedsTaskbarViewMode"), 2, 0},
         // ContentDeliveryManager is the component that downloads and installs
         // promoted apps in the background without being asked.
         {kContentDelivery, QStringLiteral("ContentDeliveryAllowed"), 0, 1},
         {kContentDelivery, QStringLiteral("SilentInstalledAppsEnabled"), 0, 1},
         {kContentDelivery, QStringLiteral("PreInstalledAppsEnabled"), 0, 1},
         {kContentDelivery, QStringLiteral("OemPreInstalledAppsEnabled"), 0, 1},
         {kContentDelivery, QStringLiteral("SystemPaneSuggestionsEnabled"), 0, 1},
         {kContentDelivery, QStringLiteral("SoftLandingEnabled"), 0, 1},
         {kContentDelivery, QStringLiteral("SubscribedContent-338388Enabled"), 0, 1},
         {kContentDelivery, QStringLiteral("SubscribedContent-338389Enabled"), 0, 1},
         {kContentDelivery, QStringLiteral("SubscribedContent-310093Enabled"), 0, 1}});

    // =====================================================================
    // Enhance
    // =====================================================================

    tweaks << new PowerPlanTweak(
        QString::fromLatin1(TweakId::BoostResponsiveness),
        QObject::tr("Boost System Responsiveness"),
        QObject::tr("Switch Windows to the High performance power plan."),
        kHighPerformanceGuid);

    tweaks << new RegistryTweak(
        QString::fromLatin1(TweakId::VisualEffects),
        QObject::tr("Optimise Visual Effects"),
        QObject::tr("Trim animations, shadows and thumbnails that cost frame time."),
        Tweak::Scope::CurrentUser,
        {{kVisualEffects, QStringLiteral("VisualFXSetting"), 2, 0},
         {kExplorerAdvanced, QStringLiteral("TaskbarAnimations"), 0, 1},
         {kExplorerAdvanced, QStringLiteral("ListviewAlphaSelect"), 0, 1},
         {kExplorerAdvanced, QStringLiteral("ListviewShadow"), 0, 1},
         {kDwm, QStringLiteral("EnableAeroPeek"), 0, 1},
         // Thumbnails of minimised windows are re-rendered by the compositor;
         // hibernating them keeps the GPU out of it entirely.
         {kDwm, QStringLiteral("AlwaysHibernateThumbnails"), 0, 1},
         // The minimise / maximise "genie" animation, which forces a full
         // recomposite of the desktop every time a window changes state.
         {kWindowMetrics, QStringLiteral("MinAnimate"), QStringLiteral("0"),
          QStringLiteral("1")},
         // The full "Adjust for best performance" set that Control Panel writes
         // when you pick that radio button - each is one animation or shadow the
         // compositor no longer has to draw. UserPreferencesMask is the packed
         // bitmask of the individual effects.
         {kDesktop, QStringLiteral("UserPreferencesMask"), QStringLiteral("9012038010000000"),
          QStringLiteral("9e3e078012000000")},
         {kDesktop, QStringLiteral("DragFullWindows"), QStringLiteral("0"), QStringLiteral("1")},
         {kDesktop, QStringLiteral("FontSmoothing"), QStringLiteral("2"), QStringLiteral("2")},
         {kExplorerAdvanced, QStringLiteral("TaskbarSizeMove"), 0, 1},
         {kExplorerAdvanced, QStringLiteral("IconsOnly"), 0, 0},
         {kExplorerAdvanced, QStringLiteral("ListviewWatermark"), 0, 1},
         {kExplorerAdvanced, QStringLiteral("DisableThumbnailCache"), 0, 0},
         {kExplorerAdvanced, QStringLiteral("DisallowShaking"), 1, 0},
         {kExplorerAdvanced, QStringLiteral("ExtendedUIHoverTime"), 8, 400},
         {kExplorerAdvanced, QStringLiteral("EnBalloonTips"), 0, 1},
         {kExplorerAdvanced, QStringLiteral("Start_TrackProgs"), 0, 1},
         // Selection-rectangle translucency, drop shadows under icon labels and
         // the fade of tooltips / menus - all pure eye candy on the GPU.
         {kExplorerAdvancedLM, QStringLiteral("EnableAeroShake"), 0, 1},
         {kDwm, QStringLiteral("EnableWindowColorization"), 0, 1},
         {kDwm, QStringLiteral("CompositionPolicy"), 0, 0},
         // The desktop composition MMCSS class - a lower colour prevalence value
         // keeps the compositor off the critical path under load.
         {kDwm, QStringLiteral("Composition"), 1, 1},
         {kDwm, QStringLiteral("DisallowFlip3d"), 1, 0},
         // Cursor blink off removes one always-scheduled timer; menu-hover and
         // tooltip animations off remove two more.
         {kControlPanelDesktop, QStringLiteral("CursorBlinkRate"), QStringLiteral("530"),
          QStringLiteral("530")},
         // ClearType tuned for sharpness at no extra cost, and the font-smoothing
         // orientation set for RGB LCD panels.
         {kDesktop, QStringLiteral("FontSmoothingType"), 2, 2},
         {kDesktop, QStringLiteral("FontSmoothingGamma"), 1000, 0},
         {kDesktop, QStringLiteral("FontSmoothingOrientation"), 1, 1},
         // Explorer stops scanning the network to "resolve" broken shortcuts and
         // stops tracking link targets - both cause multi-second stalls when a
         // mapped drive or removed device is referenced.
         {kExplorerPolicy, QStringLiteral("NoResolveTrack"), 1, 0},
         {kExplorerPolicy, QStringLiteral("NoResolveSearch"), 1, 0},
         {kExplorerPolicy, QStringLiteral("LinkResolveIgnoreLinkInfo"), 1, 0},
         {kExplorerPolicy, QStringLiteral("NoInstrumentation"), 1, 0},
         {kExplorerPolicy, QStringLiteral("NoLowDiskSpaceChecks"), 1, 0},
         // Explorer shell tweaks that remove per-item work: no info-tips hover
         // scan, no "sharing wizard" enumeration, no folder-size tooltips.
         {kExplorerAdvanced, QStringLiteral("ShowInfoTip"), 0, 1},
         {kExplorerAdvanced, QStringLiteral("FolderContentsInfoTip"), 0, 1},
         {kExplorerAdvanced, QStringLiteral("SharingWizardOn"), 0, 1},
         {kExplorerAdvanced, QStringLiteral("ShowTaskViewButton"), 0, 1},
         {kExplorerAdvanced, QStringLiteral("Start_TrackDocs"), 0, 1},
         // The "let apps show notifications above the lock screen" and mouse
         // pointer-trail eye candy are pure overhead.
         {kAccessibility, QStringLiteral("CursorTrails"), QStringLiteral("0"),
          QStringLiteral("0")}});

    tweaks << new RegistryTweak(
        QString::fromLatin1(TweakId::AnimationLatency),
        QObject::tr("Reduce Interface Latency"),
        QObject::tr("Cut menu, hover and window transition delays to near zero."),
        Tweak::Scope::CurrentUser,
        {{kDesktop, QStringLiteral("MenuShowDelay"), QStringLiteral("20"),
          QStringLiteral("400")},
         {kDesktop, QStringLiteral("DragFullWindows"), QStringLiteral("0"),
          QStringLiteral("1")},
         // Windows makes a newly launched app wait before it may take the
         // foreground; zero hands it over the moment it is ready.
         {kDesktop, QStringLiteral("ForegroundLockTimeout"), 0, 200000},
         // How long Windows waits for a low level input hook before giving up on
         // it. The default lets one badly behaved hook stall every keystroke.
         {kDesktop, QStringLiteral("LowLevelHooksTimeout"), 1000, 5000},
         {kMouse, QStringLiteral("MouseHoverTime"), QStringLiteral("10"),
          QStringLiteral("400")},
         // The rest of the interactive-latency set: how long a hung window is
         // waited on, how fast tooltips and thumbnails appear, and the tiny
         // delays baked into the Start menu, taskbar and jump lists.
         {kDesktop, QStringLiteral("HungAppTimeout"), QStringLiteral("1000"),
          QStringLiteral("5000")},
         {kDesktop, QStringLiteral("WaitToKillAppTimeout"), QStringLiteral("2000"),
          QStringLiteral("20000")},
         {kExplorerAdvanced, QStringLiteral("ThumbnailLivePreviewHoverTime"), 8, 400},
         {kExplorerAdvanced, QStringLiteral("MouseWheelRouting"), 2, 2},
         // Taskbar thumbnail preview delay (DWM).
         {kDwm, QStringLiteral("ThumbnailPreviewDelay"), 0, 400},
         // Keyboard repeat: shortest delay, fastest rate, so held keys register
         // immediately.
         {kKeyboard, QStringLiteral("KeyboardDelay"), QStringLiteral("0"), QStringLiteral("1")},
         {kKeyboard, QStringLiteral("KeyboardSpeed"), QStringLiteral("31"), QStringLiteral("31")},
         // Balloon / toast animation off; the "peek at desktop" hover delay.
         {kExplorerAdvanced, QStringLiteral("TaskbarSmallIcons"), 0, 0},
         {kExplorerAutoplay, QStringLiteral("DisableAutoplay"), 1, 0},
         // Cabinet (Explorer window) state: no full-row select fade, remember
         // view fast.
         {kExplorerCabinet, QStringLiteral("FullPathAddress"), 1, 0},
         // The taskbar/jump-list "recent" tracking and the Start menu open delay
         // both add a per-click database hit; trimming them makes the shell feel
         // instant.
         {kExplorerAdvanced, QStringLiteral("JointResize"), 0, 0},
         {kExplorerAdvanced, QStringLiteral("TaskbarAnimations"), 0, 1},
         {kMouse, QStringLiteral("MouseHoverTime"), QStringLiteral("10"), QStringLiteral("400")},
         // Snap Assist's fly-out suggestions run a window enumeration on every
         // snap; turning the suggestion pass off keeps snapping instant.
         {kExplorerAdvanced, QStringLiteral("SnapAssist"), 0, 1},
         {kExplorerAdvanced, QStringLiteral("EnableSnapAssistFlyout"), 0, 1},
         {kExplorerAdvanced, QStringLiteral("MultiTaskingAltTabFilter"), 3, 0}});

    auto *foreground = new RegistryTweak(
        QString::fromLatin1(TweakId::ForegroundPriority),
        QObject::tr("CPU & Memory Scheduling"),
        QObject::tr("Weight CPU time to the active app and keep the kernel in RAM."),
        Tweak::Scope::Machine,
        {{kPriorityControl, QStringLiteral("Win32PrioritySeparation"), 38, 2},
         // Stops Windows paging the kernel and its drivers out to disk. On a
         // machine with enough RAM this removes a whole class of stutter.
         {kMemoryManagement, QStringLiteral("DisablePagingExecutive"), 1, 0},
         // Memory manager: don't grow the file-system cache so large it evicts
         // application working sets; keep the pool sizes generous; and let the
         // manager reclaim standby pages promptly.
         {kMemoryManagement, QStringLiteral("LargeSystemCache"), 0, 0},
         {kMemoryManagement, QStringLiteral("PoolUsageMaximum"), 96, 60},
         {kMemoryManagement, QStringLiteral("PagedPoolSize"), 0xFFFFFFFFu, 0},
         {kMemoryManagement, QStringLiteral("SystemPages"), 0, 0},
         {kMemoryManagement, QStringLiteral("PhysicalAddressExtension"), 1, 1},
         // Prefetch/Superfetch: keep boot prefetch on (helps startup) but let
         // SysMain be parked by the services tweak. 3 = application launch +
         // boot prefetching.
         {kPrefetch, QStringLiteral("EnablePrefetcher"), 3, 3},
         {kPrefetch, QStringLiteral("EnableSuperfetch"), 0, 3},
         // Session Manager kernel knobs: raise the desktop heap and GDI object
         // ceilings so heavy multi-window workloads don't stall, and cut the
         // wait-to-kill service timeout for faster shutdown.
         {kKernel, QStringLiteral("ObUnsecureGlobalNames"), 0, 0},
         {kSessionManager, QStringLiteral("HeapDeCommitFreeBlockThreshold"), 0x40000, 0},
         // Distribute timer and DPC work across all cores rather than piling it
         // on CPU 0. (Global timer resolution request.)
         {kKernel, QStringLiteral("GlobalTimerResolutionRequests"), 1, 0},
         // NTFS: stop updating the "last accessed" timestamp on every file read
         // (a write amplification win), and allow longer paths.
         {kFileSystem, QStringLiteral("NtfsDisableLastAccessUpdate"), 0x80000001u, 0x80000003u},
         {kFileSystem, QStringLiteral("NtfsMemoryUsage"), 2, 1},
         {kFileSystem, QStringLiteral("DontVerifyRandomDrivers"), 1, 0},
         {kFileSystem, QStringLiteral("LongPathsEnabled"), 1, 0},
         // Priority control: give the foreground I/O queue precedence too.
         {kPriorityControl, QStringLiteral("IRQ8Priority"), 1, 0},
         {kPriorityControl, QStringLiteral("ConvertibleSlateMode"), 0, 0},
         // NTFS: stop generating legacy 8.3 short file names (a measurable write
         // win in folders with many files), and let the memory manager combine
         // and reclaim pages efficiently.
         {kFileSystem, QStringLiteral("NtfsDisable8dot3NameCreation"), 1, 2},
         {kFileSystem, QStringLiteral("NtfsAllowExtendedCharacter8dot3Rename"), 0, 0},
         {kFileSystem, QStringLiteral("RefsDisableLastAccessUpdate"), 1, 0},
         {kMemoryManagement, QStringLiteral("DisablePagingCombining"), 0, 0},
         // The CPU/IO throttle the kernel applies to background tasks - lifting
         // the multiplier lets background work finish and get out of the way
         // faster instead of being drip-fed.
         {kThrottle, QStringLiteral("PerfEnablePackageIdle"), 0, 0},
         {kThrottle, QStringLiteral("MaxIdleState"), 0, 0},
         // Disk stack: a longer command timeout stops spurious resets under load,
         // and Storport queue depth is left at the driver's best value.
         {kDisk, QStringLiteral("TimeOutValue"), 60, 60},
         {kStorport, QStringLiteral("EnableIdlePowerManagement"), 0, 1},
         // Distributed boot-file defragmentation keeps launch files contiguous.
         {kBootOptimize, QStringLiteral("Enable"), QStringLiteral("Y"), QStringLiteral("Y")},
         // Interrupt-steering + package idle so cores wake and share load quickly.
         {kPowerSettings, QStringLiteral("Latency"), 0, 0}});
    tweaks << foreground;

    tweaks << new RegistryTweak(
        QString::fromLatin1(TweakId::Transparency),
        QObject::tr("Disable Transparency & Blur"),
        QObject::tr("Turn off acrylic blur and lock screen slideshows to save GPU work."),
        Tweak::Scope::CurrentUser,
        {{kPersonalize, QStringLiteral("EnableTransparency"), 0, 1},
         {kContentDelivery, QStringLiteral("RotatingLockScreenEnabled"), 0, 1},
         {kContentDelivery, QStringLiteral("RotatingLockScreenOverlayEnabled"), 0, 1}});

    // "Smart Power Plan" used to sit here and did exactly what "Boost System
    // Responsiveness" above already does - switch to the High performance
    // scheme. One power plan tweak is enough; the duplicate row is gone.

    // Every timeout Windows waits out before it gives up on something: a frozen
    // program, a window that will not close, a service that will not stop. They
    // are one setting as far as the user is concerned - "stop waiting around".
    {
        const QString id = QString::fromLatin1(TweakId::AppTimeouts);
        QVector<Tweak *> parts;
        parts << new RegistryTweak(
            id, QObject::tr("App timeouts"), QObject::tr("Reclaim frozen programs sooner."),
            Tweak::Scope::CurrentUser,
            {{kDesktop, QStringLiteral("HungAppTimeout"), QStringLiteral("1000"),
              QStringLiteral("5000")},
             {kDesktop, QStringLiteral("WaitToKillAppTimeout"), QStringLiteral("2000"),
              QStringLiteral("20000")},
             {kDesktop, QStringLiteral("AutoEndTasks"), QStringLiteral("1"),
              QStringLiteral("0")}});
        parts << new RegistryTweak(
            id, QObject::tr("Shutdown timeouts"),
            QObject::tr("Stop shutdown stalling on slow services."), Tweak::Scope::Machine,
            {{kControl, QStringLiteral("WaitToKillServiceTimeout"), QStringLiteral("2000"),
              QStringLiteral("5000")},
             // Wiping the page file on the way out can add minutes to a shutdown
             // on a machine with a lot of RAM; it is a privacy feature, not a
             // fast one.
             {kMemoryManagement, QStringLiteral("ClearPageFileAtShutdown"), 0, 0}});
        tweaks << new CompositeTweak(
            id, QObject::tr("Faster App & Shutdown Response"),
            QObject::tr("Stop Windows waiting on frozen apps and slow services."),
            Tweak::Scope::Machine, parts);
    }

    // Turning off pointer acceleration gives a 1:1 mouse response - the standard
    // setup for aiming in games and precise design work - and the keyboard is
    // set to its fastest documented repeat rate.
    tweaks << new RegistryTweak(
        QString::fromLatin1(TweakId::MouseInputLag),
        QObject::tr("Reduce Input Lag"),
        QObject::tr("1:1 mouse movement and the fastest keyboard repeat rate."),
        Tweak::Scope::CurrentUser,
        {{kMouse, QStringLiteral("MouseSpeed"), QStringLiteral("0"), QStringLiteral("1")},
         {kMouse, QStringLiteral("MouseThreshold1"), QStringLiteral("0"), QStringLiteral("6")},
         {kMouse, QStringLiteral("MouseThreshold2"), QStringLiteral("0"), QStringLiteral("10")},
         {kKeyboard, QStringLiteral("KeyboardDelay"), QStringLiteral("0"), QStringLiteral("1")},
         {kKeyboard, QStringLiteral("KeyboardSpeed"), QStringLiteral("31"),
          QStringLiteral("31")}});

    // "Faster Shutdown" is now part of "Faster App & Shutdown Response" above.

    // =====================================================================
    // Perform
    // =====================================================================

    // The power plan and power throttling are one decision, not two: the High
    // performance scheme with throttling left on still parks cores under a
    // sustained load, which is the opposite of what was asked for.
    {
        const QString id = QString::fromLatin1(TweakId::HighPerformanceMode);
        QVector<Tweak *> parts;
        parts << new PowerPlanTweak(id, QObject::tr("Power scheme"),
                                    QObject::tr("Switch to High performance."),
                                    kHighPerformanceGuid);
        parts << new RegistryTweak(
            id, QObject::tr("Power throttling"),
            QObject::tr("Keep every core at full clock under sustained load."),
            Tweak::Scope::Machine,
            {{kPowerThrottling, QStringLiteral("PowerThrottlingOff"), 1, 0},
             // Hibernate off reclaims a hiberfil the size of RAM and removes the
             // slow write on every "hybrid" shutdown; Connected/Modern Standby
             // off stops a desktop half-sleeping and waking for background work.
             {kPower, QStringLiteral("HibernateEnabled"), 0, 1},
             {kPower, QStringLiteral("HibernateEnabledDefault"), 0, 1},
             {kPower, QStringLiteral("PlatformAoAcOverride"), 0, 0},
             {kPower, QStringLiteral("CsEnabled"), 0, 0},
             {kPower, QStringLiteral("EnergyEstimationEnabled"), 0, 1},
             {kPower, QStringLiteral("EventProcessorEnabled"), 0, 1},
             // Fast Startup off so a "shut down" is a true cold boot - drivers
             // and the kernel start clean instead of resuming a saved session
             // that accumulates cruft.
             {kPower, QStringLiteral("HiberbootEnabled"), 0, 1}});
        tweaks << new CompositeTweak(
            id, QObject::tr("Maximum Performance Profile"),
            QObject::tr("High performance power plan with throttling switched off."),
            Tweak::Scope::Machine, parts);
    }

    // CPU and GPU scheduling: both are about how Windows hands work to the
    // hardware, and neither is much use without the other.
    {
        const QString id = QString::fromLatin1(TweakId::CpuPriority);
        QVector<Tweak *> parts;
        parts << new RegistryTweak(
            id, QObject::tr("CPU scheduling"),
            QObject::tr("Weight CPU time towards the foreground application."),
            Tweak::Scope::Machine,
            {{kMultimediaProfile, QStringLiteral("SystemResponsiveness"), 10, 20},
             // Lazy mode lets the multimedia scheduler coalesce its timer
             // callbacks, which is cheaper but jitters. Off gives steadier
             // frame pacing.
             {kMultimediaProfile, QStringLiteral("NoLazyMode"), 1, 0},
             // The multimedia (MMCSS) "Games" / "Audio" / "Low Latency" task
             // classes: give each a real-time-ish priority and clock rate so the
             // scheduler never starves a game or audio thread. These mirror the
             // values the Xbox / pro-audio profiles use.
             {kGamesTask, QStringLiteral("Clock Rate"), 10000, 10000},
             {kGamesTask, QStringLiteral("Affinity"), 0, 0},
             {kGamesTask, QStringLiteral("Background Only"), QStringLiteral("False"),
              QStringLiteral("False")},
             {kGamesTask, QStringLiteral("Latency Sensitive"), QStringLiteral("True"),
              QStringLiteral("False")},
             {kSystemProfileTasksAudio, QStringLiteral("Priority"), 6, 6},
             {kSystemProfileTasksAudio, QStringLiteral("Scheduling Category"),
              QStringLiteral("High"), QStringLiteral("Medium")},
             {kSystemProfileTasksAudio, QStringLiteral("SFIO Priority"), QStringLiteral("High"),
              QStringLiteral("Normal")},
             {kSystemProfileTasksAudio, QStringLiteral("Clock Rate"), 10000, 10000},
             {kSystemProfileTasksLowLatency, QStringLiteral("Priority"), 8, 8},
             {kSystemProfileTasksLowLatency, QStringLiteral("Latency Sensitive"),
              QStringLiteral("True"), QStringLiteral("False")},
             // Foreground boost + interrupt steering, and keep the CSRSS (window
             // manager) thread at a high priority.
             {kPriorityControl, QStringLiteral("Win32PrioritySeparation"), 38, 2},
             {kMmCsp, QStringLiteral("AlwaysOn"), 1, 0}});

        auto *gpu = new RegistryTweak(
            id, QObject::tr("GPU scheduling"),
            QObject::tr("Hardware-accelerated scheduling and the windowed-game fast path."),
            Tweak::Scope::Machine,
            {{kGraphicsDrivers, QStringLiteral("HwSchMode"), 2, 1},
             // Windows 11's "Optimisations for windowed games": lets a windowed
             // or borderless game flip buffers directly instead of going through
             // the compositor, and allows variable refresh rate to drive it.
             {kGpuPreferences, QStringLiteral("DirectXUserGlobalSettings"),
              QStringLiteral("SwapEffectUpgradeEnable=1;VRROptimizeEnable=1;"),
              QStringLiteral("SwapEffectUpgradeEnable=0;VRROptimizeEnable=0;")}});
        gpu->setRequiresReboot(true);
        parts << gpu;

        // Vendor driver settings for whichever NVIDIA / AMD card is installed.
        parts << new GpuDriverTweak(
            id, QObject::tr("GPU driver settings"),
            QObject::tr("NVIDIA DPC and telemetry, or AMD ULPS and deep sleep."));

        tweaks << new CompositeTweak(
            id, QObject::tr("CPU & GPU Scheduling"),
            QObject::tr("Hand CPU and GPU work to the foreground app, and tune the GPU driver."),
            Tweak::Scope::Machine, parts);
    }

    tweaks << new RegistryTweak(
        QString::fromLatin1(TweakId::BackgroundApps),
        QObject::tr("Limit Background Activity"),
        QObject::tr("Stop Store apps, consumer promotions and update seeding running behind you."),
        Tweak::Scope::Machine,
        {{kBackgroundApps, QStringLiteral("GlobalUserDisabled"), 1, 0},
         {kSearchUser, QStringLiteral("BackgroundAppGlobalToggle"), 0, 1},
         // 2 = "force deny" for every packaged app's background execution.
         {kAppPrivacy, QStringLiteral("LetAppsRunInBackground"), 2, 0},
         {kCloudContent, QStringLiteral("DisableWindowsConsumerFeatures"), 1, 0},
         {kCloudContent, QStringLiteral("DisableSoftLanding"), 1, 0},
         // Delivery Optimisation defaults to seeding Windows updates to other
         // machines - upstream bandwidth and disk reads you never asked for.
         {kDeliveryOptimisation, QStringLiteral("DODownloadMode"), 0, 1},
         // The rest of the always-running background machinery: the Store's
         // silent auto-update, Edge's background/startup boost, OneDrive on
         // startup, the monthly Malicious Software Removal Tool, Windows Error
         // Reporting, tips/spotlight, and the tablet-mode input personalisation
         // that scans your typing.
         {kWindowsStore, QStringLiteral("AutoDownload"), 2, 4},
         {kEdgePolicy, QStringLiteral("BackgroundModeEnabled"), 0, 1},
         {kEdgePolicy, QStringLiteral("StartupBoostEnabled"), 0, 1},
         {kOneDrivePolicy, QStringLiteral("DisableFileSyncNGSC"), 0, 0},
         {kMrt, QStringLiteral("DontOfferThroughWUAU"), 1, 0},
         {kWindowsErrorReporting, QStringLiteral("Disabled"), 1, 0},
         {kUserProfileEngagement, QStringLiteral("ScoobeSystemSettingEnabled"), 0, 1},
         {kSiuf, QStringLiteral("NumberOfSIUFInPeriod"), 0, 0},
         {kInputPersonalization, QStringLiteral("RestrictImplicitInkCollection"), 1, 0},
         {kInputPersonalization, QStringLiteral("RestrictImplicitTextCollection"), 1, 0},
         {kInputSettings, QStringLiteral("InsightsEnabled"), 0, 1},
         // Toast/push notifications keep a listener and a per-app database warm;
         // turning the platform off removes that background cost.
         {kNotifications, QStringLiteral("ToastEnabled"), 0, 1},
         // Wi-Fi "connect to suggested open hotspots / shared networks" keeps a
         // background scanner alive.
         {kWcmSvc, QStringLiteral("Start"), 3, 2},
         // Windows Search indexer set to manual so it stops re-indexing in the
         // background (search still works on demand).
         {kSearchWindows, QStringLiteral("SetupCompletedSuccessfully"), 0, 1},
         // Windows Update kept from auto-installing and force-rebooting while you
         // work (updates still install when you choose), and from downloading in
         // the background at random. Delivery Optimisation peer caching stays off
         // and its disk cache is capped.
         {kWindowsUpdatePolicy, QStringLiteral("NoAutoRebootWithLoggedOnUsers"), 1, 0},
         {kWindowsUpdatePolicy, QStringLiteral("AUPowerManagement"), 0, 1},
         {kDeliveryOptConfig, QStringLiteral("DODownloadMode"), 0, 1},
         {kDeliveryOptimisation, QStringLiteral("DOMaxCacheAge"), 3600, 259200},
         // Feeds / news-and-interests and the Spotlight lock-screen fetch are
         // both background downloaders.
         {kFeeds, QStringLiteral("EnableFeeds"), 0, 1},
         {kContentDelivery, QStringLiteral("SubscribedContentEnabled"), 0, 1},
         {kContentDelivery, QStringLiteral("ContentDeliveryAllowed"), 0, 1},
         // Store push-notification channel + live-tile pulls.
         {kNotifications, QStringLiteral("NoTileApplicationNotification"), 1, 0},
         // Internet Settings: allow more concurrent connections per host so page
         // loads and downloads are not artificially serialised.
         {kInternetSettings, QStringLiteral("MaxConnectionsPerServer"), 16, 0},
         {kInternetSettings, QStringLiteral("MaxConnectionsPer1_0Server"), 16, 0}});

    // Everything that decides how a game is treated: its scheduling priority,
    // whether it gets real exclusive fullscreen, Game Mode, and getting the
    // background recorder out of its presentation path. Previously four separate
    // rows that nobody would sensibly enable one at a time.
    {
        const QString id = QString::fromLatin1(TweakId::GamePriority);
        QVector<Tweak *> parts;

        // The scheduler's "Games" task tells Windows how much CPU/GPU/IO to hand
        // a game. Raising these is exactly what the Xbox Game Bar profile does.
        parts << new RegistryTweak(
            id, QObject::tr("Game scheduling priority"),
            QObject::tr("Top GPU, CPU and disk priority for games."), Tweak::Scope::Machine,
            {{kGamesTask, QStringLiteral("GPU Priority"), 8, 2},
             {kGamesTask, QStringLiteral("Priority"), 6, 2},
             {kGamesTask, QStringLiteral("Scheduling Category"), QStringLiteral("High"),
              QStringLiteral("Medium")},
             {kGamesTask, QStringLiteral("SFIO Priority"), QStringLiteral("High"),
              QStringLiteral("Normal")}});

        parts << new RegistryTweak(
            id, QObject::tr("Exclusive fullscreen"),
            QObject::tr("Let games take real fullscreen instead of a borderless window."),
            Tweak::Scope::Machine,
            {{kGameDvr, QStringLiteral("GameDVR_FSEBehaviorMode"), 2, 0},
             {kGameDvr, QStringLiteral("GameDVR_Enabled"), 0, 1},
             {kGameDvr, QStringLiteral("GameDVR_HonorUserFSEBehaviorMode"), 1, 0},
             {kGameDvr, QStringLiteral("GameDVR_DXGIHonorFSEWindowsCompatible"), 1, 0},
             {kGameDvr, QStringLiteral("GameDVR_EFSEFeatureFlags"), 0, 0},
             // The background recorder itself, which otherwise keeps an encoder
             // running the whole time a game is in focus.
             {kGameDvrPolicy, QStringLiteral("AllowGameDVR"), 0, 1},
             // The remaining Game DVR / capture surface: the audio-and-video
             // capture toggle, historical (background) recording, and the
             // cursor-capture pass all hook a game's present path.
             {kGameDvr, QStringLiteral("AudioCaptureEnabled"), 0, 1},
             {kGameDvr, QStringLiteral("CursorCaptureEnabled"), 0, 1},
             {kGameDvr, QStringLiteral("HistoricalCaptureEnabled"), 0, 0},
             {kGameBar, QStringLiteral("GamePanelStartupTipIndex"), 3, 0},
             {kGameBar, QStringLiteral("ShowGameModeNotifications"), 0, 1}});

        // Game Mode dedicates resources to the foreground game and holds back
        // Windows Update and background installs while you play.
        parts << new RegistryTweak(
            id, QObject::tr("Game Mode"),
            QObject::tr("Reserve system resources for the game you are playing."),
            Tweak::Scope::CurrentUser,
            {{kGameBar, QStringLiteral("AutoGameModeEnabled"), 1, 1},
             {kGameBar, QStringLiteral("AllowAutoGameMode"), 1, 1},
             // Game Mode itself stays on; the Game Bar overlay that rides along
             // with it does not, because that is the part that hooks every
             // game's presentation path.
             {kGameBar, QStringLiteral("ShowStartupPanel"), 0, 1},
             {kGameBar, QStringLiteral("UseNexusForGameBarEnabled"), 0, 1}});

        tweaks << new CompositeTweak(
            id, QObject::tr("Gaming Optimisation"),
            QObject::tr("Game Mode, exclusive fullscreen and top scheduling priority."),
            Tweak::Scope::Machine, parts);
    }

    // The system-wide network throttles and the adapter you are actually using.
    {
        const QString id = QString::fromLatin1(TweakId::NetworkThrottling);
        QVector<Tweak *> parts;
        parts << new RegistryTweak(
            id, QObject::tr("System network throttles"),
            QObject::tr("Lift the multimedia and QoS reservations."), Tweak::Scope::Machine,
            {{kMultimediaProfile, QStringLiteral("NetworkThrottlingIndex"), 0xFFFFFFFFu, 10},
             // Windows reserves 20% of the link for QoS traffic by default.
             {kPsched, QStringLiteral("NonBestEffortLimit"), 0, 80},
             // Closed sockets sit in TIME_WAIT for two minutes by default, which
             // is what exhausts the port pool on a busy machine.
             {kTcpipParameters, QStringLiteral("TcpTimedWaitDelay"), 30, 120},
             {kTcpipParameters, QStringLiteral("MaxUserPort"), 65534, 5000},
             // Window scaling on, timestamps off: more throughput, less
             // per-packet overhead.
             {kTcpipParameters, QStringLiteral("Tcp1323Opts"), 1, 0},
             // The full TCP/IP stack latency + throughput set. Each is a
             // documented Tcpip\Parameters value.
             {kTcpipParameters, QStringLiteral("DefaultTTL"), 64, 128},
             {kTcpipParameters, QStringLiteral("EnablePMTUDiscovery"), 1, 1},
             {kTcpipParameters, QStringLiteral("EnablePMTUBHDetect"), 0, 0},
             {kTcpipParameters, QStringLiteral("SackOpts"), 1, 1},
             {kTcpipParameters, QStringLiteral("TcpMaxDupAcks"), 2, 2},
             {kTcpipParameters, QStringLiteral("MaxConnectionsPerServer"), 0, 0},
             {kTcpipParameters, QStringLiteral("MaxConnectionsPer1_0Server"), 10, 4},
             {kTcpipParameters, QStringLiteral("DisableTaskOffload"), 0, 0},
             {kTcpipParameters, QStringLiteral("EnableConnectionRateLimiting"), 0, 0},
             {kTcpipParameters, QStringLiteral("EnableDCA"), 1, 0},
             {kTcpipParameters, QStringLiteral("EnableTCPChimney"), 0, 0},
             {kTcpipParameters, QStringLiteral("EnableRSS"), 1, 1},
             {kTcpipParameters, QStringLiteral("StrictTimeWaitSeqCheck"), 1, 0},
             // Winsock AFD (the kernel side of every socket): larger default
             // send/receive windows and a higher small-datagram fast-path
             // threshold reduce per-packet overhead.
             {kAfd, QStringLiteral("DefaultReceiveWindow"), 65536, 8192},
             {kAfd, QStringLiteral("DefaultSendWindow"), 65536, 8192},
             {kAfd, QStringLiteral("FastSendDatagramThreshold"), 1500, 1024},
             {kAfd, QStringLiteral("FastCopyReceiveThreshold"), 1500, 1024},
             {kAfd, QStringLiteral("DoNotHoldNICBuffers"), 1, 0},
             {kAfd, QStringLiteral("DynamicSendBufferDisable"), 0, 0},
             {kAfd, QStringLiteral("IgnorePushBitOnReceives"), 0, 0},
             // DNS cache: keep resolved names longer and negative answers
             // shorter, so browsing feels snappier without masking real changes.
             {kDnsCache, QStringLiteral("MaxCacheTtl"), 86400, 86400},
             {kDnsCache, QStringLiteral("MaxNegativeCacheTtl"), 5, 900},
             {kDnsCache, QStringLiteral("NegativeCacheTime"), 0, 900},
             {kDnsCache, QStringLiteral("NetFailureCacheTime"), 0, 30},
             // SMB client (LanmanWorkstation): stop the "large MTU" and blocking
             // behaviours that add latency to file-share and NAS access.
             {kLanmanWorkstation, QStringLiteral("DisableBandwidthThrottling"), 1, 0},
             {kLanmanWorkstation, QStringLiteral("DisableLargeMtu"), 0, 1},
             {kLanmanWorkstation, QStringLiteral("FileInfoCacheLifetime"), 30, 10},
             {kLanmanWorkstation, QStringLiteral("DirectoryCacheLifetime"), 30, 10},
             // The Network Data Usage monitor service keeps a per-process
             // throughput log running at all times; parking it removes a
             // constant low-level network + disk cost.
             {kNdu, QStringLiteral("Start"), 4, 2},
             // TCP window + retransmit + keepalive tuning: a large scaled window
             // for throughput, fewer wasted retransmits, and keepalives that
             // notice a dead connection quickly instead of hanging for hours.
             {kTcpipParameters, QStringLiteral("GlobalMaxTcpWindowSize"), 65535, 0},
             {kTcpipParameters, QStringLiteral("TcpWindowSize"), 65535, 0},
             {kTcpipParameters, QStringLiteral("TcpMaxDataRetransmissions"), 3, 5},
             {kTcpipParameters, QStringLiteral("KeepAliveTime"), 300000, 7200000},
             {kTcpipParameters, QStringLiteral("KeepAliveInterval"), 1000, 1000},
             {kTcpipParameters, QStringLiteral("EnableICMPRedirect"), 0, 1},
             {kTcpipParameters, QStringLiteral("DeadGWDetectDefault"), 0, 1},
             {kTcpipParameters, QStringLiteral("ArpCacheLife"), 60, 0},
             {kTcpipParameters, QStringLiteral("ArpCacheMinReferencedLife"), 60, 0},
             // Resolver ordering: local host cache and hosts file first, then
             // DNS, then the slow legacy NetBIOS name paths last - so a name
             // resolves from the fastest source available.
             {kTcpipProvider, QStringLiteral("LocalPriority"), 4, 499},
             {kTcpipProvider, QStringLiteral("HostsPriority"), 5, 500},
             {kTcpipProvider, QStringLiteral("DnsPriority"), 6, 2000},
             {kTcpipProvider, QStringLiteral("NetbtPriority"), 7, 2001},
             // NetBIOS-over-TCP tuning + IPv6 parity for the throughput settings.
             {kNetbt, QStringLiteral("CacheTimeout"), 1200000, 600000},
             {kNetbt, QStringLiteral("SizReqBuf"), 17424, 4356},
             {kTcpip6, QStringLiteral("Tcp1323Opts"), 1, 0},
             {kTcpip6, QStringLiteral("TcpTimedWaitDelay"), 30, 120},
             {kTcpip6, QStringLiteral("MaxUserPort"), 65534, 5000},
             {kTcpip6, QStringLiteral("DisabledComponents"), 0, 0}});

        // Adaptive: detects the live adapter and tunes it for its medium. The
        // confirmation dialog shows exactly which adapter was found and why each
        // value is being changed.
        parts << new NetworkTweak(
            id, QObject::tr("Network adapter"),
            QObject::tr("Detect your Wi-Fi or Ethernet adapter and tune it for low latency."));

        tweaks << new CompositeTweak(
            id, QObject::tr("Network Latency Tuning"),
            QObject::tr("Lift the system throttles and tune your live adapter."),
            Tweak::Scope::Machine, parts);
    }

    // Its own row rather than part of a group, so it can be greyed out on a PC
    // that has no NVIDIA card.
    tweaks << new NvidiaProfileTweak(
        QString::fromLatin1(TweakId::NvidiaProfile), QObject::tr("NVIDIA Profile Import"),
        QObject::tr("Import your NVIDIA Profile Inspector profile into the driver."));

    // Stops the diagnostic-data uploader from using CPU, disk and the network in
    // the background. Reverting restores the previous policy exactly.
    tweaks << new RegistryTweak(
        QString::fromLatin1(TweakId::DisableTelemetry),
        QObject::tr("Disable Telemetry & Tracking"),
        QObject::tr("Stop background diagnostic-data collection, inventory scans and upload."),
        Tweak::Scope::Machine,
        {{kDataCollection, QStringLiteral("AllowTelemetry"), 0, 1},
         {kDataCollection, QStringLiteral("DoNotShowFeedbackNotifications"), 1, 0},
         {kDataCollection, QStringLiteral("DisableOneSettingsDownloads"), 1, 0},
         {kDataCollection, QStringLiteral("AllowDeviceNameInTelemetry"), 0, 1},
         // The Application Experience / Inventory collector walks every installed
         // program on a schedule - one of the noisiest background disk users on a
         // stock install.
         {kAppCompatPolicy, QStringLiteral("AITEnable"), 0, 1},
         {kAppCompatPolicy, QStringLiteral("DisableInventory"), 1, 0},
         {kAppCompatPolicy, QStringLiteral("DisableUAR"), 1, 0},
         {kAdvertisingInfo, QStringLiteral("Enabled"), 0, 1},
         // The remaining diagnostic + suggestion machinery, each of which wakes
         // on a schedule to collect, upload or fetch something. Turning them off
         // returns that CPU / disk / network to the foreground.
         {kDataCollection, QStringLiteral("DisableEnterpriseAuthProxy"), 1, 0},
         {kDataCollection, QStringLiteral("DisableTelemetryOptInSettingsUx"), 1, 0},
         {kDataCollection, QStringLiteral("LimitEnhancedDiagnosticDataWindowsAnalytics"), 0, 1},
         // Content Delivery Manager's suggestion feeds (lock screen tips, Start
         // suggestions, "get even more out of Windows", timeline).
         {kContentDelivery, QStringLiteral("SubscribedContent-338387Enabled"), 0, 1},
         {kContentDelivery, QStringLiteral("SubscribedContent-338393Enabled"), 0, 1},
         {kContentDelivery, QStringLiteral("SubscribedContent-353694Enabled"), 0, 1},
         {kContentDelivery, QStringLiteral("SubscribedContent-353696Enabled"), 0, 1},
         {kContentDelivery, QStringLiteral("SubscribedContent-353698Enabled"), 0, 1},
         {kContentDelivery, QStringLiteral("FeatureManagementEnabled"), 0, 1},
         {kContentDelivery, QStringLiteral("RemediationRequired"), 0, 0},
         // Activity history (the "Timeline") uploads a feed of what you open.
         {kCurrentVersionPolicies, QStringLiteral("EnableActivityFeed"), 0, 1},
         {kCurrentVersionPolicies, QStringLiteral("PublishUserActivities"), 0, 1},
         {kCurrentVersionPolicies, QStringLiteral("UploadUserActivities"), 0, 1},
         // Location platform + sensor polling and the tailored-experiences
         // profiler that runs off collected diagnostic data.
         {kSearchPolicy, QStringLiteral("AllowSearchToUseLocation"), 0, 1},
         {kAdvertisingInfo, QStringLiteral("DisabledByGroupPolicy"), 1, 0},
         // Edge's own telemetry + first-run + personalisation-report uploads.
         {kEdgePolicy, QStringLiteral("MetricsReportingEnabled"), 0, 1},
         {kEdgePolicy, QStringLiteral("PersonalizationReportingEnabled"), 0, 1},
         {kEdgePolicy, QStringLiteral("UserFeedbackAllowed"), 0, 1}});

    // Folded into the groups above, so these no longer need rows of their own:
    //   Remove Startup Delay        -> Optimise Startup Programs
    //   Disable Power Throttling    -> Maximum Performance Profile
    //   Optimise GPU Scheduling     -> CPU & GPU Scheduling
    //   Enable Game Mode            -> Gaming Optimisation
    //   Disable Fullscreen Optims.  -> Gaming Optimisation
    //   Optimise Network Adapter    -> Network Latency Tuning

    return tweaks;
}
