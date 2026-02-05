# CoreTaskManager

A Qt-based library designed to manage and execute tasks asynchronously in separate threads. It provides features for task registration, queuing, grouping (to limit concurrency within groups), and controlling running tasks (stopping, terminating).

## Features

*   **Asynchronous Execution:** Tasks run in dedicated background threads managed by the library.
*   **Task Registration:** Register various callable objects (free functions, member functions, lambdas, functors) as tasks with a unique integer type identifier.
*   **Flexible Arguments:** Supports tasks with various argument types, provided they are compatible with `QVariant`.
*   **Return Values:** Supports tasks returning values (convertible to `QVariant`) or `void`. Results are emitted via signals upon completion.
*   **Task Groups:** Assign tasks to groups. Only one task per group runs concurrently, allowing for resource management or serialization of related operations.
*   **Task Control:** Start tasks, request graceful stops (using internal flags), force termination (via platform-specific calls like `TerminateThread`/`pthread_cancel`), and stop all tasks.
*   **Signals:** Emits signals when tasks start, finish successfully, or are terminated, providing details like task ID, type, arguments, and results.
*   **Qt Integration:** Inherits from `QObject` and leverages Qt's signal/slot mechanism and threading primitives.

## How It Works

1.  An instance of the `Core` class is created.
2.  Callables are registered with `Core::registerTask(...)`, assigning them a unique `taskType` integer and optional group and timeout settings.
3.  Tasks are queued for execution using `Core::addTask(taskType, ...args)`.
4.  The `Core` manages a queue and ensures only one task per group runs at a time.
5.  When a slot opens up (either due to a previous task finishing or because the task belongs to a different group), the `Core` starts the next eligible task in its own thread using `CreateThread` (Windows) or `pthread_create` (Unix-like systems).
6.  The task's associated function executes within the new thread.
7.  While executing, a task can check a shared stop flag retrieved via `Core::stopTaskFlag()` to perform graceful shutdowns.
8.  Upon completion (normal, stopped, or terminated), the task emits a signal (`finishedTask`, `terminatedTask`) back to the main thread where the `Core` lives.
9.  The `Core` updates its internal lists of active and queued tasks and proceeds to start the next queued task if applicable.

## Example Usage

See the `MainWindow` constructor in the provided `mainwindow.cpp` for a comprehensive example demonstrating how to register different types of tasks (functions, member functions, lambdas, functors) and add them for execution.

## Important Notes

*   **Platform Specifics:** The library uses `CreateThread`/`TerminateThread` on Windows and `pthread_create`/`pthread_cancel` on Unix-like systems for low-level thread management.
*   **Thread Safety:** The `Core` object itself is designed to be used from the main thread (or a single managing thread). Its methods for adding/stopping tasks are called from the main thread, and its signals are emitted from the main thread context. Access to the internal stop flag (`Core::stopTaskFlag()`) is intended for use *within* the executing task's thread.
*   **Header-Only:** The library is implemented entirely within `core.h` as an inline/header-only library.
*   **Requirements:** Requires Qt 6.x (specifically tested against 6.10.2) and C++17 support.

## Prerequisites

*   Qt 5 or 6.x (tested with 6.10.2)
*   C++17 compatible compiler

## Installation

1.  Clone or download this repository.
2.  Copy the `core.h` file into your Qt project directory (or any preferred location within your source tree).
3.  Include `core.h` in your source code file where you want to use the `Core` class (e.g., `#include "core.h"`).
4.  Ensure your project is configured to use C++17 and Qt 6.x.

## API Overview

*   `Core`: Main class for task management.
    *   `registerTask(...)`: Registers a callable as a task.
    *   `addTask(...)`: Queues a registered task for execution.
    *   `stopTaskById/ByType/ByGroup(...)`: Requests graceful stop.
    *   `terminateTaskById(...)`: Forces immediate termination.
    *   `isIdle()`, `isTaskRegistered()`, etc.: Query methods.
    *   Signals: `startedTask`, `finishedTask`, `terminatedTask`.
*   `TaskHelper`: Internal helper class for thread execution (usually not used directly).

## Basic Example

```cpp
#include "core.h"
#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    Core core;

    // Define a simple task
    auto simpleTask = [](int x) -> int {
        qDebug() << "Running simple task with arg:" << x;
        return x * 2;
    };

    // Register the task with type ID 1
    core.registerTask(1, simpleTask);

    // Connect to the finished signal to handle results
    QObject::connect(&core, &Core::finishedTask, [](long id, int type, const QVariantList &args, const QVariant &result) {
        qDebug() << "Task finished:" << id << "Type:" << type << "Args:" << args << "Result:" << result;
    });

    // Add the task for execution with argument 21
    core.addTask(1, 21);

    // Your application event loop would normally run here.
    // For this example, we'll just wait a bit to see the task complete.
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(2000); // Wait 2 seconds
    QObject::connect(&timer, &QTimer::timeout, &app, &QApplication::quit);

    return app.exec();
}
```

## Grouping Example

```cpp
#include "core.h"
#include <QApplication>
#include <QDebug>
#include <QThread>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    Core core;

    // Define tasks for Group 1 (Resource A)
    auto taskForResourceA1 = [](int id) -> int {
        qDebug() << "Group 1 Task" << id << "- Starting on thread:" << QThread::currentThread();
        QThread::msleep(2000); // Simulate work taking 2 seconds
        qDebug() << "Group 1 Task" << id << "- Finished";
        return id * 10;
    };

    auto taskForResourceA2 = [](int id) -> int {
        qDebug() << "Group 1 Task" << id << "- Starting on thread:" << QThread::currentThread();
        QThread::msleep(1000); // Simulate work taking 1 second
        qDebug() << "Group 1 Task" << id << "- Finished";
        return id * 20;
    };

    // Define a task for Group 2 (Resource B) - Can run concurrently with Group 1
    auto taskForResourceB = [](int id) -> int {
        qDebug() << "Group 2 Task" << id << "- Starting on thread:" << QThread::currentThread();
        QThread::msleep(1500); // Simulate work taking 1.5 seconds
        qDebug() << "Group 2 Task" << id << "- Finished";
        return id * 30;
    };

    // Register tasks. Group 1 tasks will be serialized.
    core.registerTask(1, taskForResourceA1, 1); // Task type 1, Group 1
    core.registerTask(2, taskForResourceA2, 1); // Task type 2, Group 1
    core.registerTask(3, taskForResourceB, 2);  // Task type 3, Group 2

    QObject::connect(&core, &Core::finishedTask, [](long id, int type, const QVariantList &args, const QVariant &result) {
        qDebug() << "Task completed - ID:" << id << "Type:" << type << "Group:" << args.first().toInt() << "Result:" << result;
    });

    // Add tasks
    qDebug() << "Adding Group 1 Task 1 (ID: 10)";
    core.addTask(1, 10); // Will start immediately

    qDebug() << "Adding Group 1 Task 2 (ID: 20) - Should wait for Task 1";
    core.addTask(2, 20); // Will wait in queue behind Task 1

    qDebug() << "Adding Group 2 Task 1 (ID: 30) - Should start immediately, parallel to Group 1 Task 1";
    core.addTask(3, 30); // Will start immediately as it's in Group 2

    // Wait longer to ensure all tasks complete
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(6000); // Wait 6 seconds
    QObject::connect(&timer, &QTimer::timeout, &app, &QApplication::quit);

    return app.exec();
}
/* Expected Output (order might vary slightly due to timing):
Adding Group 1 Task 1 (ID: 10)
Adding Group 1 Task 2 (ID: 20) - Should wait for Task 1
Adding Group 2 Task 1 (ID: 30) - Should start immediately, parallel to Group 1 Task 1
Group 1 Task 10 - Starting on thread: QThread(0x...)
Group 2 Task 30 - Starting on thread: QThread(0x...) 
Group 2 Task 30 - Finished
Task completed - ID: 2 Type: 3 Group: 2 Result: 900
Group 1 Task 10 - Finished
Task completed - ID: 0 Type: 1 Group: 1 Result: 100
Group 1 Task 20 - Starting on thread: QThread(0x...) 
Group 1 Task 20 - Finished
Task completed - ID: 1 Type: 2 Group: 1 Result: 400
*/
```

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

## License

MIT License

Copyright (c) 2026 Vitalii (valeksan)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.