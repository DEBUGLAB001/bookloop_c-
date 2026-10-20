#include "service/reading_stack.hpp"

#include <algorithm>

namespace bookloop {

std::vector<int> build_reading_stack(const std::vector<Book>& books, int page_budget) {
  std::vector<int> selected;
  std::vector<std::pair<double, int>> ranked;

  for (const auto& b : books) {
    if (b.available > 0) {
      ranked.push_back({static_cast<double>(b.rating_tenths) / b.pages, b.id});
    }
  }

  std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
    return a.first > b.first;
  });

  int used = 0;
  for (const auto& entry : ranked) {
    auto it = std::find_if(books.begin(), books.end(), [&](const Book& b) {
      return b.id == entry.second;
    });

    if (it != books.end() && used + it->pages <= page_budget) {
      used += it->pages;
      selected.push_back(entry.second);
    }
  }

  return selected;
}

}  // namespace bookloop
