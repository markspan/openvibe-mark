#include <QGuiApplication>
#include <QStyleHints>
#include <QQmlApplicationEngine>
#include <QtQml/qqmlextensionplugin.h>
#include <QLoggingCategory>


int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);


    QGuiApplication::styleHints()->setUseHoverEffects(true);
    QLoggingCategory::setFilterRules(QStringLiteral("qt.qml.binding.removal.info=true"));

    QQmlApplicationEngine engine;
    engine.addImportPath("qrc:/qt/qml/");
    engine.load(QUrl("qrc:/qt/qml/openvibe-acquisition-server/main.qml"));

    return app.exec();
}
