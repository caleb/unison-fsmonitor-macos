#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>

template <typename T>
class blocking_queue {
private:
  std::mutex d_mutex;
  std::condition_variable d_condition;
  std::condition_variable empty_condition;
  std::deque<T> d_queue;

public:
  void push(T const &value) {
    {
      std::unique_lock<std::mutex> lock(this->d_mutex);
      d_queue.push_front(value);
    }
    this->d_condition.notify_one();
  }
  T pop() {
    std::unique_lock<std::mutex> lock(this->d_mutex);
    this->d_condition.wait(lock, [=] { return !this->d_queue.empty(); });
    T rc(std::move(this->d_queue.back()));
    this->d_queue.pop_back();
    if (this->d_queue.empty()) {
      this->empty_condition.notify_one();
    }
    return rc;
  }
  void wait_until_empty() {
    std::unique_lock<std::mutex> lock(this->d_mutex);
    this->empty_condition.wait(lock, [=] { return this->d_queue.empty(); });
    return;
  }
  bool tryPop(T &v, std::chrono::milliseconds dur) {
    std::unique_lock<std::mutex> lock(this->d_mutex);
    if (!this->d_condition.wait_for(lock, dur, [=] { return !this->d_queue.empty(); })) {
      return false;
    }
    v = std::move(this->d_queue.back());
    this->d_queue.pop_back();
    return true;
  }
  int size() {
    return d_queue.size();
  }
};
