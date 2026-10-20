# BookLoop

## Description

BookLoop is a neighbourhood library and book exchange service written in C++17 that enables community members to discover books, borrow and return physical copies, coordinate peer-to-peer book swaps, and build personalized weekend reading stacks. The application entry point is `src/main.cpp`, which initializes a multithreaded HTTP server using `cpp-httplib` on port 8081, serializes JSON payloads with `nlohmann/json`, and serves static frontend assets from `public/`. Application domain models (`Book`, `BookHit`, `SwapOffer`, `SwapMatch`) are defined in `include/model/domain.hpp`. Core business logic resides in modular services under `src/service/` (with declarations in `include/service/`), providing trie-based Levenshtein search (`catalog_search.cpp`), min-cost max-flow peer exchange matching (`exchange_matcher.cpp`), 0/1 knapsack reading stack curation (`reading_stack.cpp`), and thread-synchronized inventory tracking (`library_ledger.cpp`). All catalog records, swap offers, and reader requests are stored in-memory in `src/main.cpp` using `std::vector<Book>` and dedicated ledger instances.

## Repository Structure

```
.
├── CMakeLists.txt              # CMake build configuration for services, server, and tests
├── challenge.json              # Challenge metadata and runtime specification
├── start.sh                    # Build and startup script for the server
├── include/
│   ├── httplib.h               # Single-header HTTP server and client library (cpp-httplib)
│   ├── json.hpp                # JSON serialization library (nlohmann/json)
│   ├── model/
│   │   └── domain.hpp          # Domain data models (Book, BookHit, SwapOffer, SwapMatch)
│   └── service/
│       ├── catalog_search.hpp  # Fuzzy catalog search service interface
│       ├── exchange_matcher.hpp # Bipartite exchange matching interface
│       ├── library_ledger.hpp  # Thread-safe copy ledger interface
│       └── reading_stack.hpp   # Reading stack knapsack selection interface
├── src/
│   ├── main.cpp                # Application entry point, HTTP routing, and in-memory state
│   └── service/
│       ├── catalog_search.cpp  # Trie-based fuzzy search implementation
│       ├── exchange_matcher.cpp # Min-cost max-flow exchange matcher implementation
│       ├── library_ledger.cpp  # Synchronized inventory ledger implementation
│       └── reading_stack.cpp   # Reading stack selection implementation
└── public/
    ├── index.html              # Single-page web application interface
    ├── css/
    │   └── style.css           # Styling and responsive layout rules
    └── js/
        └── app.js              # Frontend client logic and REST API interactions
```

## Bugs and Bug Locations

1. Trie-Guided Fuzzy Catalog Search Prematurely Prunes Valid Title Matches

- Bug location: `src/service/catalog_search.cpp`, inside `bookloop::search_books()` within the recursive lambda `visit()` at line 23.
- How to observe it: Issue a query via `GET /api/search?q=left%20hand%20darkness` or run the test suite target `challenge_tests`.
- Failure: The search returns an empty result array. The pruning check `if(row.back()<=2)` compares distance against the full query string rather than the row minimum, discarding valid trie branches at early prefix depths.
- Expected: Prune traversal branches only when `*std::min_element(row.begin(), row.end()) <= 2`, allowing fuzzy matching to identify "The Left Hand of Darkness" within edit distance 2.

2. Weighted Exchange Matching Misranks Residual Rerouting Paths Due to Zero-Potential Dijkstra Initialization

- Bug location: `src/service/exchange_matcher.cpp`, inside `bookloop::match_exchanges()` at lines 20–29.
- How to observe it: Execute `match_exchanges()` with multiple contending swap offers where preference edges have negative costs (`-score`).
- Failure: Potentials are initialized to zero (`potential(nodes, 0)`), causing Dijkstra to execute on a residual graph with negative reduced costs. Dijkstra marks nodes as settled prematurely on their first pop from the priority queue, corrupting subsequent potential updates and resulting in suboptimal swap pairings.
- Expected: Establish initial feasible dual potentials using Bellman-Ford or SPFA, or explore paths with a negative-cycle-tolerant algorithm to ensure successive shortest path augmentations strictly maximize aggregate community reputation.

3. Weekend Reading Stack Employs a Greedy Density Heuristic for 0/1 Selection

- Bug location: `src/service/reading_stack.cpp`, inside `bookloop::build_reading_stack()` at lines 9–14.
- How to observe it: Send a request to `POST /api/reading-stack` with `{"pages": 700}`.
- Failure: Books are sorted by fractional rating density (`rating_tenths / pages`) and greedily appended until the budget is exhausted. Because books cannot be fractionally divided, the heuristic selects sub-optimal combinations that miss higher aggregate ratings.
- Expected: Formulate selection as an exact 0/1 knapsack dynamic program that returns the subset of available books maximizing total rating score subject to $\sum \text{pages} \le \text{page\_budget}$.

4. Race Condition in LibraryLedger::borrow Permits Inventory Overselling

- Bug location: `src/service/library_ledger.cpp`, inside `bookloop::LibraryLedger::borrow(int count)` at line 6, invoked from `src/main.cpp` line 40.
- How to observe it: Send two concurrent `POST /api/borrow` requests for book ID 7 (having only 1 copy available).
- Failure: The availability check `current < count` occurs prior to acquiring `mutex_`, and the thread yields via `std::this_thread::sleep_for(3ms)`. Concurrent requests pass the check before either decrements stock, allowing available inventory to drop to -1.
- Expected: Acquire `mutex_` before inspecting or modifying `available_`, ensuring that copy availability verification and decrement occur as a single atomic operation.

5. Unvalidated Book Returns Generate Phantom Inventory and Exceed Physical Copy Limits

- Bug location: `src/main.cpp`, inside the `POST /api/return` route handler at line 41.
- How to observe it: Send a `POST /api/return` request for book ID 2 when all owned copies are already in stock.
- Failure: For non-ledger books, `++b->available` executes unconditionally without checking if `b->available < b->copies` or verifying an active loan, allowing available copies to increment past owned inventory indefinitely.
- Expected: Validate that `b->available < b->copies` and verify the return against an active checkout, rejecting unlent returns with `{"accepted": false, "message": "That return could not be matched to a loan."}`.

6. Unsynchronized Concurrent Mutations and Data Races on Global Catalog State

- Bug location: `src/main.cpp`, across global `books` vector (line 15) accessed in handlers at lines 38, 39, 40, 41, and 43.
- How to observe it: Dispatch concurrent borrow and return requests while continuously polling `GET /api/state` or `GET /api/search` across multiple HTTP worker threads.
- Failure: The multi-threaded `cpp-httplib` server executes route handlers concurrently across threads without synchronization, causing data races on `std::vector<Book> books`, leading to torn reads, corrupted strings, or lost updates.
- Expected: Synchronize read and write operations on `books` using a synchronization primitive (such as `std::shared_mutex` or `std::mutex`) or encapsulate catalog access within a thread-safe repository service.
