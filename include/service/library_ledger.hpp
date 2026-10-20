#pragma once

#include <mutex>

namespace bookloop {

class LibraryLedger {
 public:
  explicit LibraryLedger(int copies);

  bool borrow(int count);
  bool return_copy(int count);
  int available() const;

 private:
  int copies_;
  int available_;
  mutable std::mutex mutex_;
};

}  // namespace bookloop
