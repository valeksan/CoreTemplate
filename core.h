#ifndef CORE_H
#define CORE_H

#include <QObject>
#include <QQueue>
#include <QVariant>
#include <QDebug>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <atomic>
#include <functional>
#include <any>

#define STOP_ACTIVE_TASK_DEFAULT_TIMEOUT 1000

template<bool...>
struct bool_pack {};

template<bool... b>
using all_true = std::is_same<bool_pack<true, b...>, bool_pack<b..., true> >;

template<bool... b>
constexpr bool all_true_v = all_true<b...>::value;

template<typename T>
struct all_convertible_to
{
    template<typename... Args>
    constexpr static bool check()
    {
        return all_true_v<std::is_convertible_v<Args, T>...>;
    }
};

template <>
struct all_convertible_to<QVariant>
{
    template<typename... Args>
    constexpr static bool check()
    {
        return all_true_v<(std::is_convertible_v<Args, QVariant> || QMetaTypeId<Args>::Defined)...>;
    }
};

template<int N>
struct placeholder {};

namespace std
{
    template<int N>
    struct is_placeholder<placeholder<N> > : public integral_constant<int, N> {};
}

template<typename R, typename...Args, typename Class, std::size_t ...N>
auto bind_placeholders(R(Class::*taskFunction)(Args...), Class *taskObj, std::index_sequence<N...>) { return std::bind(taskFunction, taskObj, placeholder<N+1>()...); }

template<typename R, typename...Args, typename Class, std::size_t ...N>
auto bind_placeholders(R(Class::*taskFunction)(Args...) const, Class *taskObj, std::index_sequence<N...>) { return std::bind(taskFunction, taskObj, placeholder<N+1>()...); }

template<typename T>
struct remove_class {};

template<typename C, typename R, typename... Args>
struct remove_class<R(C::*)(Args...)> { using type = R(Args...); };

template<typename C, typename R, typename... Args>
struct remove_class<R(C::*)(Args...) const> { using type = R(Args...); };

template<typename C, typename R, typename... Args>
struct remove_class<R(C::*)(Args...) volatile> { using type = R(Args...); };

template<typename C, typename R, typename... Args>
struct remove_class<R(C::*)(Args...) const volatile> { using type = R(Args...); };

template<typename T>
using remove_class_t = typename remove_class<T>::type;

template<typename T>
struct get_signature { using type = remove_class_t<decltype(&std::remove_reference_t<T>::operator())>; };

template<typename R, typename... Args>
struct get_signature<R(Args...)> { using type = R(Args...); };

template<typename R, typename... Args>
struct get_signature<R(&)(Args...)> { using type = R(Args...); };

template<typename R, typename... Args>
struct get_signature<R(*)(Args...)> { using type = R(Args...); };

template<typename T>
using get_signature_t = typename get_signature<T>::type;

template<typename F>
using make_function_t = std::function<get_signature_t<F> >;

template<typename F>
make_function_t<F> make_function(F &&f) { return make_function_t<F>(std::forward<F>(f)); }

class TaskHelper : public QObject
{
    Q_OBJECT
public:
    TaskHelper(std::function<QVariant()> function, QObject *parent = nullptr) : QObject(parent), m_function(function)
    {

    }

#ifdef Q_OS_WIN
    static DWORD WINAPI functionWrapper(void *pTaskHelper)
    {
        TaskHelper *pThisTaskHelper = qobject_cast<TaskHelper *>(reinterpret_cast<QObject *>(pTaskHelper));
        if(pThisTaskHelper) pThisTaskHelper->execute();
        return 0;
    }
#else
    static void *functionWrapper(void *pTaskHelper)
    {
        TaskHelper *pThisTaskHelper = qobject_cast<TaskHelper *>(reinterpret_cast<QObject *>(pTaskHelper));
        if(pThisTaskHelper) pThisTaskHelper->execute();
        return nullptr;
    }
#endif

private:
    void execute()
    {
        emit finished(m_function());
    }

    std::function<QVariant()> m_function;

signals:
    void finished(QVariant result);
};

class Core : public QObject
{
    Q_OBJECT
public:
    explicit Core(QObject *parent = nullptr) : QObject(parent), m_blockStartTask(false)
    {

    }

    template <typename R, typename...Args>
    void registerTask(int taskType, std::function<R(Args...)> taskFunction, int taskGroup = 0, int taskStopTimeout = 1000)
    {
        std::function<QVariant(std::remove_reference_t<Args>...)> f;

        if constexpr(std::is_convertible_v<R, QVariant>)
        {
            f = [taskFunction](std::remove_reference_t<Args>... args)
            {
                return taskFunction(args...);
            };
        }
        else if constexpr(std::is_same_v<R,void>)
        {
            f = [taskFunction](std::remove_reference_t<Args>... args)
            {
                taskFunction(args...);
                return QVariant();
            };
        }
        else if constexpr(QMetaTypeId<R>::Defined)
        {
            f = [taskFunction](std::remove_reference_t<Args>... args)
            {
                return QVariant::fromValue(taskFunction(args...));
            };
        }
        else
        {
            Q_UNUSED(taskType)
            Q_UNUSED(taskFunction)
            Q_UNUSED(taskGroup)
            Q_UNUSED(taskStopTimeout)

            qWarning() << "Core::registerTask - Not convertible return type";
            throw std::logic_error("Not convertible return type");
        }

        insertToTaskHash(taskType, f, taskGroup, taskStopTimeout);
    }

    template <typename F>
    void registerTask(int taskType, F &&taskFunction, int taskGroup = 0, int taskStopTimeout = 1000)
    {
        registerTask(taskType, make_function(taskFunction), taskGroup, taskStopTimeout);
    }

    template <typename R, typename...Args>
    void registerTask(int taskType, R(*taskFunction)(Args...), int taskGroup = 0, int taskStopTimeout = 1000)
    {
        registerTask(taskType, std::function<R(Args...)>(taskFunction), taskGroup, taskStopTimeout);
    }

    template <typename Class, typename R, typename...Args>
    void registerTask(int taskType, R(Class::*taskMethod)(Args...), Class *taskObj, int taskGroup = 0, int taskStopTimeout = 1000)
    {
        std::function<R(Args...)> f = bind_placeholders(taskMethod, taskObj, std::make_index_sequence<sizeof...(Args)>());
        registerTask(taskType, f, taskGroup, taskStopTimeout);
    }

    template <typename Class, typename R, typename...Args>
    void registerTask(int taskType, R(Class::*taskMethod)(Args...) const, Class *taskObj, int taskGroup = 0, int taskStopTimeout = 1000)
    {
        std::function<R(Args...)> f = bind_placeholders(taskMethod, taskObj, std::make_index_sequence<sizeof...(Args)>());
        registerTask(taskType, f, taskGroup, taskStopTimeout);
    }

    bool unregisterTask(int taskType)
    {
        if(m_taskHash.remove(taskType)) return true;
        else return false;
    }

    template <typename...Args>
    void addTask(int taskType, Args... args)
    {
        if(!m_taskHash.contains(taskType))
        {
            qWarning() << "Core::addTask - Task not registred";
            throw std::logic_error("Task not registred");
        }
        try
        {
            auto taskFunction = std::any_cast<std::function<QVariant(Args...)> >(m_taskHash[taskType].m_function);

            QVariantList argsList;
            if constexpr(all_convertible_to<QVariant>::check<Args...>())
            {
                argsList = { QVariant::fromValue(args)... };
            }
            else
            {
                qWarning() << "Core::addTask - Arguments are not convertible to QVariantList";
            }

            auto taskFunctionBound = std::bind(taskFunction, args...);

            QSharedPointer<Task> pTask = QSharedPointer<Task>::create(taskFunctionBound, taskType, m_taskHash[taskType].m_group, argsList);

            bool start = std::all_of(m_activeTaskList.begin(), m_activeTaskList.end(), [pTask](QSharedPointer<Task> pActiveTask)
            {
                if(pTask->m_group == pActiveTask->m_group) return false;
                else return true;
            });

            if(start && !m_blockStartTask)
            {
                startTask(pTask);
            }
            else m_queuedTaskList.append(pTask);
        }
        catch(std::bad_any_cast &)
        {
            qWarning() << "Core::addTask - Bad arguments";
            throw std::logic_error("Bad arguments");
        }
    }

    std::atomic_bool *stopTaskFlag()
    {
        foreach(auto task, m_activeTaskList)
        {
#ifdef Q_OS_WIN
            if(task->m_threadId == GetCurrentThreadId()) return &task->m_stopFlag;
#else
            if(pthread_equal(pthread_self(), task->m_threadHandle)) return &task->m_stopFlag;
#endif
        }
        return nullptr;
    }

    void terminateTaskById(long id)
    {
        auto pTask = activeTaskById(id);

        if(pTask.isNull()) return;

        terminateTask(pTask);
    }

    void stopTaskById(long id)
    {
        auto pTask = activeTaskById(id);

        if(pTask.isNull()) return;

        stopTask(pTask);
    }

    void stopTaskByType(int type)
    {
        auto pTask = activeTaskByType(type);

        if(pTask.isNull()) return;

        stopTask(pTask);
    }

    void stopTaskByGroup(int group)
    {
        auto pTask = activeTaskByGroup(group);

        if(pTask.isNull()) return;

        stopTask(pTask);
    }

    void stopTasks()
    {
        m_blockStartTask = true;
        QTimer *pTimer = new QTimer();
        connect(pTimer, &QTimer::timeout, this, [=]()
        {
            if(m_activeTaskList.isEmpty())
            {
                m_blockStartTask = false;
                pTimer->stop();
                pTimer->deleteLater();
            }
        });

        int stopTasksTimeout = 0;
        foreach(auto pTask, m_activeTaskList)
        {
            int stopTaskTimeout = m_taskHash[pTask->m_type].m_stopTimeout;
            if(stopTaskTimeout > stopTasksTimeout) stopTasksTimeout = stopTaskTimeout;
            stopTask(pTask);
        }

        pTimer->start(stopTasksTimeout);
    }

    bool isTaskRegistred(int type)
    {
        if(m_taskHash.contains(type)) return true;
        else return false;
    }

    int groupByTask(int type, bool *ok = nullptr)
    {
        if(isTaskRegistred(type))
        {
            if(ok) *ok = true;
            return m_taskHash[type].m_group;
        }
        else
        {
            if(ok) *ok = false;
            return -1;
        }
    }

    bool isIdle()
    {
        return m_activeTaskList.isEmpty();
    }

    bool isTaskAddedByType(int type, bool *isActive = nullptr)
    {
        foreach(auto pTask, m_activeTaskList)
        {
            if(pTask->m_type == type)
            {
                if(isActive) *isActive = true;
                return true;
            }
        }

        foreach(auto pTask, m_queuedTaskList)
        {
            if(pTask->m_type == type)
            {
                if(isActive) *isActive = false;
                return true;
            }
        }

        return false;
    }

    bool isTaskAddedByGroup(int group, bool *isActive = nullptr)
    {
        foreach(auto pTask, m_activeTaskList)
        {
            if(pTask->m_group == group)
            {
                if(isActive) *isActive = true;
                return true;
            }
        }

        foreach(auto pTask, m_queuedTaskList)
        {
            if(pTask->m_group == group)
            {
                if(isActive) *isActive = false;
                return true;
            }
        }

        return false;
    }

private:
    enum TaskState
    {
        INACTIVE,
        ACTIVE,
        FINISHED,
        TERMINATED
    };

    struct TaskInfo
    {
        std::any m_function;
        int m_group;
        int m_stopTimeout;
    };

    struct Task
    {
        Task(std::function<QVariant()> functionBound, int type, int group, QVariantList argsList = QVariantList())
        {
            static long id = 0;
            m_id = id++;
            m_functionBound = functionBound;
            m_type = type;
            m_group = group;
            m_argsList = argsList;
            m_stopFlag.store(false);
            m_state = INACTIVE;
        }

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

    QSharedPointer<Task> activeTaskById(long id)
    {
        foreach(auto pTask, m_activeTaskList)
        {
            if(pTask->m_id == id) return pTask;
        }

        return nullptr;
    }

    QSharedPointer<Task> activeTaskByType(int type)
    {
        foreach(auto pTask, m_activeTaskList)
        {
            if(pTask->m_type == type) return pTask;
        }

        return nullptr;
    }

    QSharedPointer<Task> activeTaskByGroup(int group)
    {
        foreach(auto pTask, m_activeTaskList)
        {
            if(pTask->m_group == group) return pTask;
        }

        return nullptr;
    }

    void terminateTask(QSharedPointer<Task> pTask)
    {
#ifdef Q_OS_WIN
        TerminateThread(pTask->m_threadHandle, 0);
#else
        pthread_cancel(pTask->m_threadHandle);
#endif
        pTask->m_state = TERMINATED;

        emit terminatedTask(pTask->m_id, pTask->m_type, pTask->m_argsList);

        m_activeTaskList.removeOne(pTask);

        startQueuedTask();
    }

    void stopTask(QSharedPointer<Task> pTask)
    {
        pTask->m_stopFlag.store(true);
        QTimer::singleShot(m_taskHash[pTask->m_type].m_stopTimeout, this, [=]()
        {
            switch(pTask->m_state)
            {
            case FINISHED:
                qDebug() << QString("Task %1 was sucessfully stopped").arg(QString::number(pTask->m_id));
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

    void startTask(QSharedPointer<Task> pTask)
    {
        m_activeTaskList.append(pTask);

        pTask->m_state = ACTIVE;

        TaskHelper *pTaskHelper = new TaskHelper(pTask->m_functionBound);

        connect(pTaskHelper, &TaskHelper::finished, this, [=](QVariant result)
        {
            pTask->m_state = FINISHED;

            emit finishedTask(pTask->m_id, pTask->m_type, pTask->m_argsList, result);

            m_activeTaskList.removeOne(pTask);

            startQueuedTask();

            pTaskHelper->deleteLater();
        });

#ifdef Q_OS_WIN
        pTask->m_threadHandle = CreateThread(nullptr, 0, &TaskHelper::functionWrapper, reinterpret_cast<void *>(pTaskHelper), 0, &pTask->m_threadId);
#else
        pthread_create(&pTask->m_threadHandle, nullptr, &TaskHelper::functionWrapper, pTaskHelper);
        pthread_detach(pTask->m_threadHandle);
#endif

        emit startedTask(pTask->m_id, pTask->m_type, pTask->m_argsList);
    }

    void startQueuedTask()
    {
        foreach(auto pQueuedTask, m_queuedTaskList)
        {
            bool start = std::all_of(m_activeTaskList.begin(), m_activeTaskList.end(), [pQueuedTask](QSharedPointer<Task> pActiveTask)
            {
                if(pQueuedTask->m_group == pActiveTask->m_group) return false;
                else return true;
            });

            if(start)
            {
                m_queuedTaskList.removeOne(pQueuedTask);
                startTask(pQueuedTask);
            }
        }
    }

    template <typename...Args>
    void insertToTaskHash(int taskType, std::function<QVariant(Args...)> taskFunction, int taskGroup = 0, int taskStopTimeout = 1000)
    {
        if(m_taskHash.contains(taskType))
        {
            qWarning() << "Core::registerTask - Task type is already registred";
            throw std::logic_error("Task type is already registred");
        }

        m_taskHash.insert(taskType, TaskInfo{taskFunction, taskGroup, taskStopTimeout});
    }

    QHash<int, TaskInfo> m_taskHash;

    QList<QSharedPointer<Task> > m_activeTaskList;
    QList<QSharedPointer<Task> > m_queuedTaskList;

    bool m_blockStartTask;

signals:
    void finishedTask(long id, int type, QVariantList argsList = QVariantList(), QVariant result = QVariant());
    void startedTask(long id, int type, QVariantList argsList = QVariantList());
    void terminatedTask(long id, int type, QVariantList argsList = QVariantList());

public slots:

};

#endif // CORE_H
