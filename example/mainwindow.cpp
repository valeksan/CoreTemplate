#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QThread>
#include <QMenu>
#include <QListWidgetItem>
#include <QTimer>
#include <QToolButton>
#include <QSplitter>
#include <QVector>
#include <QSet>

// --- Examples of functions/structures to demonstrate the capabilities of CoreTaskManager ---

// A simple function with a return value
int calculateSum(int a, int b, int c) {
    qDebug() << "calculateSum() executed with args:" << a << b << c;
    return a + b + c;
}

// The structure we want to use in the tasks
struct MyData {
    int value1;
    int value2;
    QString text;
};

Q_DECLARE_METATYPE(MyData)

// A function that returns a custom type (must be Q_DECLARE_METATYPE)
MyData createMyData(int val1, int val2, const QString& txt) {
    qDebug() << "createMyData() executed with args:" << val1 << val2 << txt;
    return MyData{val1, val2, txt};
}

// Function with no return value
void performAction() {
    qDebug() << "performAction() executed.";
}

// A functor (an object with the operator())
struct MultiplyFunctor {
    int factor;
    int operator()(int x, int y) {
        qDebug() << "MultiplyFunctor::operator() executed with args:" << x << y << "and factor:" << factor;
        return (x + y) * factor;
    }
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_pCore(new Core(this))
{
    ui->setupUi(this);

    constexpr int RoleTaskId = Qt::UserRole + 1;
    constexpr int RoleTaskType = Qt::UserRole + 2;
    constexpr int RoleTaskGroup = Qt::UserRole + 3;

    enum class LogKind {
        Started,
        Finished,
        StopRequested,
        StopTimedOut,
        Terminated,
        Info
    };

    struct LogEntry {
        LogKind kind;
        QString text;
    };

    auto logColor = [](LogKind kind) -> QString {
        switch (kind) {
        case LogKind::Started: return "#2e7d32";
        case LogKind::Finished: return "#1565c0";
        case LogKind::StopRequested: return "#ef6c00";
        case LogKind::StopTimedOut: return "#c62828";
        case LogKind::Terminated: return "#6a1b9a";
        case LogKind::Info: return "#37474f";
        }
        return "#37474f";
    };

    auto logTag = [](LogKind kind) -> QString {
        switch (kind) {
        case LogKind::Started: return "STARTED";
        case LogKind::Finished: return "FINISHED";
        case LogKind::StopRequested: return "STOP REQUESTED";
        case LogKind::StopTimedOut: return "STOP TIMED OUT";
        case LogKind::Terminated: return "TERMINATED";
        case LogKind::Info: return "INFO";
        }
        return "INFO";
    };

    auto* pLogEntries = new QVector<LogEntry>();
    auto* pVisibleKinds = new QSet<LogKind>({
        LogKind::Started,
        LogKind::Finished,
        LogKind::StopRequested,
        LogKind::StopTimedOut,
        LogKind::Terminated,
        LogKind::Info
    });

    auto rebuildLog = [this, pLogEntries, pVisibleKinds, logColor, logTag]() {
        ui->textEdit->clear();
        for (const auto& entry : std::as_const(*pLogEntries)) {
            if (!pVisibleKinds->contains(entry.kind)) {
                continue;
            }
            const QString html = QString("<span style=\"color:%1;\"><b>[%2]</b> %3</span>")
                                     .arg(logColor(entry.kind),
                                          logTag(entry.kind),
                                          entry.text.toHtmlEscaped());
            ui->textEdit->append(html);
        }
    };

    auto addLog = [this, pLogEntries, pVisibleKinds, logColor, logTag](LogKind kind, const QString& text) {
        pLogEntries->append(LogEntry{kind, text});
        if (!pVisibleKinds->contains(kind)) {
            return;
        }
        const QString html = QString("<span style=\"color:%1;\"><b>[%2]</b> %3</span>")
                                 .arg(logColor(kind),
                                      logTag(kind),
                                      text.toHtmlEscaped());
        ui->textEdit->append(html);
    };

    auto* pLogFilterButton = findChild<QToolButton*>("toolButtonLogFilter");
    if (!pLogFilterButton) {
        pLogFilterButton = new QToolButton(ui->centralWidget);
        pLogFilterButton->setText("Log Filter...");
    }
    pLogFilterButton->setPopupMode(QToolButton::InstantPopup);

    auto* pLogMenu = new QMenu(pLogFilterButton);
    auto makeFilterAction = [pLogMenu, pVisibleKinds, rebuildLog](const QString& title, LogKind kind) {
        QAction* pAction = pLogMenu->addAction(title);
        pAction->setCheckable(true);
        pAction->setChecked(true);
        QObject::connect(pAction, &QAction::toggled, pLogMenu, [pVisibleKinds, rebuildLog, kind](bool checked) {
            if (checked) {
                pVisibleKinds->insert(kind);
            } else {
                pVisibleKinds->remove(kind);
            }
            rebuildLog();
        });
    };

    makeFilterAction("Started", LogKind::Started);
    makeFilterAction("Finished", LogKind::Finished);
    makeFilterAction("Stop Requested", LogKind::StopRequested);
    makeFilterAction("Stop Timed Out", LogKind::StopTimedOut);
    makeFilterAction("Terminated", LogKind::Terminated);
    makeFilterAction("Info", LogKind::Info);
    pLogMenu->addSeparator();
    pLogMenu->addAction("Clear Log", [this, pLogEntries]() {
        pLogEntries->clear();
        ui->textEdit->clear();
    });
    pLogFilterButton->setMenu(pLogMenu);

    if (auto* pMainSplitter = findChild<QSplitter*>("splitterMainPanels")) {
        pMainSplitter->setStretchFactor(0, 2);
        pMainSplitter->setStretchFactor(1, 3);
        pMainSplitter->setSizes({240, 320});
    }

    auto removeTaskItemById = [this, RoleTaskId](TaskId taskId) {
        for (int i = 0; i < ui->listWidget->count(); ++i) {
            QListWidgetItem* item = ui->listWidget->item(i);
            if (!item) {
                continue;
            }
            if (item->data(RoleTaskId).toLongLong() == taskId) {
                delete ui->listWidget->takeItem(i);
                return;
            }
        }
    };

    // Hiding the menu and toolbar for simplicity of the example
    ui->menuBar->setHidden(true);
    ui->mainToolBar->setHidden(true);

    // Setting the context menu for ListWidget (to stop/interrupt tasks)
    ui->listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listWidget, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QListWidgetItem *item = ui->listWidget->itemAt(pos);
        if(!item) return;

        const auto taskId = static_cast<TaskId>(item->data(RoleTaskId).toLongLong());
        const auto taskType = static_cast<TaskType>(item->data(RoleTaskType).toInt());
        const auto taskGroup = static_cast<TaskGroup>(item->data(RoleTaskGroup).toInt());

        QMenu *contextMenu = new QMenu(this); // The parent of this is for automatic cleaning
        contextMenu->setAttribute(Qt::WA_DeleteOnClose);

        contextMenu->addAction("Cancel This Task", [this, taskId]() {
            m_pCore->cancelTaskById(taskId);
        });
        contextMenu->addAction("Terminate This Task", [this, taskId]() {
            m_pCore->terminateTaskById(taskId);
        });
        contextMenu->addSeparator();
        contextMenu->addAction("Cancel By Type", [this, taskType]() {
            m_pCore->cancelTaskByType(taskType);
        });
        contextMenu->addAction("Cancel Group (Active)", [this, taskGroup]() {
            m_pCore->cancelTasksByGroup(taskGroup, false);
        });
        contextMenu->addAction("Cancel Group (All)", [this, taskGroup]() {
            m_pCore->cancelTasksByGroup(taskGroup, true);
        });

        contextMenu->exec(ui->listWidget->viewport()->mapToGlobal(pos));
    });

    // --- Configuring validators for input fields ---
    ui->lineEditStopTaskId->setValidator(new QIntValidator(0, INT_MAX, this));
    ui->lineEditStopTaskType->setValidator(new QIntValidator(0, INT_MAX, this));
    ui->lineEditStopTaskGroup->setValidator(new QIntValidator(0, INT_MAX, this));

    // --- Registration of tasks ---
    // 1. A task that can be stopped
    // We use a lambda that checks the stop flag
    m_pCore->registerTask(TASK_STOPPABLE, [this]() {
        auto stopFlag = m_pCore->stopTaskFlag(); // Getting the stop flag for the current stream
        if (!stopFlag) {
            qDebug() << "TASK_STOPPABLE - No stop flag, exiting.";
            return;
        }
        int counter = 0;
        while (counter < 10 && !stopFlag->load()) { // // Checking the flag
            qDebug() << "TASK_STOPPABLE - Iteration:" << counter << "on thread:" << QThread::currentThread();
            QThread::msleep(1000); // sleep 1 second
            counter++;
        }
        if (stopFlag->load()) {
            qDebug() << "TASK_STOPPABLE - Stopped gracefully.";
        } else {
            qDebug() << "TASK_STOPPABLE - Finished normally.";
        }
    }, 0); // Group 0.

    // 2. A task that does not check the stop flag (will be aborted)
    m_pCore->registerTask(TASK_TERMINATED, []() {
        qDebug() << "TASK_TERMINATED - Starting long-running operation...";
        // This task does NOT check the stop flag!
        for (int i = 0; i < 100; ++i) {
            qDebug() << "TASK_TERMINATED - Working... iteration" << i;
            QThread::msleep(500); // Sleep 0.5 s
        }
        qDebug() << "TASK_TERMINATED - Would finish after 50 seconds, but likely terminated earlier.";
    }, 1, 2000); // Group 1. The stop timeout is 2 seconds

    // 3. A task with arguments that can be stopped
    m_pCore->registerTask(TASK_STOPPABLE_WITH_ARG, [this](int durationSeconds) {
        int remaining = durationSeconds;
        auto stopFlag = m_pCore->stopTaskFlag();
        if (!stopFlag) {
            qDebug() << "TASK_STOPPABLE_WITH_ARG - No stop flag, exiting.";
            return;
        }
        while (remaining > 0 && !stopFlag->load()) {
            qDebug() << "TASK_STOPPABLE_WITH_ARG - Remaining time:" << remaining << "seconds on thread:" << QThread::currentThread();
            QThread::msleep(1000);
            remaining--;
        }
        if (stopFlag->load()) {
            qDebug() << "TASK_STOPPABLE_WITH_ARG - Stopped gracefully.";
        } else {
            qDebug() << "TASK_STOPPABLE_WITH_ARG - Finished normally after" << durationSeconds << "seconds.";
        }
    }, 2); // Group 2.

    // 4. A task that calls a class method (not const)
    m_pCore->registerTask(TASK_CLASS_METHOD, &Calculator::add, &m_calculator);

    // 5. The task that calls the const method of the class
    m_pCore->registerTask(TASK_CLASS_CONST_METHOD, &Calculator::multiply, &m_calculator);

    // 6. A task that calls a free function with a return value
    m_pCore->registerTask(TASK_FREE_FUNCTION_RETURN, calculateSum);

    // 7. A task that calls a free function with a custom type (QMetaType)
    m_pCore->registerTask(TASK_CUSTOM_TYPE_RETURN, createMyData);

    // 8. A task that calls a free function without a return value
    m_pCore->registerTask(TASK_VOID_FUNCTION, performAction);

    // 9. The task that calls the functor
    m_pCore->registerTask(TASK_FUNCTOR, MultiplyFunctor{5}); // factor = 5

    // 10. The task that calls the lambda
    m_pCore->registerTask(TASK_LAMBDA, [](int x) -> int {
        qDebug() << "TASK_LAMBDA executed with arg:" << x;
        return x * 10;
    });

    // --- Signal processing from Core ---

    // Task start signal
    connect(m_pCore, &Core::startedTask, this, [this, RoleTaskId, RoleTaskType, RoleTaskGroup, addLog](TaskId id, TaskType type, const QVariantList& argsList) {
        Q_UNUSED(argsList);
        const TaskGroup group = m_pCore->groupByTask(type);
        QString info = QString("ID: %1, Type: %2, Group: %3")
                           .arg(id)
                           .arg(type)
                           .arg(group);
        addLog(LogKind::Started, QString("Task (%1) started.").arg(info));
        auto* item = new QListWidgetItem(info);
        item->setData(RoleTaskId, static_cast<qlonglong>(id));
        item->setData(RoleTaskType, type);
        item->setData(RoleTaskGroup, group);
        ui->listWidget->addItem(item);
    });

    // Task completion signal
    connect(m_pCore, &Core::finishedTask, this, [this, removeTaskItemById, addLog](TaskId id, TaskType type, const QVariantList& argsList, const QVariant& result) {
        Q_UNUSED(argsList);
        QString info = QString("ID: %1, Type: %2").arg(id).arg(type);
        addLog(LogKind::Finished, QString("Task (%1) finished.").arg(info));

        // Result processing depending on the type of task
        switch (type) {
        case TASK_CLASS_METHOD:
            qDebug() << "Result from TASK_CLASS_METHOD (Calculator::add):" << result.toInt();
            break;
        case TASK_CLASS_CONST_METHOD:
            qDebug() << "Result from TASK_CLASS_CONST_METHOD (Calculator::multiply):" << result.toInt();
            break;
        case TASK_FREE_FUNCTION_RETURN:
            qDebug() << "Result from TASK_FREE_FUNCTION_RETURN (calculateSum):" << result.toInt();
            break;
        case TASK_CUSTOM_TYPE_RETURN: {
            auto data = result.value<MyData>(); // Extracting a custom type
            qDebug() << "Result from TASK_CUSTOM_TYPE_RETURN (createMyData):" << data.value1 << data.value2 << data.text;
            break;
        }
        case TASK_VOID_FUNCTION:
            qDebug() << "TASK_VOID_FUNCTION finished (no result).";
            break;
        case TASK_FUNCTOR:
            qDebug() << "Result from TASK_FUNCTOR (MultiplyFunctor):" << result.toInt();
            break;
        case TASK_LAMBDA:
            qDebug() << "Result from TASK_LAMBDA:" << result.toInt();
            break;
        default:
            break;
        }

        removeTaskItemById(id);
    });

    // A signal about the forced completion of a task
    connect(m_pCore, &Core::terminatedTask, this, [this, removeTaskItemById, addLog](TaskId id, TaskType type, const QVariantList& argsList) {
        Q_UNUSED(argsList);
        QString info = QString("ID: %1, Type: %2").arg(id).arg(type);
        addLog(LogKind::Terminated, QString("Task (%1) was TERMINATED.").arg(info));
        removeTaskItemById(id);
    });

    // A signal that cooperative stop was requested for the task
    connect(m_pCore, &Core::stopRequestedTask, this, [this, addLog](TaskId id, TaskType type, const QVariantList& argsList) {
        Q_UNUSED(argsList);
        QString info = QString("ID: %1, Type: %2").arg(id).arg(type);
        addLog(LogKind::StopRequested, QString("Task (%1) STOP REQUESTED.").arg(info));
    });

    // A signal that task did not stop within timeout
    connect(m_pCore, &Core::stopTimedOutTask, this, [this, addLog](TaskId id, TaskType type, const QVariantList& argsList, TaskStopTimeout timeout) {
        Q_UNUSED(argsList);
        QString info = QString("ID: %1, Type: %2").arg(id).arg(type);
        addLog(LogKind::StopTimedOut, QString("Task (%1) STOP TIMED OUT after %2 ms.").arg(info).arg(timeout));
    });

    // --- Connecting UI buttons ---

    connect(ui->pushButtonAddTask0, &QPushButton::clicked, this, [this]() {
        m_pCore->addTask(TASK_STOPPABLE);
    });

    connect(ui->pushButtonAddTask1, &QPushButton::clicked, this, [this]() {
        m_pCore->addTask(TASK_TERMINATED);
    });

    connect(ui->pushButtonAddTask2, &QPushButton::clicked, this, [this]() {
        m_pCore->addTask(TASK_STOPPABLE_WITH_ARG, 5); // Run for 5 seconds
    });

    connect(ui->pushButtonStopTaskById, &QPushButton::clicked, this, [this]() {
        bool ok;
        long id = ui->lineEditStopTaskId->text().toLong(&ok);
        if (ok) {
            m_pCore->cancelTaskById(id);
        } else {
            qDebug() << "Invalid Task ID entered.";
        }
    });

    connect(ui->pushButtonStopTaskByType, &QPushButton::clicked, this, [this]() {
        bool ok;
        int type = ui->lineEditStopTaskType->text().toInt(&ok);
        if (ok) {
            m_pCore->cancelTaskByType(type);
        } else {
            qDebug() << "Invalid Task Type entered.";
        }
    });

    connect(ui->pushButtonStopTaskByGroup, &QPushButton::clicked, this, [this]() {
        bool ok;
        int group = ui->lineEditStopTaskGroup->text().toInt(&ok);
        if (ok) {
            m_pCore->cancelTasksByGroup(group, true); // Cancel by group including queued tasks
        } else {
            qDebug() << "Invalid Task Group entered.";
        }
    });

    connect(ui->pushButtonStopTasks, &QPushButton::clicked, this, [this]() {
        QMenu menu(this);
        QAction* pStopActive = menu.addAction("Cancel Active Tasks");
        QAction* pStopAll = menu.addAction("Cancel ALL Tasks (Active + Queued)");
        menu.addSeparator();
        QAction* pStopGroupActive = menu.addAction("Cancel Active Tasks By Group");
        QAction* pStopGroupAll = menu.addAction("Cancel ALL Tasks By Group (Active + Queued)");

        QAction* pSelected = menu.exec(ui->pushButtonStopTasks->mapToGlobal(QPoint(0, ui->pushButtonStopTasks->height())));
        if (!pSelected) {
            return;
        }

        if (pSelected == pStopActive) {
            m_pCore->cancelTasks();
            return;
        }

        if (pSelected == pStopAll) {
            m_pCore->cancelAllTasks();
            return;
        }

        bool ok = false;
        int group = ui->lineEditStopTaskGroup->text().toInt(&ok);
        if (!ok) {
            qDebug() << "Invalid Task Group entered.";
            return;
        }

        if (pSelected == pStopGroupActive) {
            m_pCore->cancelTasksByGroup(group, false);
        } else if (pSelected == pStopGroupAll) {
            m_pCore->cancelTasksByGroup(group, true);
        }
    });

    // --- Adding multiple tasks for demonstration ---

    qDebug() << "\n--- Adding initial tasks for demonstration ---";

    // Tasks for demonstrating different types
    m_pCore->addTask(TASK_CLASS_METHOD, 10, 20);            // Will call calc.add(10, 20)
    m_pCore->addTask(TASK_CLASS_CONST_METHOD, 10, 20);      // Will call calc.multiply(10, 20)
    m_pCore->addTask(TASK_FREE_FUNCTION_RETURN, 1, 2, 3);   // Will call calculateSum(1, 2, 3)
    m_pCore->addTask(
        TASK_CUSTOM_TYPE_RETURN,
        100, 200, QString("Hello")
    );                                                      // Will call createMyData(...)
    m_pCore->addTask(TASK_VOID_FUNCTION);                   // Will call performAction()
    m_pCore->addTask(TASK_FUNCTOR, 7, 8);                   // Will call MultiplyFunctor{5}(7, 8)
    m_pCore->addTask(TASK_LAMBDA, 42);                      // Will call a lambda with argument 42
}

MainWindow::~MainWindow()
{
    delete ui;
    // m_pCore will be automatically deleted because it has a parent (this) installed
}
