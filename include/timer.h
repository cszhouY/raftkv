#ifndef _TIMER_H_
#define _TIMER_H_

#include <chrono>
#include <functional>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <atomic>

class Timer {
public:
    Timer() : m_expired(true), m_clock(0) {}

    Timer(const Timer& timer) {
        m_expired = timer.m_expired.load();
    }

    ~Timer() {
        stop();
    }

    void start(int interval, std::function<void()> func) {
        ++m_clock;
        m_expired.store(false);
        std::thread([this, interval, func]() {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(interval), [this] { return m_expired.load(); });
            if (!m_expired.load()) {
                func();
                // m_expired.store(true);
            }
        }).detach();
    }

    void local_start(int interval, std::function<void()> func) {
        ++m_clock;
        m_expired.store(false);
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait_for(lock, std::chrono::milliseconds(interval), [this] { return m_expired.load(); });
        if (!m_expired.load()) {
            func();
            m_expired.store(true);
        }
    }

    void loop_start(int interval, std::function<void()> func) {
        ++m_clock;
        m_expired.store(false);
        std::thread([this, interval, func]() {
            while (!m_expired.load()) {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait_for(lock, std::chrono::milliseconds(interval), [this] { return m_expired.load(); });
                if (!m_expired.load()) {
                    func();
                }
            }
        }).detach();
    }

    void stop() {
        m_expired.store(true);
        m_cv.notify_one();
    }

    void reset() {
        m_expired.store(false);
        m_cv.notify_one();
    }

    bool isExpired() const {
        return m_expired.load();
    }

    int clock() const {
        return m_clock;
    }

private:
    std::atomic<bool> m_expired;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    int m_clock;
};

#endif  // _TIMER_H_