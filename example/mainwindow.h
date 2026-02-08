#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QApplication>
#include <QMainWindow>
#include "core.h"

namespace Ui {
class MainWindow;
}

// --- Declaring named constants for task types ---
// This makes the code more readable and understandable compared to numeric values TASK0, TASK1, etc.
enum Tasks
{
    TASK_STOPPABLE = 0,          // A task that can be stopped
    TASK_TERMINATED,             // Task to be aborted (does not check the flag)
    TASK_STOPPABLE_WITH_ARG,     // A task with an argument that can be stopped
    TASK_CLASS_METHOD,           // Calling a class method (not const)
    TASK_CLASS_CONST_METHOD,     // Calling the const method of the class
    TASK_FREE_FUNCTION_RETURN,   // Calling a free function with a return value
    TASK_CUSTOM_TYPE_RETURN,     // Calling a function that returns a custom type (QMetaType)
    TASK_VOID_FUNCTION,          // Calling a function without a return value
    TASK_FUNCTOR,                // Calling the functor
    TASK_LAMBDA                  // Calling lambda
};
// enum Tasks
// {
//     TASK0,
//     TASK1,
//     TASK2,
//     TASK3,
//     TASK4,
//     TASK5,
//     TASK6,
//     TASK7,
//     TASK8,
//     TASK9,
//     TASK10,
//     TASK11,
//     TASK12,
//     TASK13,
//     TASK14
// };

// --- Example of a class for demonstrating method invocation ---
// Implementation of methods inside the class so that there is no conflict when included in .cpp
class Calculator
{
public:
    int add(int a, int b) {
        qDebug() << "Calculator::add() executed with args:" << a << b;
        return a + b;
    }

    int multiply(int a, int b) const {
        qDebug() << "Calculator::multiply() executed with args:" << a << b;
        return a * b;
    }
};

// class Test
// {
// public:
//     Test()
//     {
//         ;
//     }

//     int doTest(int a, int b)
//     {
//         return a+b;
//     }

//     int doTestConst() const
//     {
//         return 9999;
//     }
// };

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    // A pointer to our task manager
    Core *m_pCore;

    // An object of the class for demonstrating method invocation
    Calculator m_calculator;
    // Test t;

    // A variable to demonstrate the transfer of a pointer
    int m_demoVariable = 0;
    // int m_a;

public slots:
};

#endif // MAINWINDOW_H
