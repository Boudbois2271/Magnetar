// SPDX-License-Identifier: GPL-3.0-or-later
// Magnetar: a desktop configurator for the Akko TAC75 HE keyboard.
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QUrl>

#include "backend.h"
#include "color_palette.h"
#include "desktop_size.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("magnetar");
    app.setApplicationDisplayName("Magnetar");
    app.setApplicationVersion("0.1.0");

    // Matches the window to magnetar.desktop, so the compositor (Wayland app_id) and the
    // launcher pick up the installed icon.
    app.setDesktopFileName("magnetar");
    app.setWindowIcon(QIcon::fromTheme("magnetar"));

    // Material, Dense variant (from qtquickcontrols2.conf): desktop-sized controls.
    QQuickStyle::setStyle("Material");

    // Installed before anything is translated; every qsTr() binding is re-evaluated when
    // the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    DesktopSize ui;
    Backend backend;
    // ~/.config/magnetar/settings.ini: app preferences, such as the saved colours.
    ColorPalette colorPalette(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/settings.ini");

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("ui", &ui);
    engine.rootContext()->setContextProperty("colorPalette", &colorPalette);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
