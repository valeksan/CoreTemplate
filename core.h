// core.h
#ifndef CORE_H
#define CORE_H

// --- Import Qt headers ---
#include <QObject>
#include <QQueue>
#include <QVariant>
#include <QList>
#include <QSharedPointer>
#include <QHash>
#include <QMetaType>
#include <QDebug>
#include <QTimer>

// --- Import the headers of the standard library ---
#include <atomic>
#include <functional>
#include <any>
#include <type_traits>
#include <algorithm>
#include <utility>

#ifdef Q_OS_WIN
    #include <windows.h>
    #include <process.h>
#else
    #include <pthread.h>
#endif

// --- Using aliases to improve readability ---
using TaskId = long;
using TaskType = int;
using TaskGroup = int;
using QVariantList = QList<QVariant>; // Qt already provides, but for clarity
using TaskStopTimeout = int; // ms

// --- Declaring constants with macros ---
#define STOP_ACTIVE_TASK_DEFAULT_TIMEOUT 1000

// --- Templates for checking convertibility ---
template<typename T>
struct all_convertible_to {
    template<typename... Args>
    static constexpr bool check() {
        return std::conjunction_v<std::is_convertible<Args, T>...>;
    }
};

template<>
struct all_convertible_to<QVariant> {
    template<typename... Args>
    static constexpr bool check() {
        return std::conjunction_v<std::disjunction<std::is_convertible<Args, QVariant>, std::bool_constant<QMetaTypeId<Args>::Defined>>...>;
    }
};

template<int N>
struct placeholder {};

/// @cond INTERNAL_DOCS
namespace std {
template<int N>
struct is_placeholder<::placeholder<N>> : public std::integral_constant<int, N> {};
}
/// @endcond

// --- Helper functions for binding member methods ---
template<typename R, typename... Args, typename Class, std::size_t... N>
auto bind_placeholders(R (Class::*taskFunction)(Args...), Class* taskObj, std::index_sequence<N...>) {
    return std::bind(taskFunction, taskObj, placeholder<N + 1>()...);
}

template<typename R, typename... Args, typename Class, std::size_t... N>
auto bind_placeholders(R (Class::*taskFunction)(Args...) const, Class* taskObj, std::index_sequence<N...>) {
    return std::bind(taskFunction, taskObj, placeholder<N + 1>()...);
}

// --- Classes ---
class TaskHelper : public QObject {
    Q_OBJECT

public:
    explicit TaskHelper(std::function<QVariant()> function, QObject* parent = nullptr);

#ifdef Q_OS_WIN
    static DWORD WINAPI functionWrapper(void* pTaskHelper);
#else
    static void* functionWrapper(void* pTaskHelper);
#endif

private:
    std::function<QVariant()> m_function;
    void execute(); // Method declaration

signals:
    void finished(QVariant result);
};

class Core : public QObject {
    Q_OBJECT

public:
    explicit Core(QObject* parent = nullptr);

    template <typename R, typename... Args>
    void registerTask(int taskType, std::function<R(Args...)> taskFunction, int taskGroup = 0, int taskStopTimeout = 1000);

    template <typename R, typename... Args>
    void registerTask(int taskType, R (*taskFunction)(Args...), int taskGroup = 0, int taskStopTimeout = 1000);

    template <typename Class, typename R, typename... Args>
    void registerTask(int taskType, R (Class::*taskMethod)(Args...), Class* taskObj, int taskGroup = 0, int taskStopTimeout = 1000);

    template <typename Class, typename R, typename... Args>
    void registerTask(int taskType, R (Class::*taskMethod)(Args...) const, Class* taskObj, int taskGroup = 0, int taskStopTimeout = 1000);

    // More generic overload (for lambdas, functors, results of std::bind)
    template <typename F>
    void registerTask(int taskType, F&& taskFunction, int taskGroup = 0, int taskStopTimeout = 1000);

    bool unregisterTask(int taskType);

    template <typename... Args>
    void addTask(int taskType, Args... args);

    std::atomic_bool* stopTaskFlag();
    void terminateTaskById(long id);
    void stopTaskById(long id);
    void stopTaskByType(int type);
    void stopTaskByGroup(int group);
    void stopTasks();
    bool isTaskRegistered(int type);
    int groupByTask(int type, bool* ok = nullptr);
    bool isIdle();
    bool isTaskAddedByType(int type, bool* isActive = nullptr);
    bool isTaskAddedByGroup(int group, bool* isActive = nullptr);

private:
    enum TaskState {
        INACTIVE,
        ACTIVE,
        FINISHED,
        TERMINATED
    };

    struct TaskInfo {
        std::any m_function;
        int m_group;
        int m_stopTimeout;
    };

    struct Task {
        Task(std::function<QVariant()> functionBound, int type, int group, QVariantList argsList = QVariantList());

        long m_id;
        std::function<QVariant()> m_functionBound;
        int m_type;
        int m_group;
        QVariantList m_argsList;
#ifdef Q_OS_WIN
        HANDLE m_threadHandle;
        DWORD m_threadId;
#else
        pthread_t m_threadHandle;
#endif
        std::atomic_bool m_stopFlag;
        TaskState m_state;
    };

    QSharedPointer<Task> activeTaskById(long id);
    QSharedPointer<Task> activeTaskByType(int type);
    QSharedPointer<Task> activeTaskByGroup(int group);

    void terminateTask(QSharedPointer<Task> pTask);
    void stopTask(QSharedPointer<Task> pTask);
    void startTask(QSharedPointer<Task> pTask);
    void startQueuedTask();

    template <typename... Args>
    void insertToTaskHash(int taskType, std::function<QVariant(Args...)> taskFunction, int taskGroup = 0, int taskStopTimeout = 1000);

    QHash<int, TaskInfo> m_taskHash;
    QList<QSharedPointer<Task>> m_activeTaskList;
    QList<QSharedPointer<Task>> m_queuedTaskList;
    bool m_blockStartTask;

signals:
    void finishedTask(long id, int type, QVariantList argsList = QVariantList(), QVariant result = QVariant());
    void startedTask(long id, int type, QVariantList argsList = QVariantList());
    void terminatedTask(long id, int type, QVariantList argsList = QVariantList());
};

// --- Class method implementations *after* class declarations ---
// Using inline to avoid duplicate symbols

// TaskHelper Implementation
inline TaskHelper::TaskHelper(std::function<QVariant()> function, QObject* parent)
    : QObject(parent), m_function(function) {}

inline void TaskHelper::execute() {
    emit finished(m_function());
}

#ifdef Q_OS_WIN
inline DWORD TaskHelper::functionWrapper(void* pTaskHelper) {
    TaskHelper *pThisTaskHelper = qobject_cast<TaskHelper *>(reinterpret_cast<QObject *>(pTaskHelper));
    if(pThisTaskHelper) pThisTaskHelper->execute();
    return 0;
}
#else
inline void* TaskHelper::functionWrapper(void* pTaskHelper) {
    TaskHelper *pThisTaskHelper = qobject_cast<TaskHelper *>(reinterpret_cast<QObject *>(pTaskHelper));
    if(pThisTaskHelper) pThisTaskHelper->execute();
    return nullptr;
}
#endif

// Core Implementation
inline Core::Core(QObject* parent)
    : QObject(parent), m_blockStartTask(false) {}

template <typename R, typename... Args>
void Core::registerTask(int taskType, std::function<R(Args...)> taskFunction, int taskGroup, int taskStopTimeout) {
    std::function<QVariant(std::remove_reference_t<Args>...)> f;

    if constexpr (std::is_void_v<R>) {
        f = [taskFunction](std::remove_reference_t<Args>... args) -> QVariant {
            taskFunction(args...);
            return QVariant();
        };
    } else if constexpr (std::is_convertible_v<R, QVariant>) {
        f = [taskFunction](std::remove_reference_t<Args>... args) -> QVariant {
            return taskFunction(args...);
        };
    } else if constexpr (QMetaTypeId<R>::Defined) {
        f = [taskFunction](std::remove_reference_t<Args>... args) -> QVariant {
            return QVariant::fromValue(taskFunction(args...));
        };
    } else {
        qWarning() << "Core::registerTask - Not convertible return type for task type:" << taskType;
        throw std::logic_error("Not convertible return type");
    }

    insertToTaskHash(taskType, f, taskGroup, taskStopTimeout);
}

template <typename R, typename... Args>
void Core::registerTask(int taskType, R (*taskFunction)(Args...), int taskGroup, int taskStopTimeout) {
    registerTask(taskType, std::function<R(Args...)>(taskFunction), taskGroup, taskStopTimeout);
}

template <typename Class, typename R, typename... Args>
void Core::registerTask(int taskType, R (Class::*taskMethod)(Args...), Class* taskObj, int taskGroup, int taskStopTimeout) {
    // Direct call to bind_placeholders and registerTask with explicit std::function signature
    auto boundFunc = bind_placeholders(taskMethod, taskObj, std::make_index_sequence<sizeof...(Args)>());
    // Explicitly specify std::function type for the result of std::bind
    std::function<R(Args...)> func = boundFunc;
    registerTask(taskType, func, taskGroup, taskStopTimeout);
}

template <typename Class, typename R, typename... Args>
void Core::registerTask(int taskType, R (Class::*taskMethod)(Args...) const, Class* taskObj, int taskGroup, int taskStopTimeout) {
    auto boundFunc = bind_placeholders(taskMethod, taskObj, std::make_index_sequence<sizeof...(Args)>());
    std::function<R(Args...)> func = boundFunc;
    registerTask(taskType, func, taskGroup, taskStopTimeout);
}

// Generic overload (works for lambdas, functors)
template <typename F>
void Core::registerTask(int taskType, F&& taskFunction, int taskGroup, int taskStopTimeout) {
    // Try to convert to std::function, relying on template argument deduction
    // This works if F has an operator() with a unique signature
    registerTask(taskType, std::function(std::forward<F>(taskFunction)), taskGroup, taskStopTimeout);
}

inline bool Core::unregisterTask(int taskType) {
    return m_taskHash.remove(taskType) > 0;
}

template <typename... Args>
void Core::addTask(int taskType, Args... args) {
    if (!m_taskHash.contains(taskType)) {
        qWarning() << "Core::addTask - Task not registered for type:" << taskType;
        throw std::logic_error("Task not registered");
    }

    try {
        auto storedFuncAny = m_taskHash[taskType].m_function;
        auto taskFunction = std::any_cast<std::function<QVariant(Args...)>>(storedFuncAny);

        QVariantList argsList;
        if constexpr (all_convertible_to<QVariant>::check<Args...>()) {
            argsList = { QVariant::fromValue(args)... };
        } else {
            qWarning() << "Core::addTask - Arguments are not convertible to QVariantList for task type:" << taskType;
        }

        auto taskFunctionBound = std::bind(taskFunction, args...);
        QSharedPointer<Task> pTask = QSharedPointer<Task>::create(taskFunctionBound, taskType, m_taskHash[taskType].m_group, argsList);

        bool start = std::all_of(m_activeTaskList.begin(), m_activeTaskList.end(), [pTask](QSharedPointer<Task> pActiveTask) {
            return pTask->m_group != pActiveTask->m_group;
        });

        if (start && !m_blockStartTask) {
            startTask(pTask);
        } else {
            m_queuedTaskList.append(pTask);
        }
    } catch (const std::bad_any_cast& e) {
        qWarning() << "Core::addTask - Bad arguments or function signature mismatch for task type:" << taskType << e.what();
        throw std::logic_error("Bad arguments or function signature mismatch");
    }
}

inline std::atomic_bool* Core::stopTaskFlag() {
    for (auto& task : m_activeTaskList) {
#ifdef Q_OS_WIN
        if (task->m_threadId == GetCurrentThreadId()) {
            return &task->m_stopFlag;
        }
#else
        if (pthread_equal(pthread_self(), task->m_threadHandle)) {
            return &task->m_stopFlag;
        }
#endif
    }
    return nullptr;
}

inline void Core::terminateTaskById(long id) {
    auto pTask = activeTaskById(id);
    if (!pTask.isNull()) {
        terminateTask(pTask);
    }
}

inline void Core::stopTaskById(long id) {
    auto pTask = activeTaskById(id);
    if (!pTask.isNull()) {
        stopTask(pTask);
    }
}

inline void Core::stopTaskByType(int type) {
    auto pTask = activeTaskByType(type);
    if (!pTask.isNull()) {
        stopTask(pTask);
    }
}

inline void Core::stopTaskByGroup(int group) {
    auto pTask = activeTaskByGroup(group);
    if (!pTask.isNull()) {
        stopTask(pTask);
    }
}

inline void Core::stopTasks() {
    m_blockStartTask = true;
    QTimer* pTimer = new QTimer(this); // Add parent
    connect(pTimer, &QTimer::timeout, this, [this, pTimer]() {
        if (m_activeTaskList.isEmpty()) {
            m_blockStartTask = false;
            pTimer->stop();
            pTimer->deleteLater();
        }
    });

    int stopTasksTimeout = 0;
    for (auto& pTask : m_activeTaskList) {
        int stopTaskTimeout = m_taskHash[pTask->m_type].m_stopTimeout;
        if (stopTaskTimeout > stopTasksTimeout) {
            stopTasksTimeout = stopTaskTimeout;
        }
        stopTask(pTask);
    }
    pTimer->start(stopTasksTimeout);
}

inline bool Core::isTaskRegistered(int type) {
    return m_taskHash.contains(type);
}

inline int Core::groupByTask(int type, bool* ok) {
    if (isTaskRegistered(type)) {
        if (ok) *ok = true;
        return m_taskHash[type].m_group;
    } else {
        if (ok) *ok = false;
        return -1;
    }
}

inline bool Core::isIdle() {
    return m_activeTaskList.isEmpty();
}

inline bool Core::isTaskAddedByType(int type, bool* isActive) {
    for (auto& pTask : m_activeTaskList) {
        if (pTask->m_type == type) {
            if (isActive) *isActive = true;
            return true;
        }
    }
    for (auto& pTask : m_queuedTaskList) {
        if (pTask->m_type == type) {
            if (isActive) *isActive = false;
            return true;
        }
    }
    return false;
}

inline bool Core::isTaskAddedByGroup(int group, bool* isActive) {
    for (auto& pTask : m_activeTaskList) {
        if (pTask->m_group == group) {
            if (isActive) *isActive = true;
            return true;
        }
    }
    for (auto& pTask : m_queuedTaskList) {
        if (pTask->m_group == group) {
            if (isActive) *isActive = false;
            return true;
        }
    }
    return false;
}

// --- Internal method implementations (inline) ---

inline QSharedPointer<Core::Task> Core::activeTaskById(long id) {
    for (auto& pTask : m_activeTaskList) {
        if (pTask->m_id == id) {
            return pTask;
        }
    }
    return nullptr;
}

inline QSharedPointer<Core::Task> Core::activeTaskByType(int type) {
    for (auto& pTask : m_activeTaskList) {
        if (pTask->m_type == type) {
            return pTask;
        }
    }
    return nullptr;
}

inline QSharedPointer<Core::Task> Core::activeTaskByGroup(int group) {
    for (auto& pTask : m_activeTaskList) {
        if (pTask->m_group == group) {
            return pTask;
        }
    }
    return nullptr;
}

inline void Core::terminateTask(QSharedPointer<Core::Task> pTask) {
#ifdef Q_OS_WIN
    TerminateThread(pTask->m_threadHandle, 0);
#else
    pthread_cancel(pTask->m_threadHandle);
#endif
    pTask->m_state = TERMINATED;
    emit terminatedTask(pTask->m_id, pTask->m_type, pTask->m_argsList);
    m_activeTaskList.removeAll(pTask);
    startQueuedTask();
}

inline void Core::stopTask(QSharedPointer<Core::Task> pTask) {
    pTask->m_stopFlag.store(true);
    QTimer::singleShot(m_taskHash[pTask->m_type].m_stopTimeout, this, [this, pTask]() {
        switch (pTask->m_state) {
        case FINISHED:
            qDebug() << QString("Task %1 was successfully stopped").arg(QString::number(pTask->m_id));
            break;
        case TERMINATED:
            qDebug() << QString("Task %1 was terminated").arg(QString::number(pTask->m_id));
            break;
        case ACTIVE:
            qDebug() << QString("Task %1 was not stopped, terminating").arg(QString::number(pTask->m_id));
            terminateTask(pTask);
            break;
        default:
            qDebug() << QString("Task %1 unexpected state").arg(QString::number(pTask->m_id));
            break;
        }
    });
}

inline void Core::startTask(QSharedPointer<Core::Task> pTask) {
    m_activeTaskList.append(pTask);
    pTask->m_state = ACTIVE;
    TaskHelper* pTaskHelper = new TaskHelper(pTask->m_functionBound, this); // Add parent

    connect(pTaskHelper, &TaskHelper::finished, this, [this, pTask, pTaskHelper](QVariant result) {
        pTask->m_state = FINISHED;
        emit finishedTask(pTask->m_id, pTask->m_type, pTask->m_argsList, result);
        m_activeTaskList.removeAll(pTask);
        startQueuedTask();
        pTaskHelper->deleteLater();
    });

#ifdef Q_OS_WIN
    pTask->m_threadHandle = CreateThread(nullptr, 0, &TaskHelper::functionWrapper, reinterpret_cast<void*>(pTaskHelper), 0, &pTask->m_threadId);
#else
    pthread_create(&pTask->m_threadHandle, nullptr, &TaskHelper::functionWrapper, pTaskHelper);
    pthread_detach(pTask->m_threadHandle);
#endif

    emit startedTask(pTask->m_id, pTask->m_type, pTask->m_argsList);
}

inline void Core::startQueuedTask() {
    for (auto it = m_queuedTaskList.begin(); it != m_queuedTaskList.end();) {
        QSharedPointer<Task> pQueuedTask = *it;
        bool start = std::all_of(m_activeTaskList.begin(), m_activeTaskList.end(), [pQueuedTask](QSharedPointer<Task> pActiveTask) {
            return pQueuedTask->m_group != pActiveTask->m_group;
        });
        if (start) {
            it = m_queuedTaskList.erase(it);
            startTask(pQueuedTask);
        } else {
            ++it;
        }
    }
}

template <typename... Args>
void Core::insertToTaskHash(int taskType, std::function<QVariant(Args...)> taskFunction, int taskGroup, int taskStopTimeout) {
    if (m_taskHash.contains(taskType)) {
        qWarning() << "Core::registerTask - Task type is already registered:" << taskType;
        throw std::logic_error("Task type is already registered");
    }
    m_taskHash.insert(taskType, TaskInfo{taskFunction, taskGroup, taskStopTimeout});
}

// --- Task implementation (inline) ---

inline Core::Task::Task(std::function<QVariant()> functionBound, int type, int group, QVariantList argsList)
    : m_functionBound(functionBound), m_type(type), m_group(group), m_argsList(argsList) {
    static long id = 0;
    m_id = id++;
    m_stopFlag.store(false);
    m_state = INACTIVE;
}

#endif // CORE_H
