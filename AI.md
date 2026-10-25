# BookLoop

## Description

BookLoop is a neighbourhood library and book exchange service written in C++17 that enables community members to discover books, borrow and return physical copies, coordinate peer-to-peer book swaps, and assemble personalized weekend reading stacks. The application entry point is `src/main.cpp`, which initializes a multi-threaded HTTP server using `cpp-httplib` on port 8080, serializes JSON payloads with `nlohmann/json`, and serves static frontend assets from `public/`. Application domain models (`Book`, `BookHit`, `SwapOffer`, `SwapMatch`) are defined in `include/model/domain.hpp`. Core business logic resides in modular services under `src/service/` (with declarations in `include/service/`), providing in-memory catalog storage (`catalog_store.cpp`), fuzzy catalog search (`catalog_search.cpp`), peer exchange matching (`exchange_matcher.cpp`), reading stack curation (`reading_stack.cpp`), thread-safe inventory tracking (`library_ledger.cpp`), and book return processing (`return_service.cpp`).

## Repository Structure

```
.
├── CMakeLists.txt              # CMake build configuration
├── challenge.json              # Platform runtime configuration
├── start.sh                    # Build and server startup script
├── include/
│   ├── httplib.h               # Single-header HTTP server library (cpp-httplib)
│   ├── json.hpp                # Single-header JSON serializer (nlohmann/json)
│   ├── model/
│   │   └── domain.hpp          # Domain data models (Book, BookHit, SwapOffer, SwapMatch)
│   └── service/
│       ├── catalog_search.hpp  # Fuzzy catalog search service interface
│       ├── catalog_store.hpp   # In-memory catalog repository store interface
│       ├── exchange_matcher.hpp # Peer exchange matching interface
│       ├── library_ledger.hpp  # Inventory ledger interface
│       ├── reading_stack.hpp   # Reading stack selection interface
│       └── return_service.hpp  # Book return processing interface
├── src/
│   ├── main.cpp                # HTTP server bootstrap, routing, and static file serving
│   └── service/
│       ├── catalog_search.cpp  # Trie-based fuzzy search implementation
│       ├── catalog_store.cpp   # In-memory catalog store implementation
│       ├── exchange_matcher.cpp # Minimum-cost flow exchange matcher implementation
│       ├── library_ledger.cpp  # Inventory ledger implementation
│       ├── reading_stack.cpp   # Reading stack recommendation implementation
│       └── return_service.cpp  # Book return processing implementation
└── public/
    ├── index.html              # Frontend dashboard interface
    ├── css/
    │   └── style.css           # Styling
    └── js/
        └── app.js              # Client-side API integration and UI logic
```

## Bugs and Bug Locations

1. Catalog Search Misses Valid Title Matches

- Bug location: `src/service/catalog_search.cpp`, inside `bookloop::search_books()` at line 73.
- How to observe it: Query `GET /api/search?q=left%20hand%20darkness` or run `./build/challenge_tests`.
- Failure: Searching returns an empty result set. The pruning condition `if (row.back() <= 2)` checks distance against the full query string rather than `*std::min_element(row.begin(), row.end()) <= 2`, discarding valid trie branches at early prefix depths.
- Expected: Prune search branches only when `*std::min_element(row.begin(), row.end()) <= 2`, allowing fuzzy matching to identify "The Left Hand of Darkness" within edit distance 2.

2. Sub-optimal Book Exchange Pairings

- Bug location: `src/service/exchange_matcher.cpp`, inside `bookloop::match_exchanges()` at lines 46–51.
- How to observe it: Execute `match_exchanges()` with multiple contending swap offers having negative cost preference edges (`-score`).
- Failure: Dual potentials are initialized to zero (`potential(nodes, 0)`), causing Dijkstra to execute on a residual graph containing negative reduced costs. Dijkstra marks nodes settled prematurely on initial extraction, corrupting potential updates and producing sub-optimal swap matches.
- Expected: Establish initial feasible dual potentials using Bellman-Ford or SPFA, or find augmenting paths using an algorithm tolerant of negative weights, ensuring successive shortest path augmentations strictly maximize aggregate community reputation.

3. Inaccurate Weekend Reading Stack Selection

- Bug location: `src/service/reading_stack.cpp`, inside `bookloop::build_reading_stack()` at lines 13–29.
- How to observe it: Send a request to `POST /api/reading-stack` with `{"pages": 700}`.
- Failure: Books are sorted by fractional rating density (`rating_tenths / pages`) and greedily added until the budget is exhausted. Because physical books cannot be fractionally divided, this heuristic selects sub-optimal combinations that miss higher aggregate ratings.
- Expected: Formulate selection as an exact 0/1 knapsack dynamic program that returns the subset of available books maximizing total rating score within the specified page budget.

4. Concurrent Borrow Requests Cause Inventory Overselling

- Bug location: `src/service/library_ledger.cpp`, inside `bookloop::LibraryLedger::borrow()` at lines 15–23.
- How to observe it: Send concurrent `POST /api/borrow` requests for book ID 7 (having 1 copy available).
- Failure: The stock check `current < count` occurs before acquiring `mutex_`, and the thread sleeps for 3ms. Multiple concurrent requests pass the check before any decrement occurs, dropping available inventory below zero.
- Expected: Acquire `mutex_` before checking or modifying `available_`, ensuring that availability verification and decrement occur as a single atomic operation.

5. Book Returns Exceed Available Copy Limits

- Bug location: `src/service/return_service.cpp`, inside `bookloop::process_return()` at lines 10–11.
- How to observe it: Send a `POST /api/return` request for a book when all owned copies are already in stock.
- Failure: For standard books, `++book.available` executes unconditionally without verifying whether `book.available < book.copies` or confirming an active loan, allowing available inventory to exceed total physical ownership.
- Expected: Verify that `book.available < book.copies` before incrementing, rejecting returns for books that are already fully stocked or have no active loan.

6. Catalog Inconsistencies During Simultaneous Reader Requests

- Bug location: `src/service/catalog_store.cpp`, within `CatalogStore` read and write methods, called from route handlers in `src/main.cpp`.
- How to observe it: Dispatch concurrent borrow and return requests while continuously polling `GET /api/state` or `GET /api/search` across multiple HTTP worker threads.
- Failure: `cpp-httplib` dispatches incoming requests across a thread pool, but `CatalogStore::books_` is read and mutated concurrently without synchronization, resulting in data races, torn reads, and memory corruption.
- Expected: Synchronize read and write operations on `books_` using synchronization primitives (such as `std::mutex` or `std::shared_mutex`), preventing data races across concurrent server worker threads.
