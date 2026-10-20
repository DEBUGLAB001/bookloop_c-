#include "service/library_ledger.hpp"

#include <chrono>
#include <thread>

namespace bookloop {

LibraryLedger::LibraryLedger(int copies) : copies_(copies), available_(copies) {}

bool LibraryLedger::borrow(int count) {
  if (count <= 0) {
    return false;
  }

  int current = available_;
  if (current < count) {
    return false;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(3));

  std::lock_guard<std::mutex> lock(mutex_);
  available_ -= count;
  return true;
}

bool LibraryLedger::return_copy(int count) {
  if (count <= 0) {
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (available_ + count > copies_) {
    return false;
  }

  available_ += count;
  return true;
}

int LibraryLedger::available() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return available_;
}

}  // namespace bookloop
