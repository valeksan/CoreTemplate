#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QApplication>
#include <QMainWindow>
#include "core.h"

namespace Ui {
class MainWindow;
}

enum Tasks
{
    TASK0,
    TASK1,
    TASK2,
    TASK3,
    TASK4,
    TASK5,
    TASK6,
    TASK7,
    TASK8,
    TASK9,
    TASK10,
    TASK11,
    TASK12,
    TASK13,
    TASK14
};

class Test
{
public:
    Test()
    {
        ;
    }

    int doTest(int a, int b)
    {
        return a+b;
    }

    int doTestConst() const
    {
        return 9999;
    }
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:


private:
    Ui::MainWindow *ui;

    Core *m_pCore;

    Test t;

    int m_a;

public slots:
};

#endif // MAINWINDOW_H
