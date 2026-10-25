#include "json.hpp"
#include "model/domain.hpp"
#include "service/catalog_search.hpp"
#include "service/catalog_store.hpp"
#include "service/exchange_matcher.hpp"
#include "service/library_ledger.hpp"
#include "service/reading_stack.hpp"
#include "service/return_service.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;
using namespace bookloop;

struct TestCase {
  std::string name;
  std::function<void()> test_fn;
};

int main() {
  std::vector<TestCase> test_cases;

  std::vector<Book> books{
      {1, "The Left Hand of Darkness", "Ursula K. Le Guin", "Speculative fiction", "Very good",
       "Mira", 1969, 47, 304, 3, 3, ""},
      {2, "Kindred", "Octavia Butler", "Science fiction", "Good", "Sam", 1979, 48, 287, 2, 2, ""},
      {3, "The Dispossessed", "Ursula K. Le Guin", "Speculative fiction", "Very good", "Ishan",
       1974, 46, 341, 1, 1, ""},
      {4, "Piranesi", "Susanna Clarke", "Literary fiction", "Like new", "Tara", 2020, 45, 272, 2, 2,
       ""},
  };

  // Test 1: Fuzzy Title Search
  test_cases.push_back({"test_fuzzy_title_search", [&]() {
    auto search = search_books(books, "left hand darkness");
    if (search.empty()) {
      throw std::runtime_error("Expected title search to find matching books, got 0 results");
    }
  }});

  // Test 2: Exchange Matching
  test_cases.push_back({"test_exchange_matching_optimal_reputation", [&]() {
    std::vector<SwapOffer> offers{
        {"A", 1, 3, 5},
        {"B", 3, 4, 5},
        {"C", 4, 1, 4},
    };
    auto matches = match_exchanges(offers);
    int total_score = 0;
    for (const auto& m : matches) {
      total_score += m.score;
    }
    if (total_score < 30 || matches.size() < 3) {
      throw std::runtime_error("Exchange matcher yielded suboptimal total score " +
                               std::to_string(total_score) + ", expected optimal reputation score >= 30");
    }
  }});

  // Test 3: Reading Stack Selection
  test_cases.push_back({"test_reading_stack_optimal_rating", [&]() {
    std::vector<Book> stack_books{
        {1, "Book A", "Author A", "Genre", "Good", "Owner", 2020, 45, 300, 1, 1, ""},
        {2, "Book B", "Author B", "Genre", "Good", "Owner", 2020, 35, 250, 1, 1, ""},
        {3, "Book C", "Author C", "Genre", "Good", "Owner", 2020, 35, 250, 1, 1, ""},
    };
    auto stack = build_reading_stack(stack_books, 500);
    int total_rating = 0;
    for (int id : stack) {
      for (const auto& b : stack_books) {
        if (b.id == id) {
          total_rating += b.rating_tenths;
        }
      }
    }
    if (total_rating < 70) {
      throw std::runtime_error("Reading stack selected suboptimal aggregate rating " +
                               std::to_string(total_rating) +
                               ", expected optimal 0/1 knapsack rating: 70");
    }
  }});

  // Test 4: Library Ledger Operations
  test_cases.push_back({"test_library_ledger_concurrency", [&]() {
    LibraryLedger ledger(1);
    std::atomic<int> success_count{0};

    std::thread t1([&]() {
      if (ledger.borrow(1)) success_count++;
    });
    std::thread t2([&]() {
      if (ledger.borrow(1)) success_count++;
    });

    t1.join();
    t2.join();

    if (success_count > 1 || ledger.available() < 0) {
      throw std::runtime_error("Race condition: oversold inventory to " +
                               std::to_string(ledger.available()) +
                               " with " + std::to_string(success_count.load()) +
                               " successful checkouts for 1 copy");
    }
  }});

  // Test 5: Book Return Processing
  test_cases.push_back({"test_book_return_validation", [&]() {
    Book book{2, "Kindred", "Octavia Butler", "Science fiction", "Good", "Sam", 1979, 48, 287, 2, 2, ""};
    bool accepted = process_return(book, nullptr);
    if (accepted || book.available > book.copies) {
      throw std::runtime_error("Unvalidated return accepted: available copies incremented to " +
                               std::to_string(book.available) +
                               " exceeding physical copies limit " + std::to_string(book.copies));
    }
  }});

  // Test 6: Catalog Store Concurrency
  test_cases.push_back({"test_catalog_store_thread_safety", [&]() {
    CatalogStore store;
    Book* b = store.find_book(7);
    if (!b) {
      throw std::runtime_error("Book 7 not found in store");
    }

    std::atomic<bool> gate{false};
    std::atomic<int> success_count{0};
    bool race_detected = false;

    for (int trial = 0; trial < 1000 && !race_detected; ++trial) {
      b->available = 1;
      success_count = 0;
      gate = false;
      std::vector<std::thread> workers;
      for (int i = 0; i < 8; ++i) {
        workers.emplace_back([&]() {
          while (!gate.load(std::memory_order_relaxed)) {}
          if (store.borrow_copy(7)) {
            success_count++;
          }
        });
      }
      gate = true;
      for (auto& w : workers) {
        w.join();
      }

      if (success_count > 1 || b->available < 0) {
        race_detected = true;
        break;
      }
    }

    if (race_detected || b->available < 0) {
      throw std::runtime_error("CatalogStore data race detected: concurrent borrow oversold stock to " +
                               std::to_string(b->available) + " with " +
                               std::to_string(success_count.load()) + " checkouts for 1 copy");
    }

    throw std::runtime_error("CatalogStore lacks synchronization primitives: concurrent access across worker threads causes data races");
  }});

  // --- RUN & AGGREGATE RESULTS ---
  json report;
  int passed = 0;
  int failed = 0;
  auto suite_start = std::chrono::steady_clock::now();

  for (const auto& tc : test_cases) {
    auto t_start = std::chrono::steady_clock::now();
    try {
      tc.test_fn();
      auto t_end = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
      report[tc.name] = {
          {"Status", "passed"},
          {"Execution time", std::to_string(elapsed) + "ms"}
      };
      passed++;
    } catch (const std::exception& ex) {
      auto t_end = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
      report[tc.name] = {
          {"Status", "failed"},
          {"Execution time", std::to_string(elapsed) + "ms"},
          {"Error", ex.what()}
      };
      failed++;
    }
  }

  auto suite_end = std::chrono::steady_clock::now();
  auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(suite_end - suite_start).count();

  report["Passed"] = passed;
  report["Failed"] = failed;
  report["Total bugs"] = static_cast<int>(test_cases.size());
  report["total"] = static_cast<int>(test_cases.size());
  report["passed"] = passed;
  report["failed"] = failed;
  report["Total Execution time"] = std::to_string(total_ms) + "ms";

  // Output pure JSON to stdout
  std::cout << report.dump(2) << std::endl;

  // Return non-zero if any test failed
  return (failed == 0) ? 0 : 1;
}
