#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QThread>
#include <QMenu>
#include <QListWidgetItem>
#include <QTimer>

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
    , m_pCore(new Core()) // Passing the parent for automatic cleaning
{
    ui->setupUi(this);

    // Hiding the menu and toolbar for simplicity of the example
    ui->menuBar->setHidden(true);
    ui->mainToolBar->setHidden(true);

    // Setting the context menu for ListWidget (to stop/interrupt tasks)
    ui->listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listWidget, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QListWidgetItem *item = ui->listWidget->itemAt(pos);
        if(!item) return;

        QStringList parts = item->text().split(' ');
        if (parts.size() < 2) return; // Checking that the string contains the ID!

        bool ok;
        long taskId = parts[1].toLong(&ok);
        if (!ok) return; // Checking that the ID is the correct number!

        if(item)
        {
            QMenu *contextMenu = new QMenu(this); // The parent of this is for automatic cleaning
            contextMenu->setAttribute(Qt::WA_DeleteOnClose);

            // Action to request a task stop
            contextMenu->addAction("Stop Task", [this, taskId]() {
                m_pCore->stopTaskById(taskId);
            });

            // Action to force completion of a task (terminate)
            contextMenu->addAction("Terminate Task", [this, taskId]() {
                m_pCore->terminateTaskById(taskId);
            });

            contextMenu->exec(ui->listWidget->viewport()->mapToGlobal(pos));
        }
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
    Calculator calc;
    m_pCore->registerTask(TASK_CLASS_METHOD, &Calculator::add, &calc);

    // 5. The task that calls the const method of the class
    m_pCore->registerTask(TASK_CLASS_CONST_METHOD, &Calculator::multiply, &calc);

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
    connect(m_pCore, &Core::startedTask, this, [this](TaskId id, TaskType type, const QVariantList& argsList) {
        Q_UNUSED(argsList);
        QString info = QString("ID: %1, Type: %2, Group: %3")
                           .arg(id)
                           .arg(type)
                           .arg(m_pCore->groupByTask(type));
        ui->textEdit->append(QString("Task (%1) started.").arg(info));
        ui->listWidget->addItem(info);
    });

    // Task completion signal
    connect(m_pCore, &Core::finishedTask, this, [this](TaskId id, TaskType type, const QVariantList& argsList, const QVariant& result) {
        QString info = QString("ID: %1, Type: %2").arg(id).arg(type);
        ui->textEdit->append(QString("Task (%1) finished.").arg(info));

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

        // Removing an item from the task list
        auto itemsToRemove = ui->listWidget->findItems(info, Qt::MatchContains);
        for (auto* item : std::as_const(itemsToRemove)) { // clazy: range-loop-detach
            delete item;
        }
    });

    // A signal about the forced completion of a task
    connect(m_pCore, &Core::terminatedTask, this, [this](TaskId id, TaskType type, const QVariantList& argsList) {
        Q_UNUSED(argsList);
        QString info = QString("ID: %1, Type: %2").arg(id).arg(type);
        ui->textEdit->append(QString("Task (%1) was TERMINATED.").arg(info));

        // Удаляем элемент из списка задач
        auto itemsToRemove = ui->listWidget->findItems(info, Qt::MatchContains);
        for (auto* item : std::as_const(itemsToRemove)) { // clazy: range-loop-detach
            delete item;
        }
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
            m_pCore->stopTaskById(id);
        } else {
            qDebug() << "Invalid Task ID entered.";
        }
    });

    connect(ui->pushButtonStopTaskByType, &QPushButton::clicked, this, [this]() {
        bool ok;
        int type = ui->lineEditStopTaskType->text().toInt(&ok);
        if (ok) {
            m_pCore->stopTaskByType(type);
        } else {
            qDebug() << "Invalid Task Type entered.";
        }
    });

    connect(ui->pushButtonStopTaskByGroup, &QPushButton::clicked, this, [this]() {
        bool ok;
        int group = ui->lineEditStopTaskGroup->text().toInt(&ok);
        if (ok) {
            m_pCore->stopTaskByGroup(group);
        } else {
            qDebug() << "Invalid Task Group entered.";
        }
    });

    connect(ui->pushButtonStopTasks, &QPushButton::clicked, this, [this]() {
        m_pCore->stopTasks(); // Stop all active tasks
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
