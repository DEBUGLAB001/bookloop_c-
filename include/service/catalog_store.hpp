#ifndef BOOKLOOP_SERVICE_CATALOG_STORE_HPP
#define BOOKLOOP_SERVICE_CATALOG_STORE_HPP

#include "model/domain.hpp"
#include <vector>

namespace bookloop {

class CatalogStore {
public:
  CatalogStore();
  std::vector<Book>& get_books();
  const std::vector<Book>& get_books() const;
  Book* find_book(int id);
  bool borrow_copy(int id);

private:
  // Intentional challenge defect: global catalog state is stored and mutated without
  // synchronization primitives, causing data races across concurrent HTTP threads.
  std::vector<Book> books_;
};

}

#endif
