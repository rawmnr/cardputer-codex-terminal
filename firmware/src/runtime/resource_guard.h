#pragma once

#include <Arduino.h>

#include <atomic>

#if !defined(ARDUINO) || defined(NATIVE_BUILD)
#include <chrono>
#include <mutex>
#endif

class ResourceGuard {
 public:
  enum class Kind : uint8_t {
    Spi = 0,
    I2c = 1,
  };

  struct Stats {
    uint32_t spi_timeouts = 0;
    uint32_t i2c_timeouts = 0;
    uint32_t spi_contention = 0;
    uint32_t i2c_contention = 0;
  };

  class ScopedLock {
   public:
    ScopedLock() = default;
    ScopedLock(Kind kind, uint32_t timeout_ms) { acquire(kind, timeout_ms); }

    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;

    ScopedLock(ScopedLock&& other) noexcept { moveFrom(other); }
    ScopedLock& operator=(ScopedLock&& other) noexcept {
      if (this != &other) {
        release();
        moveFrom(other);
      }
      return *this;
    }

    ~ScopedLock() { release(); }

    bool acquired() const { return acquired_; }
    explicit operator bool() const { return acquired(); }

   private:
    friend class ResourceGuard;

    void moveFrom(ScopedLock& other) {
      kind_ = other.kind_;
      acquired_ = other.acquired_;
#if !defined(ARDUINO) || defined(NATIVE_BUILD)
      mutex_ = other.mutex_;
#endif
      other.acquired_ = false;
#if !defined(ARDUINO) || defined(NATIVE_BUILD)
      other.mutex_ = nullptr;
#endif
    }

    void acquire(Kind kind, uint32_t timeout_ms) {
      kind_ = kind;
#if defined(ARDUINO) && !defined(NATIVE_BUILD)
      if (!ResourceGuard::ensureMutexes()) {
        return;
      }

      SemaphoreHandle_t* mutex = ResourceGuard::mutexFor(kind);
      if (mutex == nullptr || *mutex == nullptr) {
        return;
      }

      const TickType_t timeout_ticks = timeout_ms == portMAX_DELAY ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
      acquired_ = xSemaphoreTake(*mutex, timeout_ticks) == pdTRUE;
      if (!acquired_) {
        ResourceGuard::recordContention(kind);
      }
#else
      std::timed_mutex* mutex = &ResourceGuard::mutexFor(kind);
      mutex_ = mutex;
      acquired_ = mutex_->try_lock_for(std::chrono::milliseconds(timeout_ms));
      if (!acquired_) {
        ResourceGuard::recordContention(kind);
      }
#endif
    }

    void release() {
      if (!acquired_) {
        return;
      }

#if defined(ARDUINO) && !defined(NATIVE_BUILD)
      SemaphoreHandle_t* mutex = ResourceGuard::mutexFor(kind_);
      if (mutex != nullptr && *mutex != nullptr) {
        xSemaphoreGive(*mutex);
      }
#else
      if (mutex_ != nullptr) {
        mutex_->unlock();
      }
      mutex_ = nullptr;
#endif
      acquired_ = false;
    }

    Kind kind_ = Kind::Spi;
    bool acquired_ = false;
#if !defined(ARDUINO) || defined(NATIVE_BUILD)
    std::timed_mutex* mutex_ = nullptr;
#endif
  };

  static void begin() {
#if defined(ARDUINO) && !defined(NATIVE_BUILD)
    ensureMutexes();
#endif
    resetStats();
  }

  static ScopedLock lock(Kind kind, uint32_t timeout_ms = 5) { return ScopedLock(kind, timeout_ms); }

  static Stats stats() {
    Stats snapshot;
    snapshot.spi_timeouts = spi_timeouts_.load();
    snapshot.i2c_timeouts = i2c_timeouts_.load();
    snapshot.spi_contention = spi_contention_.load();
    snapshot.i2c_contention = i2c_contention_.load();
    return snapshot;
  }

  static void resetStats() {
    spi_timeouts_ = 0;
    i2c_timeouts_ = 0;
    spi_contention_ = 0;
    i2c_contention_ = 0;
  }

 private:
#if defined(ARDUINO) && !defined(NATIVE_BUILD)
  static bool ensureMutexes() {
    if (spi_mutex_ == nullptr) {
      spi_mutex_ = xSemaphoreCreateMutex();
    }
    if (i2c_mutex_ == nullptr) {
      i2c_mutex_ = xSemaphoreCreateMutex();
    }
    return spi_mutex_ != nullptr && i2c_mutex_ != nullptr;
  }

  static SemaphoreHandle_t* mutexFor(Kind kind) {
    return kind == Kind::Spi ? &spi_mutex_ : &i2c_mutex_;
  }
#else
  static std::timed_mutex& mutexFor(Kind kind) {
    return kind == Kind::Spi ? spi_mutex_ : i2c_mutex_;
  }
#endif

  static void recordContention(Kind kind) {
    if (kind == Kind::Spi) {
      ++spi_timeouts_;
      ++spi_contention_;
    } else {
      ++i2c_timeouts_;
      ++i2c_contention_;
    }
  }

  inline static std::atomic<uint32_t> spi_timeouts_{0};
  inline static std::atomic<uint32_t> i2c_timeouts_{0};
  inline static std::atomic<uint32_t> spi_contention_{0};
  inline static std::atomic<uint32_t> i2c_contention_{0};

#if defined(ARDUINO) && !defined(NATIVE_BUILD)
  inline static SemaphoreHandle_t spi_mutex_ = nullptr;
  inline static SemaphoreHandle_t i2c_mutex_ = nullptr;
#else
  inline static std::timed_mutex spi_mutex_;
  inline static std::timed_mutex i2c_mutex_;
#endif
};