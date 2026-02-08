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
using TaskStopTimeout = int; // ms

// --- Declaring constants with macros ---
#define DEFAULT_STOP_TIMEOUT 1000

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
    void registerTask(TaskType taskType, std::function<R(Args...)> taskFunction, TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = DEFAULT_STOP_TIMEOUT);

    template <typename R, typename... Args>
    void registerTask(TaskType taskType, R (*taskFunction)(Args...), TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = DEFAULT_STOP_TIMEOUT);

    template <typename Class, typename R, typename... Args>
    void registerTask(TaskType taskType, R (Class::*taskMethod)(Args...), Class* taskObj, TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = DEFAULT_STOP_TIMEOUT);

    template <typename Class, typename R, typename... Args>
    void registerTask(TaskType taskType, R (Class::*taskMethod)(Args...) const, Class* taskObj, TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = DEFAULT_STOP_TIMEOUT);

    // More generic overload (for lambdas, functors, results of std::bind)
    template <typename F>
    void registerTask(TaskType taskType, F&& taskFunction, TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = DEFAULT_STOP_TIMEOUT);

    bool unregisterTask(TaskType taskType);

    template <typename... Args>
    void addTask(TaskType taskType, Args... args);

    std::atomic_bool* stopTaskFlag();
    void terminateTaskById(TaskId id);
    void stopTaskById(TaskId id);
    void stopTaskByType(TaskType type);
    void stopTaskByGroup(TaskGroup group);
    void stopTasks();
    bool isTaskRegistered(TaskType type);
    TaskGroup groupByTask(TaskType type, bool* ok = nullptr);
    bool isIdle();
    bool isTaskAddedByType(TaskType type, bool* isActive = nullptr);
    bool isTaskAddedByGroup(TaskGroup group, bool* isActive = nullptr);

private:
    enum class TaskState {
        Inactive,
        Active,
        Finished,
        Terminated
    };

    struct TaskInfo {
        std::any m_function;
        TaskGroup m_group;
        TaskStopTimeout m_stopTimeout;
    };

    struct Task {
        Task(std::function<QVariant()> functionBound, TaskType type, TaskGroup group, QVariantList argsList = {})
            : m_functionBound(std::move(functionBound))
            , m_type(type)
            , m_group(group)
            , m_argsList(std::move(argsList))
            , m_state(TaskState::Inactive) {

            static TaskId id_counter = 0;
            m_id = id_counter++; // m_id cannot be initialized in the list, because it depends on the counter
        }

        TaskId m_id;
        std::function<QVariant()> m_functionBound;
        TaskType m_type;
        TaskGroup m_group;
        QVariantList m_argsList;
    #ifdef Q_OS_WIN
        HANDLE m_threadHandle = nullptr;
        DWORD m_threadId = 0;
    #else
        pthread_t m_threadHandle = 0;
    #endif
        std::atomic_bool m_stopFlag{false};
        TaskState m_state;
    };

    QSharedPointer<Task> activeTaskById(TaskId id);
    QSharedPointer<Task> activeTaskByType(TaskType type);
    QSharedPointer<Task> activeTaskByGroup(TaskGroup group);

    void terminateTask(QSharedPointer<Task> pTask);
    void stopTask(QSharedPointer<Task> pTask);
    void startTask(QSharedPointer<Task> pTask);
    void startQueuedTask();

    template <typename... Args>
    void insertToTaskHash(TaskType taskType, std::function<QVariant(Args...)> taskFunction, TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = DEFAULT_STOP_TIMEOUT);

    QHash<TaskType, TaskInfo> m_taskHash;
    QList<QSharedPointer<Task>> m_activeTaskList;
    QList<QSharedPointer<Task>> m_queuedTaskList;
    bool m_blockStartTask = false;

signals:
    void finishedTask(TaskId id, TaskType type, QVariantList argsList = QVariantList(), QVariant result = QVariant());
    void startedTask(TaskId id, TaskType type, QVariantList argsList = QVariantList());
    void terminatedTask(TaskId id, TaskType type, QVariantList argsList = QVariantList());
};

// --- Class method implementations *after* class declarations ---

// TaskHelper Implementation
inline TaskHelper::TaskHelper(std::function<QVariant()> function, QObject* parent)
    : QObject(parent), m_function(function) {}

inline void TaskHelper::execute() {
    emit finished(m_function());
}

#ifdef Q_OS_WIN
inline DWORD TaskHelper::functionWrapper(void* pTaskHelper) {
    TaskHelper *pThisTaskHelper = qobject_cast<TaskHelper *>(reinterpret_cast<QObject *>(pTaskHelper));
    if (pThisTaskHelper) pThisTaskHelper->execute();
    return 0;
}
#else
inline void* TaskHelper::functionWrapper(void* pTaskHelper) {
    TaskHelper *pThisTaskHelper = qobject_cast<TaskHelper *>(reinterpret_cast<QObject *>(pTaskHelper));
    if (pThisTaskHelper) pThisTaskHelper->execute();
    return nullptr;
}
#endif

// Core Implementation
inline Core::Core(QObject* parent)
    : QObject(parent) {}

template <typename R, typename... Args>
void Core::registerTask(TaskType taskType, std::function<R(Args...)> taskFunction, TaskGroup taskGroup, TaskStopTimeout taskStopTimeout) {
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

    insertToTaskHash(taskType, std::move(f), taskGroup, taskStopTimeout);
}

template <typename R, typename... Args>
void Core::registerTask(TaskType taskType, R (*taskFunction)(Args...), TaskGroup taskGroup, TaskStopTimeout taskStopTimeout) {
    // Use deduction guide for std::function
    registerTask(taskType, std::function(taskFunction), taskGroup, taskStopTimeout);
}

template <typename Class, typename R, typename... Args>
void Core::registerTask(TaskType taskType, R (Class::*taskMethod)(Args...), Class* taskObj, TaskGroup taskGroup, TaskStopTimeout taskStopTimeout) {
    // Direct call to bind_placeholders and registerTask with explicit std::function signature
    auto boundFunc = bind_placeholders(taskMethod, taskObj, std::make_index_sequence<sizeof...(Args)>());
    // Use deduction guide for std::function
    registerTask(taskType, static_cast<std::function<R(Args...)>>(std::move(boundFunc)), taskGroup, taskStopTimeout);
}

template <typename Class, typename R, typename... Args>
void Core::registerTask(TaskType taskType, R (Class::*taskMethod)(Args...) const, Class* taskObj, TaskGroup taskGroup, TaskStopTimeout taskStopTimeout) {
    auto boundFunc = bind_placeholders(taskMethod, taskObj, std::make_index_sequence<sizeof...(Args)>());
    // Use deduction guide for std::function
    registerTask(taskType, static_cast<std::function<R(Args...)>>(std::move(boundFunc)), taskGroup, taskStopTimeout);
}

// Generic overload (works for lambdas, functors)
template <typename F>
void Core::registerTask(TaskType taskType, F&& taskFunction, TaskGroup taskGroup, TaskStopTimeout taskStopTimeout) {
    // Try to convert to std::function, relying on template argument deduction
    // This works if F has an operator() with a unique signature
    registerTask(taskType, std::function(std::forward<F>(taskFunction)), taskGroup, taskStopTimeout);
}

inline bool Core::unregisterTask(TaskType taskType) {
    return m_taskHash.remove(taskType) > 0;
}

template <typename... Args>
void Core::addTask(TaskType taskType, Args... args) {
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
        auto pTask = QSharedPointer<Task>::create(
            std::move(taskFunctionBound),
            taskType,
            m_taskHash[taskType].m_group,
            std::move(argsList)
            );

        bool start = std::none_of(m_activeTaskList.begin(), m_activeTaskList.end(), [group = pTask->m_group](const auto& pActiveTask) {
            return pActiveTask->m_group == group;
        });

        if (start && !m_blockStartTask) {
            startTask(pTask);
        } else {
            m_queuedTaskList.append(std::move(pTask));
        }
    } catch (const std::bad_any_cast& e) {
        qWarning() << "Core::addTask - Bad arguments or function signature mismatch for task type:" << taskType << e.what();
        throw std::logic_error("Bad arguments or function signature mismatch");
    }
}

inline std::atomic_bool* Core::stopTaskFlag() {
    auto it = std::find_if(m_activeTaskList.begin(), m_activeTaskList.end(), [](const auto& task) {
        #ifdef Q_OS_WIN
        return task->m_threadId == GetCurrentThreadId();
        #else
        return pthread_equal(pthread_self(), task->m_threadHandle);
        #endif
    });

    if (it != m_activeTaskList.end()) {
        return &(*it)->m_stopFlag;
    }
    return nullptr;
}

inline void Core::terminateTaskById(TaskId id) {
    if (auto pTask = activeTaskById(id); !pTask.isNull()) {
        terminateTask(std::move(pTask));
    }
}

inline void Core::stopTaskById(TaskId id) {
    if (auto pTask = activeTaskById(id); !pTask.isNull()) {
        stopTask(std::move(pTask));
    }
}

inline void Core::stopTaskByType(TaskType type) {
    if (auto pTask = activeTaskByType(type); !pTask.isNull()) {
        stopTask(std::move(pTask));
    }
}

inline void Core::stopTaskByGroup(TaskGroup group) {
    if (auto pTask = activeTaskByGroup(group); !pTask.isNull()) {
        stopTask(std::move(pTask));
    }
}

inline void Core::stopTasks() {
    m_blockStartTask = true;
    QTimer* pTimer = new QTimer(this); // with parent for automatic cleanup!
    connect(pTimer, &QTimer::timeout, this, [this, pTimer]() {
        if (isIdle()) { // Use the public method
            m_blockStartTask = false;
            pTimer->stop();
            pTimer->deleteLater();
        }
    });

    // Calculating the maximum stop timeout among active tasks
    TaskStopTimeout maxTimeout = 0;
    for (const auto& pTask : m_activeTaskList) { // range-based for
        maxTimeout = std::max(maxTimeout, m_taskHash[pTask->m_type].m_stopTimeout);
    }

    // Requesting to stop all active tasks
    for (const auto& pTask : m_activeTaskList) { // range-based for
        stopTask(pTask);
    }

    // Starting the timer with the maximum timeout
    pTimer->start(maxTimeout);
}

inline bool Core::isTaskRegistered(TaskType type) {
    return m_taskHash.contains(type);
}

inline TaskGroup Core::groupByTask(TaskType type, bool* ok) {
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

inline bool Core::isTaskAddedByType(TaskType type, bool* isActive) {
    for (const auto& pTask : m_activeTaskList) {
        if (pTask->m_type == type) {
            if (isActive) *isActive = true;
            return true;
        }
    }
    for (const auto& pTask : m_queuedTaskList) {
        if (pTask->m_type == type) {
            if (isActive) *isActive = false;
            return true;
        }
    }
    return false;
}

inline bool Core::isTaskAddedByGroup(TaskGroup group, bool* isActive) {
    for (const auto& pTask : m_activeTaskList) {
        if (pTask->m_group == group) {
            if (isActive) *isActive = true;
            return true;
        }
    }
    for (const auto& pTask : m_queuedTaskList) {
        if (pTask->m_group == group) {
            if (isActive) *isActive = false;
            return true;
        }
    }
    return false;
}

// --- Internal method implementations (inline) ---

inline QSharedPointer<Core::Task> Core::activeTaskById(TaskId id) {
    auto it = std::find_if(m_activeTaskList.begin(), m_activeTaskList.end(),
                           [id](const auto& pTask) { return pTask->m_id == id; }); // range-based for -> find_if
    return (it != m_activeTaskList.end()) ? *it : QSharedPointer<Task>{};
}

inline QSharedPointer<Core::Task> Core::activeTaskByType(TaskType type) {
    auto it = std::find_if(m_activeTaskList.begin(), m_activeTaskList.end(),
                           [type](const auto& pTask) { return pTask->m_type == type; });
    return (it != m_activeTaskList.end()) ? *it : QSharedPointer<Task>{};
}

inline QSharedPointer<Core::Task> Core::activeTaskByGroup(TaskGroup group) {
    auto it = std::find_if(m_activeTaskList.begin(), m_activeTaskList.end(),
                           [group](const auto& pTask) { return pTask->m_group == group; });
    return (it != m_activeTaskList.end()) ? *it : QSharedPointer<Task>{};
}

inline void Core::terminateTask(QSharedPointer<Core::Task> pTask) {
#ifdef Q_OS_WIN
    TerminateThread(pTask->m_threadHandle, 0);
#else
    pthread_cancel(pTask->m_threadHandle);
#endif
    pTask->m_state = TaskState::Terminated;
    emit terminatedTask(pTask->m_id, pTask->m_type, pTask->m_argsList);
    m_activeTaskList.removeAll(pTask);
    startQueuedTask();
}

inline void Core::stopTask(QSharedPointer<Core::Task> pTask) {
    pTask->m_stopFlag.store(true);
    QTimer::singleShot(m_taskHash[pTask->m_type].m_stopTimeout, this, [this, pTask]() {
        switch (pTask->m_state) {
        case TaskState::Finished:
            qDebug() << QString("Task %1 was successfully stopped").arg(QString::number(pTask->m_id));
            break;
        case TaskState::Terminated:
            qDebug() << QString("Task %1 was terminated").arg(QString::number(pTask->m_id));
            break;
        case TaskState::Active:
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
    pTask->m_state = TaskState::Active;
    TaskHelper* pTaskHelper = new TaskHelper(pTask->m_functionBound, this); // Add with parent!

    connect(pTaskHelper, &TaskHelper::finished, this, [this, pTask, pTaskHelper](QVariant result) {
        pTask->m_state = TaskState::Finished;
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
            startTask(std::move(pQueuedTask));
        } else {
            ++it;
        }
    }
}

template <typename... Args>
void Core::insertToTaskHash(TaskType taskType, std::function<QVariant(Args...)> taskFunction, TaskGroup taskGroup, TaskStopTimeout taskStopTimeout) {
    if (m_taskHash.contains(taskType)) {
        qWarning() << "Core::registerTask - Task type is already registered:" << taskType;
        throw std::logic_error("Task type is already registered");
    }
    m_taskHash.insert(taskType, TaskInfo{taskFunction, taskGroup, taskStopTimeout});
}

#endif // CORE_H
