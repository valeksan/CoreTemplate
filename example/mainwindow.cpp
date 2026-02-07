#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QThread>


int test_function1(int a, int b, int c)
{
    qDebug() << "test_function1()";
    return a+b+c;
}

struct TestStruct
{
    int a;
    int b;
    int c;
};

TestStruct test_function2(int a, int b, int c)
{
    qDebug() << "test_function2()";
    return TestStruct{a, b, c};
}

struct TestStructDeclared
{
    int a;
    int b;
    int c;
};

Q_DECLARE_METATYPE(TestStructDeclared)

TestStructDeclared test_function3(int a, int b, int c)
{
    qDebug() << "test_function3()";
    return TestStructDeclared{a, b, c};
}

struct TestStructFunctor
{
    int c;
    int operator()(int a, int b)
    {
        qDebug() << "TestStructFunctor()";
        return (a+b)*c;
    }
};

void test_function4()
{
    qDebug() << "test_function4()";
}

int test_function5(int &a)
{
    qDebug() << "test_function5()";
    int b = a;
    return ++b;
}

int test_function6(const int &a)
{
    qDebug() << "test_function6()";
    int b = a;
    return ++b;
}

int test_function7(TestStruct s)
{
    qDebug() << "test_function7()";
    return s.a+s.b+s.c;
}

int test_function8(TestStructDeclared s)
{
    qDebug() << "test_function8()";
    return s.a+s.b+s.c;
}

int test_function9(int *p)
{
    qDebug() << "test_function9()";
    return *p;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow), m_pCore(new Core())
{
    m_a = 0;

    ui->setupUi(this);

    ui->menuBar->setHidden(true);
    ui->mainToolBar->setHidden(true);

    ui->listWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    ui->lineEditStopTaskId->setValidator(new QIntValidator());
    ui->lineEditStopTaskType->setValidator(new QIntValidator());
    ui->lineEditStopTaskGroup->setValidator(new QIntValidator());

    connect(ui->listWidget, &QListWidget::customContextMenuRequested, this, [=](const QPoint& pos)
    {
        QListWidgetItem *item = ui->listWidget->itemAt(pos);
        if(!item) return;

        QStringList tmp = item->text().split(' ');

        if(item)
        {
            QMenu *pMenu = new QMenu();
            pMenu->setAttribute(Qt::WA_DeleteOnClose);

            pMenu->addAction("StopTask", [=]()
            {
                m_pCore->stopTaskById(tmp[1].toInt());
            });

            pMenu->addAction("TerminateTask", [=]()
            {
                m_pCore->terminateTaskById(tmp[1].toInt());
            });

            pMenu->exec(ui->listWidget->viewport()->mapToGlobal(pos));
        }
    });

    //STOP TASK TESTS
    //TASK0 STOPS CORRECTLY
    m_pCore->registerTask(TASK0, [=]()
    {
        auto stopFlag = m_pCore->stopTaskFlag();
        forever
        {
            if(stopFlag->load()) return;
            qDebug() << "TASK0" << QThread::currentThread();
            QThread::msleep(1000);
        }
    }, 0);

    //TASK1 TERMINATES
    m_pCore->registerTask(TASK1, [=]()
    {
        forever
        {
            qDebug() << "TASK1" << QThread::currentThread();
            QThread::msleep(1000);
        }
    }, 1, 2000);

    //TASK2 STOPS CORRECTLY 2
    m_pCore->registerTask(TASK2, [=](int n)
    {
        int step = n;
        auto stopFlag = m_pCore->stopTaskFlag();
        while(step && !stopFlag->load())
        {
            qDebug() << "TASK2" << QThread::currentThread();
            QThread::msleep(1000);
            step--;
        }
    }, 2);

    //DIFFERENT TASK TESTS
    //TASK3 - CLASS METHOD
    m_pCore->registerTask(TASK3, &Test::doTest, &t);

    //TASK4 - CLASS METHOD CONST
    m_pCore->registerTask(TASK4, &Test::doTestConst, &t);

    //TASK5 - FUNCTION WITH CONVERTIBLE RETURN TYPE
    m_pCore->registerTask(TASK5, test_function1);

    //TASK6 - FUNCTION WITH NON CONVERTIBLE RETURN TYPE
    try
    {
        m_pCore->registerTask(TASK6, test_function2);
    }
    catch(const std::exception &e)
    {
        qWarning() << e.what();
    }

    //TASK6 - FUNCTION WITH CONVERTIBLE RETURN TYPE2
    m_pCore->registerTask(TASK6, test_function3);

    //TASK7 - FUNCTION WITH VOID RETURN
    m_pCore->registerTask(TASK7, test_function4);

    //TASK8 - FUNCTOR
    m_pCore->registerTask(TASK8, TestStructFunctor{10});

    //TASK9 - LAMBDA
    m_pCore->registerTask(TASK9, [=](int a)
    {
        qDebug() << "Lambda";
        int b = a;
        return ++b;
    });

    //TEST REGISTER ALREADY REGISTRED TASK
    try
    {
        m_pCore->registerTask(TASK9, test_function1);
    }
    catch(const std::exception &e)
    {
        qWarning() << e.what();
    }

    //TASK10 - FUNCTION WITH REFERENCE ARG
    m_pCore->registerTask(TASK10, test_function5);

    //TASK11 - FUNCTION WITH CONST REFERENCE ARG
    m_pCore->registerTask(TASK11, test_function6);

    //TASK12 - FUNCTION WITH NON CONVERTIBLE ARG
    m_pCore->registerTask(TASK12, test_function7);

    //TASK13 - FUNCTION WITH CONVERTIBLE ARG
    m_pCore->registerTask(TASK13, test_function8);

    //TASK14 - FUNCTION WITH POINTER ARG
    m_pCore->registerTask(TASK14, test_function9);

    connect(m_pCore, &Core::startedTask, this, [=](long id, int type, QVariantList argsList)
    {
        Q_UNUSED(argsList);

        QString t = QString("id %1 / type %2 / group %3").arg(QString::number(id), QString::number(type), QString::number(m_pCore->groupByTask(type)));

        ui->textEdit->append(QString("Task (%1) was started").arg(t));

        ui->listWidget->addItem(t);
    });

    connect(m_pCore, &Core::finishedTask, this, [=](long id, int type, QVariantList argsList, QVariant result)
    {
//        Q_UNUSED(result);
//        Q_UNUSED(argsList);

        switch(type)
        {
        case TASK0:
            break;
        case TASK1:
            break;
        case TASK2:
            break;
        case TASK3:
            qDebug() << "TASK3 - CLASS METHOD TEST";
            qDebug() << "Result:" << result.toInt();
            qDebug() << "Args:" << argsList;
            break;
        case TASK4:
            qDebug() << "TASK4 - CLASS METHOD CONST";
            qDebug() << "Result:" << result.toInt();
            qDebug() << "Args:" << argsList;
            break;
        case TASK5:
            qDebug() << "TASK5 - FUNCTION WITH CONVERTIBLE RETURN TYPE";
            qDebug() << "Result:" << result.toInt();
            qDebug() << "Args:" << argsList;
            break;
        case TASK6:
        {
            qDebug() << "TASK6 - FUNCTION WITH CONVERTIBLE RETURN TYPE2";
            auto res = result.value<TestStructDeclared>();
            qDebug() << QString("Result: TestStructDeclared { a=%1 b=%2 c=%3 }").arg(res.a).arg(res.b).arg(res.c);
            qDebug() << "Args:" << argsList;
            break;
        }
        case TASK7:
            qDebug() << "TASK7 - FUNCTION WITH VOID RETURN";
            qDebug() << "Args:" << argsList;
            break;
        case TASK8:
            qDebug() << "TASK8 - FUNCTOR";
            qDebug() << "Result:" << result.toInt();
            qDebug() << "Args:" << argsList;
            break;
        case TASK9:
            qDebug() << "TASK9 - LAMBDA";
            qDebug() << "Result:" << result.toInt();
            qDebug() << "Args:" << argsList;
            break;
        case TASK10:
            qDebug() << "TASK10 - FUNCTION WITH REFERENCE ARG";
            qDebug() << "Result:" << result.toInt();
            qDebug() << "Args:" << argsList;
            break;
        case TASK11:
            qDebug() << "TASK11 - FUNCTION WITH CONST REFERENCE ARG";
            qDebug() << "Result:" << result.toInt();
            qDebug() << "Args:" << argsList;
            break;
        case TASK12:
            qDebug() << "TASK12 - FUNCTION WITH NON CONVERTIBLE ARG";
            qDebug() << "Result:" << result.toInt();
            qDebug() << "Args:" << argsList;
            break;
        case TASK13:
        {
            auto arg = argsList.first().value<TestStructDeclared>();
            qDebug() << "TASK13 - FUNCTION WITH CONVERTIBLE ARG";
            qDebug() << "Result:" << result.toInt();
            qDebug() << QString("Args: TestStructDeclared { a=%1 b=%2 c=%3 }").arg(arg.a).arg(arg.b).arg(arg.c);;
            break;
        }
        case TASK14:
        {
            qDebug() << "TASK14 - FUNCTION WITH POINTER ARG";
            qDebug() << "Result:" << result.toInt();
            qDebug() << argsList;
            break;
        }
        default: break;
        }

        QString t = QString("id %1 / type %2 / group %3").arg(QString::number(id), QString::number(type), QString::number(m_pCore->groupByTask(type)));

        ui->textEdit->append(QString("Task (%1) was finished").arg(t));

        auto listToRemove = ui->listWidget->findItems(t, Qt::MatchExactly);

        foreach(auto elemToRemove, listToRemove) delete elemToRemove;
    });

    connect(m_pCore, &Core::terminatedTask, this, [=](long id, int type, QVariantList argsList)
    {
        Q_UNUSED(argsList);

        QString t = QString("id %1 / type %2 / group %3").arg(QString::number(id), QString::number(type), QString::number(m_pCore->groupByTask(type)));

        ui->textEdit->append(QString("Task (%1) was terminated").arg(t));

        auto listToRemove = ui->listWidget->findItems(t, Qt::MatchExactly);

        foreach(auto elemToRemove, listToRemove) delete elemToRemove;
    });

    connect(ui->pushButtonAddTask0, &QPushButton::clicked, this, [=]()
    {
        m_pCore->addTask(TASK0);
    });

    connect(ui->pushButtonAddTask1, &QPushButton::clicked, this, [=]()
    {
        m_pCore->addTask(TASK1);
    });

    connect(ui->pushButtonAddTask2, &QPushButton::clicked, this, [=]()
    {
        m_pCore->addTask(TASK2, 5);
    });

    connect(ui->pushButtonStopTaskById, &QPushButton::clicked, this, [=]()
    {
        m_pCore->stopTaskById(ui->lineEditStopTaskId->text().toInt());
    });

    connect(ui->pushButtonStopTaskByType, &QPushButton::clicked, this, [=]()
    {
        m_pCore->stopTaskByType(ui->lineEditStopTaskType->text().toInt());
    });

    connect(ui->pushButtonStopTaskByGroup, &QPushButton::clicked, this, [=]()
    {
        m_pCore->stopTaskByGroup(ui->lineEditStopTaskGroup->text().toInt());
    });

    connect(ui->pushButtonStopTasks, &QPushButton::clicked, this, [=]()
    {
        m_pCore->stopTasks();
    });

    //
    int a = 10;
    int b = 20;
    int c = 30;
    m_a = 999;

    //TEST ADD TASK WITHOUT CORRECT ARGS
    try
    {
        m_pCore->addTask(TASK3, a);
    }
    catch(const std::exception &e)
    {
        qWarning() << e.what();
    }

    m_pCore->addTask(TASK3, 11, 12);
    m_pCore->addTask(TASK3, a, b);
    m_pCore->addTask(TASK4);
    m_pCore->addTask(TASK5, 11, 12, 13);
    m_pCore->addTask(TASK5, a, b, c);
    m_pCore->addTask(TASK6, a, b, c);
    m_pCore->addTask(TASK7);
    m_pCore->addTask(TASK8, 11, 12);
    m_pCore->addTask(TASK8, a, b);
    m_pCore->addTask(TASK9, 11);
    m_pCore->addTask(TASK9, a);
    m_pCore->addTask(TASK10, 11);
    m_pCore->addTask(TASK10, a);
    m_pCore->addTask(TASK11, 11);
    m_pCore->addTask(TASK11, a);
    m_pCore->addTask(TASK12, TestStruct{11,12,13});
    m_pCore->addTask(TASK13, TestStructDeclared{11,12,13});
    m_pCore->addTask(TASK14, &m_a);
}

MainWindow::~MainWindow()
{
    delete ui;
}
