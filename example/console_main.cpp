#include "../core.h"

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    Core core;

    core.registerTask(1, [](int value) -> int {
        return value * 2;
    });

    QObject::connect(&core, &Core::finishedTask, &app, [&app](TaskId id, TaskType type, const QVariantList& argsList, const QVariant& result) {
        Q_UNUSED(id);
        Q_UNUSED(type);
        Q_UNUSED(argsList);
        qDebug() << "Console example result:" << result.toInt();
        app.quit();
    });

    core.addTask(1, 21);

    return app.exec();
}
