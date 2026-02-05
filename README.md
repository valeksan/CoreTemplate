# \# CoreTaskManager

# 

# A Qt-based library designed to manage and execute tasks asynchronously in separate threads. It provides features for task registration, queuing, grouping (to limit concurrency within groups), and controlling running tasks (stopping, terminating).

# 

# \## Features

# 

# \*   \*\*Asynchronous Execution:\*\* Tasks run in dedicated background threads managed by the library.

# \*   \*\*Task Registration:\*\* Register various callable objects (free functions, member functions, lambdas, functors) as tasks with a unique integer type identifier.

# \*   \*\*Flexible Arguments:\*\* Supports tasks with various argument types, provided they are compatible with `QVariant`.

# \*   \*\*Return Values:\*\* Supports tasks returning values (convertible to `QVariant`) or `void`. Results are emitted via signals upon completion.

# \*   \*\*Task Groups:\*\* Assign tasks to groups. Only one task per group runs concurrently, allowing for resource management or serialization of related operations.

# \*   \*\*Task Control:\*\* Start tasks, request graceful stops (using internal flags), force termination (via platform-specific calls like `TerminateThread`/`pthread\_cancel`), and stop all tasks.

# \*   \*\*Signals:\*\* Emits signals when tasks start, finish successfully, or are terminated, providing details like task ID, type, arguments, and results.

# \*   \*\*Qt Integration:\*\* Inherits from `QObject` and leverages Qt's signal/slot mechanism and threading primitives.

# 

# \## How It Works

# 

# 1\.  An instance of the `Core` class is created.

# 2\.  Callables are registered with `Core::registerTask(...)`, assigning them a unique `taskType` integer and optional group and timeout settings.

# 3\.  Tasks are queued for execution using `Core::addTask(taskType, ...args)`.

# 4\.  The `Core` manages a queue and ensures only one task per group runs at a time.

# 5\.  When a slot opens up (either due to a previous task finishing or because the task belongs to a different group), the `Core` starts the next eligible task in its own thread using `CreateThread` (Windows) or `pthread\_create` (Unix-like systems).

# 6\.  The task's associated function executes within the new thread.

# 7\.  While executing, a task can check a shared stop flag retrieved via `Core::stopTaskFlag()` to perform graceful shutdowns.

# 8\.  Upon completion (normal, stopped, or terminated), the task emits a signal (`finishedTask`, `terminatedTask`) back to the main thread where the `Core` lives.

# 9\.  The `Core` updates its internal lists of active and queued tasks and proceeds to start the next queued task if applicable.

# 

# \## Example Usage

# 

# See the `MainWindow` constructor in the provided `mainwindow.cpp` for a comprehensive example demonstrating how to register different types of tasks (functions, member functions, lambdas, functors) and add them for execution.

# 

# \## Important Notes

# 

# \*   \*\*Platform Specifics:\*\* The library uses `CreateThread`/`TerminateThread` on Windows and `pthread\_create`/`pthread\_cancel` on Unix-like systems for low-level thread management.

# \*   \*\*Thread Safety:\*\* The `Core` object itself is designed to be used from the main thread (or a single managing thread). Its methods for adding/stopping tasks are called from the main thread, and its signals are emitted from the main thread context. Access to the internal stop flag (`Core::stopTaskFlag()`) is intended for use \*within\* the executing task's thread.

# \*   \*\*Header-Only:\*\* The library is implemented entirely within `core.h` as an inline/header-only library.

# \*   \*\*Requirements:\*\* Requires Qt 6.x (specifically tested against 6.10.2) and C++17 support.

# 

# \## License

# 

# MIT License

# 

# Copyright (c) 2026 Vitalii (valeksan)

# 

# Permission is hereby granted, free of charge, to any person obtaining a copy

# of this software and associated documentation files (the "Software"), to deal

# in the Software without restriction, including without limitation the rights

# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell

# copies of the Software, and to permit persons to whom the Software is

# furnished to do so, subject to the following conditions:

# 

# The above copyright notice and this permission notice shall be included in all

# copies or substantial portions of the Software.

# 

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR

# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,

# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE

# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER

# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,

# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE

# SOFTWARE.

