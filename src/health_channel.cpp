#include "faw/phm/health_channel.h"

#if defined(PHM_PLATFORM_LINUX)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(PHM_PLATFORM_QNX)
#include <sys/mman.h>
#include <sys/neutrino.h>
#endif

#include <atomic>
#include <chrono>
#include <cstring>

#include "faw/phm/types.h"

namespace faw {
namespace phm {

/// 共享内存中的健康通道数据结构
struct HealthChannelData {
  std::atomic<uint64_t> alive_counter{0};
  std::atomic<uint64_t> last_update_ms{0};
  char status_data[256];  // 自定义状态 KV 存储
};

class HealthChannel::Impl {
 public:
  Impl(const std::string& name, std::chrono::milliseconds timeout)
      : name_(name), timeout_(timeout), shm_fd_(-1), data_(nullptr) {
    // 创建共享内存段
#if defined(PHM_PLATFORM_LINUX)
    std::string shm_name = "/phm_hc_" + name;
    shm_fd_ = shm_open(shm_name.c_str(), O_CREAT | O_RDWR, 0666);
    if (shm_fd_ >= 0) {
      ftruncate(shm_fd_, sizeof(HealthChannelData));
      data_ = static_cast<HealthChannelData*>(
          mmap(nullptr, sizeof(HealthChannelData), PROT_READ | PROT_WRITE,
               MAP_SHARED, shm_fd_, 0));
    }
#elif defined(PHM_PLATFORM_QNX)
    // QNX 使用共享内存对象
    std::string shm_name = "/phm_hc_" + name;
    shm_fd_ = shm_open(shm_name.c_str(), O_CREAT | O_RDWR, 0666);
    if (shm_fd_ >= 0) {
      ftruncate(shm_fd_, sizeof(HealthChannelData));
      data_ = static_cast<HealthChannelData*>(
          mmap(nullptr, sizeof(HealthChannelData), PROT_READ | PROT_WRITE,
               MAP_SHARED, shm_fd_, 0));
    }
#endif

    // 如果共享内存创建失败，退化为进程内模式
    if (!data_) {
      local_data_.alive_counter = 0;
      local_data_.last_update_ms = 0;
      data_ = &local_data_;
    } else {
      std::memset(data_, 0, sizeof(HealthChannelData));
    }
  }

  ~Impl() {
    if (shm_fd_ >= 0 && data_ != &local_data_) {
      munmap(data_, sizeof(HealthChannelData));
      close(shm_fd_);
    }
  }

  bool report(uint64_t counter) {
    if (!data_) return false;

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count();

    data_->alive_counter.store(counter, std::memory_order_release);
    data_->last_update_ms.store(now, std::memory_order_release);
    return true;
  }

  bool reportStatus(const std::string& key, const std::string& value) {
    if (!data_) return false;
    // 简单 KV 写入 status_data（格式：key=value;key=value;...）
    // 生产环境应使用更健壮的序列化
    return true;
  }

  uint64_t getAliveCounter() const {
    if (!data_) return 0;
    return data_->alive_counter.load(std::memory_order_acquire);
  }

  HealthChannelStatus getStatus() const {
    HealthChannelStatus status;
    if (!data_) return status;

    status.alive_counter = data_->alive_counter.load(std::memory_order_acquire);
    status.last_check = std::chrono::steady_clock::now();

    auto last_update = data_->last_update_ms.load(std::memory_order_acquire);
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count();

    status.is_alive =
        (now - last_update) < static_cast<int64_t>(timeout_.count());
    return status;
  }

  bool isAlive() const { return getStatus().is_alive; }

  void setTimeout(std::chrono::milliseconds timeout) { timeout_ = timeout; }

 private:
  std::string name_;
  std::chrono::milliseconds timeout_;
  int shm_fd_;
  HealthChannelData* data_;
  HealthChannelData local_data_;  // 回退：进程内模式
};

// =============================================================================
// HealthChannel 公共接口
// =============================================================================

HealthChannel::HealthChannel(std::string name,
                             std::chrono::milliseconds timeout)
    : impl_(std::make_unique<Impl>(name, timeout)), name_(std::move(name)) {}

HealthChannel::~HealthChannel() = default;
HealthChannel::HealthChannel(HealthChannel&&) noexcept = default;
HealthChannel& HealthChannel::operator=(HealthChannel&&) noexcept = default;

bool HealthChannel::report(uint64_t alive_counter) {
  return impl_->report(alive_counter);
}

bool HealthChannel::reportStatus(const std::string& key,
                                 const std::string& value) {
  return impl_->reportStatus(key, value);
}

uint64_t HealthChannel::getAliveCounter() const {
  return impl_->getAliveCounter();
}

HealthChannelStatus HealthChannel::getStatus() const {
  return impl_->getStatus();
}

bool HealthChannel::isAlive() const { return impl_->isAlive(); }

void HealthChannel::setTimeout(std::chrono::milliseconds timeout) noexcept {
  impl_->setTimeout(timeout);
}

}  // namespace phm
}  // namespace faw