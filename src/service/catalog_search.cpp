#include "service/catalog_search.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <numeric>
#include <unordered_map>

namespace bookloop {
namespace {

struct TrieNode {
  std::unordered_map<char, int> next;
  std::vector<int> ids;
};

std::string normalize(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

}  // namespace

std::vector<BookHit> search_books(const std::vector<Book>& books, const std::string& raw) {
  const auto query = normalize(raw);
  if (query.empty()) {
    return {};
  }

  std::vector<TrieNode> trie(1);
  for (const auto& b : books) {
    auto title = normalize(b.title);
    int node_idx = 0;
    for (char c : title) {
      auto it = trie[node_idx].next.find(c);
      if (it == trie[node_idx].next.end()) {
        int next_idx = static_cast<int>(trie.size());
        trie[node_idx].next[c] = next_idx;
        trie.emplace_back();
        node_idx = next_idx;
      } else {
        node_idx = it->second;
      }
    }
    trie[node_idx].ids.push_back(b.id);
  }

  std::vector<int> initial(query.size() + 1);
  std::iota(initial.begin(), initial.end(), 0);

  std::vector<BookHit> hits;
  std::function<void(int, int, const std::vector<int>&)> visit =
      [&](int node, int depth, const std::vector<int>& previous) {
        for (int id : trie[node].ids) {
          if (previous.back() <= 2) {
            hits.push_back({id, previous.back()});
          }
        }

        for (const auto& edge : trie[node].next) {
          std::vector<int> row(query.size() + 1);
          row[0] = depth + 1;
          for (std::size_t j = 1; j < row.size(); ++j) {
            row[j] = std::min({
                row[j - 1] + 1,
                previous[j] + 1,
                previous[j - 1] + (edge.first == query[j - 1] ? 0 : 1),
            });
          }

          if (row.back() <= 2) {
            visit(edge.second, depth + 1, row);
          }
        }
      };

  visit(0, 0, initial);
  std::sort(hits.begin(), hits.end(), [](const auto& a, const auto& b) {
    return a.score < b.score;
  });

  return hits;
}

}  // namespace bookloop
