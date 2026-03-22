// core.h
#ifndef CORE_H
#define CORE_H
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L   // POSIX.1-2001 (includes pthread_kill)
#endif

// --- Import the headers of the standard library ---
#include <atomic>
#include <functional>
#include <any>
#include <type_traits>
#include <algorithm>
#include <utility>
#include <stdexcept>

// --- Import Qt headers ---
#include <QObject>
#include <QVariant>
#include <QList>
#include <QSharedPointer>
#include <QHash>
#include <QMetaType>
#include <QDebug>
#include <QTimer>
#include <QElapsedTimer>
#include <QThread>

// --- Threading/Multiprocessing API Headers ---
#ifdef Q_OS_WIN
    #include <windows.h>
#else
    #include <pthread.h>
    #include <signal.h>
#endif

// --- Using aliases to improve readability ---
using TaskId = long;
using TaskType = int;
using TaskGroup = int;
using TaskStopTimeout = int; // ms

namespace core_detail {
inline thread_local std::atomic_bool* g_currentStopFlag = nullptr;
}

// --- Declaring constants ---
inline constexpr TaskStopTimeout kDefaultStopTimeout = 1000;

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
class TaskHelper final : public QObject {
    Q_OBJECT

public:
    explicit TaskHelper(std::function<QVariant()> function, std::atomic_bool* pStopFlag, QObject* parent = nullptr);

#ifdef Q_OS_WIN
    static DWORD WINAPI functionWrapper(void* pTaskHelper);
#else
    static void* functionWrapper(void* pTaskHelper);
#endif

private:
    std::function<QVariant()> m_function;
    std::atomic_bool* m_pStopFlag = nullptr;
    void execute(); // Method declaration

signals:
    void finished(QVariant result);
};

/**
 * @brief The Core class manages task execution in separate threads.
 *
 * IMPORTANT: All public methods of this class (e.g., addTask, stopTaskById, etc.)
 * must be called from the same thread where the Core object lives (typically the main GUI thread).
 * Calling these methods from task threads concurrently may lead to undefined behavior.
 */
class Core : public QObject {
    Q_OBJECT

public:
    explicit Core(QObject* parent = nullptr);

    template <typename R, typename... Args>
    void registerTask(TaskType taskType, std::function<R(Args...)> taskFunction, TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = kDefaultStopTimeout);

    template <typename R, typename... Args>
    void registerTask(TaskType taskType, R (*taskFunction)(Args...), TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = kDefaultStopTimeout);

    template <typename Class, typename R, typename... Args>
    void registerTask(TaskType taskType, R (Class::*taskMethod)(Args...), Class* taskObj, TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = kDefaultStopTimeout);

    template <typename Class, typename R, typename... Args>
    void registerTask(TaskType taskType, R (Class::*taskMethod)(Args...) const, Class* taskObj, TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = kDefaultStopTimeout);

    // More generic overload (for lambdas, functors, results of std::bind)
    template <typename F>
    void registerTask(TaskType taskType, F&& taskFunction, TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = kDefaultStopTimeout);

    bool unregisterTask(TaskType taskType);

    template <typename... Args>
    void addTask(TaskType taskType, Args... args);

    std::atomic_bool* stopTaskFlag();
    void cancelTaskById(TaskId id);
    void cancelTaskByType(TaskType type);
    void cancelTaskByGroup(TaskGroup group);
    void cancelTasks();
    void cancelAllTasks();
    void cancelTasksByGroup(TaskGroup group, bool includeQueued);
    void terminateTaskById(TaskId id);
    void stopTaskById(TaskId id);
    void stopTaskByType(TaskType type);
    void stopTaskByGroup(TaskGroup group);
    void stopTasks();
    void stopAllTasks();
    void stopTasksByGroup(TaskGroup group, bool includeQueued);
    bool isTaskRegistered(TaskType type);
    TaskGroup groupByTask(TaskType type, bool* ok = nullptr);
    bool isIdle();
    bool isTaskAddedByType(TaskType type, bool* isActive = nullptr);
    bool isTaskAddedByGroup(TaskGroup group, bool* isActive = nullptr);

private:
    enum class TaskState {
        Inactive,
        Active,
        StopRequested,
        StopTimedOut,
        Finished,
        Terminated
    };

    struct TaskInfo {
        std::any m_function;
        TaskGroup m_group;
        TaskStopTimeout m_stopTimeout;
    };

    struct Task {
        Task(std::function<QVariant()> functionBound, TaskType type, TaskGroup group, QList<QVariant> argsList = {})
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
        QList<QVariant> m_argsList;
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
    bool ensureCalledFromOwnerThread(const char* method) const;

    template <typename... Args>
    void insertToTaskHash(TaskType taskType, std::function<QVariant(Args...)> taskFunction, TaskGroup taskGroup = 0, TaskStopTimeout taskStopTimeout = kDefaultStopTimeout);

    QHash<TaskType, TaskInfo> m_taskHash;
    QList<QSharedPointer<Task>> m_activeTaskList;
    QList<QSharedPointer<Task>> m_queuedTaskList;
    std::atomic_bool m_blockStartTask{false};

signals:
    void finishedTask(TaskId id, TaskType type, QList<QVariant> argsList = {}, QVariant result = QVariant());
    void startedTask(TaskId id, TaskType type, QList<QVariant> argsList = {});
    void terminatedTask(TaskId id, TaskType type, QList<QVariant> argsList = {});
    void stopRequestedTask(TaskId id, TaskType type, QList<QVariant> argsList = {});
    void stopTimedOutTask(TaskId id, TaskType type, QList<QVariant> argsList = {}, TaskStopTimeout timeout = kDefaultStopTimeout);
};

// --- Class method implementations *after* class declarations ---

// TaskHelper Implementation
inline TaskHelper::TaskHelper(std::function<QVariant()> function, std::atomic_bool* pStopFlag, QObject* parent)
    : QObject(parent), m_function(function), m_pStopFlag(pStopFlag) {}

inline void TaskHelper::execute() {
    core_detail::g_currentStopFlag = m_pStopFlag;
    QVariant result;
    try {
        result = m_function();
    } catch (...) {
        core_detail::g_currentStopFlag = nullptr;
        throw;
    }
    core_detail::g_currentStopFlag = nullptr;
    emit finished(result);
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

inline bool Core::ensureCalledFromOwnerThread(const char* method) const {
    if (QThread::currentThread() == thread()) {
        return true;
    }
    qWarning() << "Core::" << method
               << "- called from non-owner thread. owner =" << thread()
               << ", current =" << QThread::currentThread();
    return false;
}

template <typename R, typename... Args>
void Core::registerTask(TaskType taskType, std::function<R(Args...)> taskFunction, TaskGroup taskGroup, TaskStopTimeout taskStopTimeout) {
    if (!ensureCalledFromOwnerThread("registerTask")) {
        throw std::logic_error("Core::registerTask must be called from the owner thread");
    }

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
    if (!ensureCalledFromOwnerThread("unregisterTask")) {
        return false;
    }

    for (const auto& pTask : std::as_const(m_activeTaskList)) {
        if (pTask->m_type == taskType) {
            qWarning() << "Core::unregisterTask - Cannot unregister active task type:" << taskType;
            return false;
        }
    }
    for (const auto& pTask : std::as_const(m_queuedTaskList)) {
        if (pTask->m_type == taskType) {
            qWarning() << "Core::unregisterTask - Cannot unregister queued task type:" << taskType;
            return false;
        }
    }
    return m_taskHash.remove(taskType) > 0;
}

template <typename... Args>
void Core::addTask(TaskType taskType, Args... args) {
    if (!ensureCalledFromOwnerThread("addTask")) {
        throw std::logic_error("Core::addTask must be called from the owner thread");
    }

    auto taskInfoIt = m_taskHash.constFind(taskType);
    if (taskInfoIt == m_taskHash.cend()) {
        qWarning() << "Core::addTask - Task not registered for type:" << taskType;
        throw std::logic_error("Task not registered");
    }

    try {
        const auto& taskInfo = taskInfoIt.value();
        auto storedFuncAny = taskInfo.m_function;
        auto taskFunction = std::any_cast<std::function<QVariant(Args...)>>(storedFuncAny);

        QList<QVariant> argsList;
        if constexpr (all_convertible_to<QVariant>::check<Args...>()) {
            argsList = { QVariant::fromValue(args)... };
        } else {
            qWarning() << "Core::addTask - Arguments are not convertible to QList<QVariant> for task type:" << taskType;
        }

        auto taskFunctionBound = std::bind(taskFunction, args...);
        auto pTask = QSharedPointer<Task>(new Task(
            std::move(taskFunctionBound),
            taskType,
            taskInfo.m_group,
            std::move(argsList)
        ));

        bool start = std::none_of(m_activeTaskList.begin(), m_activeTaskList.end(), [group = pTask->m_group](const auto& pActiveTask) {
            return pActiveTask->m_group == group;
        });

        if (start && !m_blockStartTask.load()) {
            startTask(pTask);
        } else {
            m_queuedTaskList.append(std::move(pTask));
        }
    } catch (const std::bad_any_cast& e) {
        qWarning() << "Core::addTask - Bad arguments or function signature mismatch for task type:" << taskType << e.what();
        throw std::logic_error("Bad arguments or function signature mismatch");
    }
}

[[nodiscard]] inline std::atomic_bool* Core::stopTaskFlag() {
    return core_detail::g_currentStopFlag;
}

inline void Core::terminateTaskById(TaskId id) {
    if (!ensureCalledFromOwnerThread("terminateTaskById")) {
        return;
    }

    if (auto pTask = activeTaskById(id); !pTask.isNull()) {
        terminateTask(std::move(pTask));
    }
}

inline void Core::cancelTaskById(TaskId id) {
    if (!ensureCalledFromOwnerThread("cancelTaskById")) {
        return;
    }
    stopTaskById(id);
}

inline void Core::cancelTaskByType(TaskType type) {
    if (!ensureCalledFromOwnerThread("cancelTaskByType")) {
        return;
    }
    stopTaskByType(type);
}

inline void Core::cancelTaskByGroup(TaskGroup group) {
    if (!ensureCalledFromOwnerThread("cancelTaskByGroup")) {
        return;
    }
    stopTaskByGroup(group);
}

inline void Core::cancelTasks() {
    if (!ensureCalledFromOwnerThread("cancelTasks")) {
        return;
    }
    stopTasks();
}

inline void Core::cancelAllTasks() {
    if (!ensureCalledFromOwnerThread("cancelAllTasks")) {
        return;
    }
    stopAllTasks();
}

inline void Core::cancelTasksByGroup(TaskGroup group, bool includeQueued) {
    if (!ensureCalledFromOwnerThread("cancelTasksByGroup")) {
        return;
    }
    stopTasksByGroup(group, includeQueued);
}

inline void Core::stopTaskById(TaskId id) {
    if (!ensureCalledFromOwnerThread("stopTaskById")) {
        return;
    }

    if (auto pTask = activeTaskById(id); !pTask.isNull()) {
        stopTask(std::move(pTask));
    }
}

inline void Core::stopTaskByType(TaskType type) {
    if (!ensureCalledFromOwnerThread("stopTaskByType")) {
        return;
    }

    if (auto pTask = activeTaskByType(type); !pTask.isNull()) {
        stopTask(std::move(pTask));
    }
}

inline void Core::stopTaskByGroup(TaskGroup group) {
    if (!ensureCalledFromOwnerThread("stopTaskByGroup")) {
        return;
    }

    stopTasksByGroup(group, false);
}

inline void Core::stopTasks() {
    if (!ensureCalledFromOwnerThread("stopTasks")) {
        return;
    }

    if (m_activeTaskList.isEmpty()) {
        return;
    }

    m_blockStartTask.store(true);
    QTimer* pTimer = new QTimer(this); // with parent for automatic cleanup!
    connect(pTimer, &QTimer::timeout, this, [this, pTimer]() {
        if (isIdle()) { // Use the public method
            m_blockStartTask.store(false);
            startQueuedTask();
            pTimer->stop();
            pTimer->deleteLater();
        }
    });

    // Calculating the maximum stop timeout among active tasks
    TaskStopTimeout maxTimeout = 0;
    for (const auto& pTask : std::as_const(m_activeTaskList)) {
        auto taskInfoIt = m_taskHash.constFind(pTask->m_type);
        if (taskInfoIt != m_taskHash.cend()) {
            maxTimeout = std::max(maxTimeout, taskInfoIt.value().m_stopTimeout);
        } else {
            maxTimeout = std::max(maxTimeout, static_cast<TaskStopTimeout>(kDefaultStopTimeout));
            qWarning() << "Core::stopTasks - Missing registration for active task type:" << pTask->m_type;
        }
    }

    // Requesting to stop all active tasks
    for (const auto& pTask : std::as_const(m_activeTaskList)) {
        stopTask(pTask);
    }

    // Starting the timer with the maximum timeout
    pTimer->start(maxTimeout);
}

inline void Core::stopAllTasks() {
    if (!ensureCalledFromOwnerThread("stopAllTasks")) {
        return;
    }

    // Remove queued tasks immediately (they never started, so no stop timeout needed).
    for (const auto& pQueuedTask : std::as_const(m_queuedTaskList)) {
        emit terminatedTask(pQueuedTask->m_id, pQueuedTask->m_type, pQueuedTask->m_argsList);
    }
    m_queuedTaskList.clear();

    // Then request stop for all currently active tasks.
    stopTasks();
}

inline void Core::stopTasksByGroup(TaskGroup group, bool includeQueued) {
    if (!ensureCalledFromOwnerThread("stopTasksByGroup")) {
        return;
    }

    QList<QSharedPointer<Task>> activeInGroup;
    for (const auto& pTask : std::as_const(m_activeTaskList)) {
        if (pTask->m_group == group) {
            activeInGroup.append(pTask);
        }
    }

    for (const auto& pTask : std::as_const(activeInGroup)) {
        stopTask(pTask);
    }

    if (!includeQueued) {
        return;
    }

    for (auto it = m_queuedTaskList.begin(); it != m_queuedTaskList.end();) {
        const auto& pQueuedTask = *it;
        if (pQueuedTask->m_group == group) {
            emit terminatedTask(pQueuedTask->m_id, pQueuedTask->m_type, pQueuedTask->m_argsList);
            it = m_queuedTaskList.erase(it);
        } else {
            ++it;
        }
    }
}

[[nodiscard]] inline bool Core::isTaskRegistered(TaskType type) {
    if (!ensureCalledFromOwnerThread("isTaskRegistered")) {
        return false;
    }

    return m_taskHash.contains(type);
}

inline TaskGroup Core::groupByTask(TaskType type, bool* ok) {
    if (!ensureCalledFromOwnerThread("groupByTask")) {
        if (ok) *ok = false;
        return -1;
    }

    auto taskInfoIt = m_taskHash.constFind(type);
    if (taskInfoIt != m_taskHash.cend()) {
        if (ok) *ok = true;
        return taskInfoIt.value().m_group;
    } else {
        if (ok) *ok = false;
        return -1;
    }
}

[[nodiscard]] inline bool Core::isIdle() {
    if (!ensureCalledFromOwnerThread("isIdle")) {
        return false;
    }

    return m_activeTaskList.isEmpty();
}

[[nodiscard]] inline bool Core::isTaskAddedByType(TaskType type, bool* isActive) {
    if (!ensureCalledFromOwnerThread("isTaskAddedByType")) {
        if (isActive) *isActive = false;
        return false;
    }

    for (const auto& pTask : std::as_const(m_activeTaskList)) {
        if (pTask->m_type == type) {
            if (isActive) *isActive = true;
            return true;
        }
    }
    for (const auto& pTask : std::as_const(m_queuedTaskList)) {
        if (pTask->m_type == type) {
            if (isActive) *isActive = false;
            return true;
        }
    }
    return false;
}

[[nodiscard]] inline bool Core::isTaskAddedByGroup(TaskGroup group, bool* isActive) {
    if (!ensureCalledFromOwnerThread("isTaskAddedByGroup")) {
        if (isActive) *isActive = false;
        return false;
    }

    for (const auto& pTask : std::as_const(m_activeTaskList)) {
        if (pTask->m_group == group) {
            if (isActive) *isActive = true;
            return true;
        }
    }
    for (const auto& pTask : std::as_const(m_queuedTaskList)) {
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
                           [id](const auto& pTask) { return pTask->m_id == id; });
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
    // Set stop flag to request cooperative cancellation
    pTask->m_stopFlag.store(true);

    if (pTask->m_state == TaskState::Active) {
        pTask->m_state = TaskState::StopRequested;
        emit stopRequestedTask(pTask->m_id, pTask->m_type, pTask->m_argsList);
    }

    // Determine timeout from task's registered stop timeout (verification window).
    TaskStopTimeout timeout = kDefaultStopTimeout;
    auto taskInfoIt = m_taskHash.constFind(pTask->m_type);
    if (taskInfoIt != m_taskHash.cend()) {
        timeout = taskInfoIt.value().m_stopTimeout;
    }

    // IMPORTANT: do not block the UI thread here.
    // Request termination first, then confirm termination asynchronously.
    bool terminationRequested = false;

#ifdef Q_OS_WIN
    if (pTask->m_threadHandle) {
        if (WaitForSingleObject(pTask->m_threadHandle, 0) != WAIT_OBJECT_0) {
            terminationRequested = (TerminateThread(pTask->m_threadHandle, 1) != 0);
        } else {
            return; // already exited; finished path will handle cleanup
        }
    } else {
        return; // no valid handle
    }
#else
    if (pTask->m_threadHandle != 0) {
        if (pthread_kill(pTask->m_threadHandle, 0) == 0) {
            terminationRequested = (pthread_cancel(pTask->m_threadHandle) == 0);
        } else {
            return; // already not alive; finished path will handle cleanup
        }
    } else {
        return; // no valid handle
    }
#endif

    if (!terminationRequested) {
        pTask->m_state = TaskState::StopTimedOut;
        qWarning() << QString("Task %1 terminate request was rejected by platform API").arg(QString::number(pTask->m_id));
        emit stopTimedOutTask(pTask->m_id, pTask->m_type, pTask->m_argsList, timeout);
        return;
    }

    auto started = QSharedPointer<QElapsedTimer>::create();
    started->start();
    auto checker = QSharedPointer<std::function<void()>>::create();

    *checker = [this, pTask, timeout, started, checker]() {
        if (pTask->m_state == TaskState::Inactive
            || pTask->m_state == TaskState::Finished
            || pTask->m_state == TaskState::Terminated) {
            return; // already finished/terminated by another path
        }

        bool isAlive = false;
#ifdef Q_OS_WIN
        if (pTask->m_threadHandle) {
            const DWORD waitResult = WaitForSingleObject(pTask->m_threadHandle, 0);
            isAlive = (waitResult == WAIT_TIMEOUT);
            if (!isAlive) {
                CloseHandle(pTask->m_threadHandle);
                pTask->m_threadHandle = nullptr;
            }
        }
#else
        if (pTask->m_threadHandle != 0) {
            isAlive = (pthread_kill(pTask->m_threadHandle, 0) == 0);
        }
#endif

        if (!isAlive) {
            pTask->m_state = TaskState::Terminated;
            emit terminatedTask(pTask->m_id, pTask->m_type, pTask->m_argsList);
            m_activeTaskList.removeAll(pTask);
            startQueuedTask();
            return;
        }

        if (started->elapsed() >= timeout) {
            pTask->m_state = TaskState::StopTimedOut;
            qWarning() << QString("Task %1 did not stop after terminate request within timeout (%2 ms)")
                              .arg(QString::number(pTask->m_id)).arg(timeout);
            emit stopTimedOutTask(pTask->m_id, pTask->m_type, pTask->m_argsList, timeout);
            return;
        }

        QTimer::singleShot(20, this, [checker]() {
            (*checker)();
        });
    };

    QTimer::singleShot(0, this, [checker]() {
        (*checker)();
    });
}

inline void Core::stopTask(QSharedPointer<Core::Task> pTask) {
    pTask->m_stopFlag.store(true);
    if (pTask->m_state == TaskState::Active) {
        pTask->m_state = TaskState::StopRequested;
        emit stopRequestedTask(pTask->m_id, pTask->m_type, pTask->m_argsList);
    }

    TaskStopTimeout timeout = kDefaultStopTimeout;
    auto taskInfoIt = m_taskHash.constFind(pTask->m_type);
    if (taskInfoIt != m_taskHash.cend()) {
        timeout = taskInfoIt.value().m_stopTimeout;
    } else {
        qWarning() << "Core::stopTask - Missing registration for active task type:" << pTask->m_type;
    }

    QTimer::singleShot(timeout, this, [this, pTask]() {
        switch (pTask->m_state) {
        case TaskState::Finished:
            qDebug() << QString("Task %1 was successfully stopped").arg(QString::number(pTask->m_id));
            break;
        case TaskState::Terminated:
            qDebug() << QString("Task %1 was terminated").arg(QString::number(pTask->m_id));
            break;
        case TaskState::StopTimedOut:
            qDebug() << QString("Task %1 stop already timed out").arg(QString::number(pTask->m_id));
            break;
        case TaskState::StopRequested:
        case TaskState::Active:
            qDebug() << QString("Task %1 was not stopped, terminating").arg(QString::number(pTask->m_id));
            terminateTask(pTask);
            if (pTask->m_state == TaskState::Active || pTask->m_state == TaskState::StopRequested) {
                qDebug() << QString("Task %1 terminate request is in progress")
                                .arg(QString::number(pTask->m_id));
            }
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
    TaskHelper* pTaskHelper = new TaskHelper(pTask->m_functionBound, &pTask->m_stopFlag, this); // Add with parent!

    connect(pTaskHelper, &TaskHelper::finished, this, [this, pTask, pTaskHelper](QVariant result) {
        pTask->m_state = TaskState::Finished;
        emit finishedTask(pTask->m_id, pTask->m_type, pTask->m_argsList, result);
        m_activeTaskList.removeAll(pTask);
        startQueuedTask();
        pTaskHelper->deleteLater();
    });

#ifdef Q_OS_WIN
    pTask->m_threadHandle = CreateThread(nullptr, 0, &TaskHelper::functionWrapper, pTaskHelper, 0, &pTask->m_threadId);
    if (pTask->m_threadHandle == NULL) {
        qWarning() << "Core::startTask - Failed to create thread for task ID:" << pTask->m_id << ". GetLastError:" << GetLastError();
        m_activeTaskList.removeAll(pTask);
        // emit taskCreationFailed(...);
        startQueuedTask();
        pTaskHelper->deleteLater();
        return; // Abort StartTask execution
    }
    // If everything is OK, continue...
#else
    // Checking pthread_create
    int result = pthread_create(&pTask->m_threadHandle, nullptr, &TaskHelper::functionWrapper, pTaskHelper);
    if (result != 0) {
        qWarning() << "Core::startTask - Failed to create thread for task ID:" << pTask->m_id << ". Error code:" << result;
        m_activeTaskList.removeAll(pTask);
        // emit taskCreationFailed(...);
        startQueuedTask();
        pTaskHelper->deleteLater();
        return; // Abort StartTask execution
    }
    // If everything is OK, continue...
    pthread_detach(pTask->m_threadHandle);
#endif

    emit startedTask(pTask->m_id, pTask->m_type, pTask->m_argsList);
}

inline void Core::startQueuedTask() {
    if (m_blockStartTask.load()) {
        return;
    }

    for (auto it = m_queuedTaskList.begin(); it != m_queuedTaskList.end();) {
        QSharedPointer<Task> pQueuedTask = *it;
        bool canStart = std::none_of(std::as_const(m_activeTaskList).begin(), std::as_const(m_activeTaskList).end(),
                                     [&pQueuedTask](const QSharedPointer<Task>& pActiveTask) {
            return pActiveTask->m_group == pQueuedTask->m_group;
        });
        if (canStart) {
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

    TaskStopTimeout normalizedStopTimeout = taskStopTimeout;
    if (normalizedStopTimeout < 0) {
        qWarning() << "Core::registerTask - Negative stop timeout for task type:"
                   << taskType << ". Using default:" << kDefaultStopTimeout;
        normalizedStopTimeout = kDefaultStopTimeout;
    }

    m_taskHash.insert(taskType, TaskInfo{taskFunction, taskGroup, normalizedStopTimeout});
}

#endif // CORE_H

