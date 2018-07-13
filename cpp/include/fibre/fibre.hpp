#ifndef __FIBRE_HPP
#define __FIBRE_HPP

// Note that this option cannot be used to debug UART because it prints on UART
#define DEBUG_FIBRE
#ifdef DEBUG_FIBRE
#define LOG_FIBRE(...)  do { printf("%s %s(): ", __FILE__, __func__); printf(__VA_ARGS__); printf("\r\n"); } while (0)
#else
#define LOG_FIBRE(...)  ((void) 0)
#endif

/** @brief Allow Fibre to use C++ built-in threading facilities (std::thread, thread_local storage specifier) */
#define CONFIG_USE_STL_THREADING

/** @brief Allow Fibre to use C++ built-in std::chrono features */
#define CONFIG_USE_STL_CLOCK

/**
 * @brief Don't launch any scheduler thread.
 * 
 * The user application must call fibre::schedule_all() periodically, otherwise
 * Fibre will not emit any data. This option is intended for systems that don't
 * support threading.
 */
#define SCHEDULER_MODE_MANUAL 1

/**
 * @brief Launch one global scheduler thread that will handle all remote nodes.
 * This is recommended for embedded systems that don't support dynamic memory.
 */
#define SCHEDULER_MODE_GLOBAL_THREAD 2

/**
 * @brief Launch one scheduler thread per remote node.
 * This is recommended for desktop class systems.
 */
#define SCHEDULER_MODE_PER_NODE_THREAD 3

/**
 * @brief Specifies how the output data is scheduled
 */
#define CONFIG_SCHEDULER_MODE   SCHEDULER_MODE_GLOBAL_THREAD


#ifdef CONFIG_USE_STL_CLOCK

#include <chrono>

typedef std::chrono::time_point<std::chrono::steady_clock> monotonic_time_t;
__attribute__((unused))
static monotonic_time_t now() {
    return std::chrono::steady_clock::now();
}

__attribute__((unused))
static bool is_in_the_future(monotonic_time_t time_point) {
    return time_point > std::chrono::steady_clock::now();
}

#else
#error "Not implemented"
#endif

namespace fibre {
    struct global_state_t;
}

#include "uuid.hpp"
#include "protocol.hpp"


#ifdef CONFIG_USE_STL_THREADING
#include <thread>
#include <mutex>
#include <condition_variable>

namespace fibre {

template<bool AutoReset>
class EventWaitHandle {
public:
    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_variable_.wait(lock, [this]{
            bool was_set = is_set_;
            if (AutoReset)
                is_set_ = false;
            return was_set;
        });
    }

    void set() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            is_set_ = true;
        }
        condition_variable_.notify_all();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        is_set_ = false;
    }

    bool is_set() {
        std::lock_guard<std::mutex> lock(mutex_);
        return is_set_;
    }
private:
    std::mutex mutex_;
    std::condition_variable condition_variable_;
    bool is_set_ = false;
};

using ManualResetEvent = EventWaitHandle<false>;
using AutoResetEvent = EventWaitHandle<true>;

}
#endif


namespace fibre {
    struct global_state_t {
        bool initialized = false;
        Uuid own_uuid;
        std::unordered_map<Uuid, RemoteNode> remote_nodes_;
        std::vector<fibre::FibreRefType*> ref_types_ = std::vector<fibre::FibreRefType*>();
        std::vector<fibre::LocalEndpoint*> functions_ = std::vector<fibre::LocalEndpoint*>();
        std::thread scheduler_thread;
        AutoResetEvent output_pipe_ready;
        AutoResetEvent output_channel_ready;
    };

    extern global_state_t global_state;

#if CONFIG_SCHEDULER_MODE == SCHEDULER_MODE_MANUAL
    void schedule_all();
#endif
} // namespace fibre

#endif // __FIBRE_HPP
