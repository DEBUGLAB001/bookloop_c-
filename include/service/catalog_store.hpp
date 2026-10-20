#pragma once

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
  std::vector<Book> books_;
};

}  // namespace bookloop
