# BookLoop - Neighbourhood Library and Book Exchange Platform

BookLoop is a neighbourhood library and community book exchange web application designed for readers to discover local titles, borrow and return physical copies, coordinate reader-to-reader swaps, and assemble personalized weekend reading stacks within a page budget. The system combines a modern C++17 backend powered by `cpp-httplib` and `nlohmann/json` with a responsive, editorial-style frontend built using vanilla HTML5, CSS3, and modern JavaScript. The platform exposes a JSON REST API and serves all assets directly as a standalone native service without requiring external web servers or client-side runtime bundlers.

---

## 1. Application Overview

### Core Functionality

- **Community Book Catalog & Shelf**: Displays a curated inventory of neighbourhood books with metadata including title, author, genre (*Speculative fiction*, *Science fiction*, *Computing*, *Literary fiction*, *Nature writing*, *Science*), publication year, community star rating, total page count, physical copy count, and real-time availability. Readers can filter catalog items by genre or explore the entire collection.
- **Typo-Tolerant Catalog Search**: Backed by an in-memory prefix trie and dynamic programming Levenshtein distance calculations, the search engine allows readers to query the catalog by title or keywords. Results are scored and ranked by edit distance to provide instant, typo-tolerant suggestions.
- **Live Copy Borrowing & Checkout**: Enables community members to check out available copies from the local shelf via `POST /api/borrow`. The client interface tracks user loans within the session and toggles card actions between borrowing, waitlisting, and returns.
- **Book Return & Shelf Replenishment**: Allows borrowers to return physical copies to the shelf via `POST /api/return`, updating availability metrics and reflecting updated stock in the UI in real time.
- **Reader-to-Reader Book Swaps (Exchange Matching)**: Connects readers seeking reciprocal book exchanges using a weighted bipartite matching service. Readers can review peer recommendations (displaying who offers and wants specific books along with community reputation scores) and transmit swap messages to fellow readers via `POST /api/exchange/request`.
- **Weekend Reading Stack Builder**: Computes a curated weekend reading recommendation bundle tailored to a reader's available reading time. Readers adjust a page budget slider (250 to 1,400 pages) and submit requests to `POST /api/reading-stack` to receive a selection of available shelf favourites fitting the budget.
- **Responsive Editorial UI**: Features a typographic, warm aesthetic (palette centered around `#f4f0e8`, serif headlines, custom book jackets, live indicators, and interactive toast notifications) with zero external client dependencies.

### Technology Stack

- **Runtime & Language**: C++17 (compiled native executable).
- **HTTP Server**: `cpp-httplib` (v0.15+, header-only multi-threaded HTTP server in `include/httplib.h`).
- **Data Serialization**: `nlohmann/json` (v3.11+, header-only modern C++ JSON library in `include/json.hpp`).
- **Frontend Technologies**: Vanilla HTML5, CSS3, and ES6 JavaScript (`public/index.html`, `public/css/style.css`, `public/js/app.js`), served directly by the native server.
- **Persistence & State Model**: In-memory domain structures (`Book`, `BookHit`, `SwapOffer`, `SwapMatch`) and stateful services (`LibraryLedger`, `catalog_search`, `exchange_matcher`, `reading_stack`).
- **Testing Framework**: Native C++ test harness (`tests/test_suite.cpp`) executed via standard build targets and reporting structured JSON results.
- **Build System**: CMake 3.20+ with support for Ninja or Make generators, and native socket libraries (`ws2_32` on Windows, POSIX sockets on Linux/macOS).

### Build, Run, and Validation Commands

#### Building the Application

Using CMake and Ninja / Make:

```bash
# Configure the build directory
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build all targets (server and challenge_tests)
cmake --build build -j2
```

On Windows environments using Ninja and Clang++ or MinGW:

```powershell
cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

#### Starting the Application

```bash
# Run the compiled server executable
./build/server

# On Windows:
.\build\server.exe
```

By default, the server binds to `0.0.0.0:8081`. You can configure a different port by setting the `PORT` environment variable:

```bash
PORT=9090 ./build/server
```

Once running, navigate to `http://127.0.0.1:8081` in your browser.

#### Running Tests

```bash
# Build the test target
cmake --build build --target challenge_tests

# Execute the test binary
./build/challenge_tests

# On Windows:
.\build\challenge_tests.exe
```

A shell script wrapper is also provided in `tests/run_tests.sh`.

---

## 2. Debugging Challenge

The BookLoop service layer contains six intentional defects across its data structure, algorithms, concurrency, and validation routines. While the web interface presents a smooth user experience, several core operations fail or corrupt application state under specific search queries, concurrent access, or inventory updates.

The challenge objective is to trace request execution through the HTTP handlers (`src/main.cpp`), locate the underlying defects in the service implementations (`src/service/*.cpp`), and correct them while maintaining full thread safety and algorithmic optimality.

### Reported Issues & Tasks

#### Issue 1: Trie-Guided Fuzzy Catalog Search Prematurely Prunes Valid Title Matches

- **Reproduction**: Enter `left hand darkness` into the catalog search box in the web interface, or issue an HTTP request:
  ```http
  GET /api/search?q=left%20hand%20darkness HTTP/1.1
  Host: 127.0.0.1:8081
  ```
- **Failure**: The search query returns an empty result list `{"query":"left hand darkness","results":[]}`, and the native test `fuzzy_title_search` fails. The book *"The Left Hand of Darkness"* (ID 1) is never returned despite closely matching the query.
- **Expected**: The search should return *"The Left Hand of Darkness"* with an edit distance score indicating a valid fuzzy match. The recursive trie traversal should prune branches only when the minimum value across the entire Levenshtein DP row exceeds the edit distance threshold ($\le 2$), rather than checking only the full-query column (`row.back()`).

#### Issue 2: Weighted Exchange Matching Misranks Residual Rerouting Paths Due to Zero-Potential Dijkstra Initialization

- **Reproduction**: Invoke `match_exchanges()` with swap offers whose preference edges carry negative costs, or evaluate reader swaps through `GET /api/state` where multi-hop residual path rerouting is required to maximize total community reputation score.
- **Failure**: The min-cost max-flow successive shortest path algorithm executes Dijkstra with initial potentials set to zero across all nodes. Because preference edges have negative costs (`-score`), the reduced costs $\hat{c}_{u,v} = c_{u,v} + \pi(u) - \pi(v)$ become negative. Dijkstra violates its fundamental invariant by marking nodes settled prematurely upon removal from the priority queue, leading to misranked or suboptimal book exchange pairings when residual paths are explored.
- **Expected**: The network flow routine must establish valid initial dual potentials using Bellman-Ford or SPFA, or preserve non-negative reduced costs on residual edges, ensuring that successive shortest path augmentations strictly maximize aggregate community reputation.

#### Issue 3: Weekend Reading Stack Employs a Greedy Density Heuristic for 0/1 Selection

- **Reproduction**: Send an HTTP POST request to `/api/reading-stack` specifying a page budget:
  ```http
  POST /api/reading-stack HTTP/1.1
  Host: 127.0.0.1:8081
  Content-Type: application/json

  {"pages": 700}
  ```
- **Failure**: The service calculates a fractional density ratio (`rating_tenths / pages`), sorts the books in descending order of density, and greedily accumulates whole books until the budget is exhausted. Because books cannot be fractionally divided, this greedy approach misses higher-rated combinations of whole books that fit within the 700-page limit.
- **Expected**: The recommendation service must formulate the selection as an exact 0/1 knapsack dynamic programming problem, returning the subset of available books that maximizes total community rating while strictly satisfying $\sum \text{pages} \le \text{page\_budget}$.

#### Issue 4: Race Condition in `LibraryLedger::borrow` Permits Inventory Overselling

- **Reproduction**: Dispatch two concurrent HTTP POST checkout requests simultaneously for book ID 7 (*Designing Data-Intensive Applications*, which has only 1 physical copy available):
  ```bash
  # Execute two simultaneous requests
  curl -X POST http://127.0.0.1:8081/api/borrow -H "Content-Type: application/json" -d '{"id":7}' &
  curl -X POST http://127.0.0.1:8081/api/borrow -H "Content-Type: application/json" -d '{"id":7}' &
  ```
- **Failure**: Both concurrent requests receive HTTP 200 with `{"accepted": true}`. Inspecting `/api/state` reveals that the book's availability has dropped to `-1`, overselling the single copy to multiple readers.
- **Expected**: `LibraryLedger::borrow` must verify copy availability inside the critical section protected by `mutex_`. If `available_ < count`, the method must atomically reject the request and return `false`, preventing inventory from dropping below zero under concurrent load.

#### Issue 5: Unvalidated Book Returns Generate Phantom Inventory and Exceed Physical Copy Limits

- **Reproduction**: Send a return request for any catalog title that is already fully stocked (for example, book ID 2, *Kindred*, which has 2 total copies owned and 2 copies available):
  ```http
  POST /api/return HTTP/1.1
  Host: 127.0.0.1:8081
  Content-Type: application/json

  {"id": 2}
  ```
- **Failure**: The endpoint unconditionally increments availability, returning `{"accepted": true, "book": {"copies": 2, "available": 3}}`. Successive calls continue incrementing availability indefinitely (4, 5, 6...), creating phantom copies that do not physically exist.
- **Expected**: The return endpoint must validate that incoming returns do not exceed total physical copies (`b->available < b->copies`) and must verify that the returning reader holds an active loan for the title. If no copies are currently checked out, the return must be rejected with `{"accepted": false, "message": "That return could not be matched to a loan."}`.

#### Issue 6: Unsynchronized Concurrent Mutations and Data Races on Global Catalog State

- **Reproduction**: Execute concurrent borrow and return operations across books 1–6 and 8–12 while simultaneously issuing `GET /api/state` or `GET /api/search` queries across multiple threads.
- **Failure**: The global catalog `std::vector<Book> books` in `src/main.cpp` is concurrently read and mutated by multiple worker threads in `cpp-httplib` without synchronization. This introduces data races and undefined behavior under C++ memory models, resulting in torn reads, corrupted strings, and lost inventory increments or decrements.
- **Expected**: Access to shared catalog records must be thread-safe. All reads and writes across worker threads must be coordinated through synchronization primitives (such as a read-write lock, mutex, or atomic variables) or encapsulated within a thread-safe repository service.

---

## 3. Expected Behavior After Fixing Bugs

1. **Fuzzy Search Verification**: Querying `GET /api/search?q=left%20hand%20darkness` correctly returns *"The Left Hand of Darkness"* with an edit distance score $\le 2$, and the `fuzzy_title_search` test passes.
2. **Optimal Exchange Assignment**: Calling `match_exchanges()` resolves swap offers using non-negative reduced costs, ensuring that residual rerouting correctly maximizes aggregate community reputation.
3. **Optimal Reading Stack**: Sending `POST /api/reading-stack` with any page budget returns the optimal 0/1 knapsack combination of available books that maximizes total community rating without exceeding the page budget.
4. **Atomic Checkout Protection**: Sending concurrent checkout requests for the final available copy of any book permits exactly one checkout to succeed (`accepted: true`) and rejects all subsequent attempts (`accepted: false`), maintaining `available >= 0`.
5. **Bounded Inventory Returns**: Calling `POST /api/return` rejects unlent returns, ensuring that available copies never exceed total catalog ownership (`available <= copies`).
6. **Thread-Safe Catalog Access**: High-concurrency read and write operations execute without data races, torn reads, or inventory drift.

### Automated Test Suite Execution

The project includes an automated test target configured in CMake (`challenge_tests`).

Command to execute tests:
```bash
./build/challenge_tests
# On Windows:
.\build\challenge_tests.exe
```

**Current Verification Status**:
Executing `.\build\challenge_tests.exe` against the unmodified repository produces the following results:
```json
{"test":"fuzzy_title_search","passed":false}
{"test":"reading_stack_within_budget","passed":true}
{"test":"exchange_matching_finds_compatible_readers","passed":true}
{"summary":{"passed":2,"failed":1}}
```
- `fuzzy_title_search`: **FAILED** (0 results returned due to Issue 1)
- `reading_stack_within_budget`: **PASSED** (verifies page budget constraint, but does not assert 0/1 knapsack rating optimality)
- `exchange_matching_finds_compatible_readers`: **PASSED** (verifies non-empty match generation, but does not assert residual path rerouting optimality)
- Overall suite: **1 failed, 2 passed** (exit code 1).

After resolving the reported issues, all test suites will pass cleanly with zero failures.
#   b o o k l o o p _ c -  
 