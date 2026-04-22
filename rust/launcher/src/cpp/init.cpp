// SPDX-License-Identifier: LicenseRef-PolyForm-Noncommercial-1.0.0
// SPDX-FileCopyrightText: 2026 Callan Barrett
//
// C++ glue for the Rust launcher binary (Phase 1). Contains everything that
// currently lives in src/app/main.cpp and MiSterRuntime.cpp, exposed as an
// extern "C" function so Rust can call it as the application entry point.
//
// Phase 3 replaces this file piece by piece:
//   - Models → Rust QObject bridges via cxx-qt
//   - ZaparooClient → tokio-tungstenite
//   - Config → serde/toml
//   - Logger → tracing
//   - MiSterRuntime → std::process::Command + std::env::set_var
//   - Qt app setup → cxx-qt-lib (QGuiApplication, QQmlApplicationEngine)

#include "BrowseModel.h"
#include "CategoriesModel.h"
#include "Config.h"
#include "GamesModel.h"
#include "Logger.h"
#include "SystemsCatalog.h"
#include "SystemsModel.h"
#include "ZaparooClient.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml/qqmlextensionplugin.h>

// Zaparoo QML plugins are always built as static .a libs (qt_add_library STATIC).
// These imports register each plugin with Qt before the engine starts.
Q_IMPORT_QML_PLUGIN(Zaparoo_AppPlugin)
Q_IMPORT_QML_PLUGIN(Zaparoo_UiPlugin)
Q_IMPORT_QML_PLUGIN(Zaparoo_ThemePlugin)
Q_IMPORT_QML_PLUGIN(Zaparoo_BrowsePlugin)

// Static Qt (ARM32 MiSTer build): also import Qt-internal QML and platform
// plugins that the dynamic plugin loader would otherwise find at runtime.
#ifdef QT_STATIC
#include <QtPlugin>
Q_IMPORT_QML_PLUGIN(QtQuickControls2Plugin)
Q_IMPORT_QML_PLUGIN(QtQuickControls2BasicStylePlugin)
Q_IMPORT_QML_PLUGIN(QtQuickControls2ImplPlugin)
Q_IMPORT_QML_PLUGIN(QtQuickTemplates2Plugin)
Q_IMPORT_QML_PLUGIN(QtQuick_WindowPlugin)
Q_IMPORT_PLUGIN(QLinuxFbIntegrationPlugin)
#endif

namespace
{

// Pre-Qt MiSTer setup: set linuxfb env vars and call vmode. Mirrors
// MiSterRuntime::applyPreQtSetup. Phase 3 moves this to Rust.
#ifdef ZAPAROO_MISTER
void applyMiSterPreQtSetup(const zaparoo::Config& config)
{
    qputenv("QT_QPA_PLATFORM", "linuxfb");
    qputenv("QT_QUICK_BACKEND", "software");

    const QStringList vmodeArgs{"-r", QString::number(config.videoWidth),
                                QString::number(config.videoHeight), "rgb32"};
    const int result = QProcess::execute("vmode", vmodeArgs);
    if (result == -2)
        qCWarning(zapApp) << "vmode not found — display mode unchanged";
    else if (result != 0)
        qCWarning(zapApp) << "vmode exited with" << result;
}

void ensureCoreServiceRunning()
{
    if (!QProcess::startDetached("/media/fat/Scripts/zaparoo.sh", {"-service", "start"}))
        qCWarning(zapApp) << "failed to start zaparoo.sh";
}
#endif

} // namespace

extern "C" {

// Entry point called from Rust main(). Mirrors src/app/main.cpp exactly.
// argc/argv are passed through from Rust's std::env::args() collection.
int zaparoo_run_launcher(int argc, char** argv)
{
    QGuiApplication::setApplicationName("Zaparoo Launcher");
    QGuiApplication::setApplicationVersion("0.1.0");
    QGuiApplication::setOrganizationName("Zaparoo");
    QGuiApplication::setOrganizationDomain("zaparoo.org");

    zaparoo::Logger::install();
    const zaparoo::Config config = zaparoo::loadConfig();
    zaparoo::Logger::applyConfig(config);

#ifdef ZAPAROO_MISTER
    applyMiSterPreQtSetup(config);
    ensureCoreServiceRunning();
#endif

    QGuiApplication app(argc, argv);

    zaparoo::ZaparooClient client;

    zaparoo::BrowseModel browseModel(&client);
    zaparoo::BrowseModel::setInstance(&browseModel);

    zaparoo::SystemsCatalog catalog(&client);

    zaparoo::CategoriesModel categoriesModel(&catalog);
    zaparoo::CategoriesModel::setInstance(&categoriesModel);

    zaparoo::SystemsModel systemsModel(&catalog);
    zaparoo::SystemsModel::setInstance(&systemsModel);

    zaparoo::GamesModel gamesModel(&client);
    zaparoo::GamesModel::setInstance(&gamesModel);

    QFontDatabase::addApplicationFont(
        ":/qt/qml/Zaparoo/App/resources/fonts/PressStart2P.ttf");

    QQuickStyle::setStyle("Basic");

    QQmlApplicationEngine engine;
#ifndef ZAPAROO_DEV_BUILD
    engine.setInitialProperties({{"fullScreen", true}});
#endif
    engine.loadFromModule("Zaparoo.App", "Main");

    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE;

    client.connectToCore(config.coreEndpoint);

    return QGuiApplication::exec();
}

} // extern "C"
