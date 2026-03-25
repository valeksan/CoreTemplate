#include <QCoreApplication>
#include <QDebug>

#include "core.h"

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    Core core;
    core.registerTask(1, [](int value) -> int {
        return value + 1;
    });

    QObject::connect(&core, &Core::finishedTask, &app, [&app](TaskId, TaskType, const QVariantList&, const QVariant& result) {
        qDebug() << "Package smoke result:" << result.toInt();
        app.quit();
    });

    core.addTask(1, 41);
    return app.exec();
}
