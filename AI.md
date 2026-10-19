# BookLoop

## Description

BookLoop is a neighbourhood library and book exchange platform engineered to facilitate local book discovery, inventory tracking, peer-to-peer exchanges, and reading schedule optimization. The application is implemented as a native C++17 service utilizing `cpp-httplib` for multi-threaded HTTP request routing and `nlohmann/json` for payload serialization, with an integrated editorial frontend built with vanilla HTML5, CSS3, and modern JavaScript.

### Primary Users and Workflows

1. **Browsing Readers**: Community members discover books currently in local circulation. Readers filter books by genre (*Speculative fiction*, *Science fiction*, *Computing*, *Literary fiction*, *Nature writing*, *Science*) or search the community shelf using typo-tolerant fuzzy matching.
2. **Borrowers and Returners**: Readers check out physical copies from the shelf (`POST /api/borrow`) and return them when finished (`POST /api/return`). The frontend tracks the member's active checkout session, dynamically adjusting availability indicators and card button states.
3. **Book Exchangers**: Community members participate in reciprocal book swaps. An exchange matching engine calculates peer matches based on offered books, desired books, and reader reputation scores. Members can send direct notes to potential swap partners (`POST /api/exchange/request`).
4. **Weekend Readers**: Readers define a target page budget (e.g., 250–1,400 pages) via an interactive slider, querying the stack builder (`POST /api/reading-stack`) to obtain an optimal reading selection of available titles that fit within their weekend reading capacity.

### Architectural Components

- **HTTP & API Gateway (`src/main.cpp`)**: Initializes `httplib::Server`, binds to `0.0.0.0:8081` (or `PORT` environment variable), mounts static frontend assets from `./public`, and registers JSON REST endpoints (`/api/health`, `/api/state`, `/api/search`, `/api/borrow`, `/api/return`, `/api/exchange/request`, `/api/reading-stack`).
- **Domain Model (`include/model/domain.hpp`)**: Declares core structures: `Book`, `BookHit`, `SwapOffer`, and `SwapMatch`.
- **Fuzzy Search Service (`src/service/catalog_search.cpp`, `include/service/catalog_search.hpp`)**: Constructs a prefix trie over catalog titles and performs a Levenshtein distance dynamic programming search to find matches within an edit distance threshold of 2.
- **Exchange Matcher Service (`src/service/exchange_matcher.cpp`, `include/service/exchange_matcher.hpp`)**: Implements successive shortest path min-cost max-flow across a bipartite graph to pair swap offers while maximizing aggregate community reputation.
- **Reading Stack Service (`src/service/reading_stack.cpp`, `include/service/reading_stack.hpp`)**: Assembles a collection of available books that fit within a specified page budget.
- **Inventory Ledger Service (`src/service/library_ledger.cpp`, `include/service/library_ledger.hpp`)**: Provides a thread-synchronized ledger (`LibraryLedger`) with mutex-guarded copy checkout and return operations.
- **Static Frontend Client (`public/index.html`, `public/css/style.css`, `public/js/app.js`)**: Single-page editorial client that interfaces with the C++ backend via asynchronous `fetch` calls.

### Data Storage and Access Model

All application data is maintained in-memory:
- Catalog books are stored as an in-memory vector `std::vector<Book> books` initialized with 12 curated titles.
- Special ledger tracking is applied to featured inventory (book ID 7, *Designing Data-Intensive Applications*) using an instance of `LibraryLedger`.
- Swap offers are initialized in a static vector `std::vector<SwapOffer> offers` containing 6 community offers across 6 members (*Mira*, *Ishan*, *Leena*, *Sam*, *Tara*, *Ayan*).
- Reader swap inquiries are recorded in `std::vector<std::string> swap_requests`.

---

## Repository Structure

```text
bookloop/
├── CMakeLists.txt                 CMake build configuration for library, server, and test targets
├── README.md                      Candidate-facing application overview and debugging challenge guide
├── AI.md                          Technical repository documentation, architectural guide, and bug reference
├── challenge.json                 Challenge metadata (runtime, port, framework, bug counts)
├── start.sh                       POSIX shell script to build and launch the server in release mode
├── include/
│   ├── httplib.h                  Single-header C++17 HTTP/HTTPS server and client library (cpp-httplib)
│   ├── json.hpp                   Single-header JSON for Modern C++ library (nlohmann/json)
│   ├── model/
│   │   └── domain.hpp             Domain structures (Book, BookHit, SwapOffer, SwapMatch)
│   └── service/
│       ├── catalog_search.hpp     Function prototype for trie-based fuzzy catalog search
│       ├── exchange_matcher.hpp   Function prototype for weighted bipartite exchange matching
│       ├── library_ledger.hpp     Class definition and interface for thread-safe copy ledger
│       └── reading_stack.hpp      Function prototype for page-budgeted reading stack selection
├── src/
│   ├── main.cpp                   Application entry point, HTTP routing, endpoint handlers, and static mounting
│   └── service/
│       ├── catalog_search.cpp     Trie construction and Levenshtein edit-distance traversal implementation
│       ├── exchange_matcher.cpp   Min-cost max-flow exchange matching implementation
│       ├── library_ledger.cpp     Synchronized copy checkout and return ledger implementation
│       └── reading_stack.cpp      Knapsack reading stack recommendation implementation
├── public/
│   ├── index.html                 Single-page application markup with catalog shelf, swaps, and stack builder
│   ├── css/
│   │   └── style.css              Application styling, layout, typography, and responsive design
│   └── js/
│       └── app.js                 Frontend state management, API client, event handlers, and UI rendering
└── tests/
    ├── run_tests.sh               POSIX shell script to compile and run native challenge tests
    └── test_suite.cpp             Native C++ test suite evaluating search, stack, and exchange services
```

---

## Bugs and Bug Locations

### 1. Trie-Guided Fuzzy Catalog Search Prematurely Prunes Valid Title Matches

- **Bug location**: `src/service/catalog_search.cpp` in `bookloop::search_books()`, specifically inside the recursive lambda `visit()` at lines 22–24:
  ```cpp
  // Intentional challenge defect: this prunes on the full-query column, not the row minimum.
  if(row.back()<=2) visit(edge.second,depth+1,row);
  ```
- **How to observe it**:
  1. Start the server and submit a multi-word or prefix query, such as:
     `GET /api/search?q=left%20hand%20darkness`
  2. Or run the native test suite via `./build/challenge_tests` (or `.\build\challenge_tests.exe` on Windows).
- **Failure**: The endpoint returns an empty result set `{"query":"left hand darkness","results":[]}`. In the native test suite, `fuzzy_title_search` fails. The book *"The Left Hand of Darkness"* (ID 1) is never found.
- **Expected**: The search should return *"The Left Hand of Darkness"* with an edit distance score indicating a match. In a trie-based fuzzy search with dynamic programming edit distance, `row[j]` holds the Levenshtein distance between the current trie prefix and the query prefix of length $j$. `row.back()` is the distance between the current prefix and the *entire* query string. Pruning on `row.back() <= 2` terminates traversal at early shallow nodes whose short prefixes necessarily have high edit distance to the full query. The pruning condition must evaluate the minimum value across the entire DP row:
  ```cpp
  if(*std::min_element(row.begin(), row.end()) <= 2) visit(edge.second, depth+1, row);
  ```

### 2. Weighted Exchange Matching Misranks Residual Rerouting Paths Due to Zero-Potential Dijkstra Initialization

- **Bug location**: `src/service/exchange_matcher.cpp` in `bookloop::match_exchanges()` at lines 20–29:
  ```cpp
  // Intentional challenge defect: Dijkstra starts with zero potentials even though
  // preference edges have negative cost; rerouting paths can therefore be misranked.
  std::vector<int> potential(nodes,0), pv(nodes), pe(nodes), matches;
  ```
- **How to observe it**:
  1. Inspect `match_exchanges()` execution when provided with multiple contending swap offers where residual path flow rerouting is necessary to maximize aggregate community reputation score.
  2. Inspect the reduced edge costs in Dijkstra's inner relaxation loop at line 28:
     ```cpp
     int nd = d + x.cost + potential[v] - potential[x.to];
     ```
- **Failure**: Swap preference edges are assigned negative costs equal to `-score` (where `score = offers[i].reputation + offers[j].reputation`). With `potential` initialized to 0, Dijkstra runs on a graph containing negative edge weights (`x.cost < 0`). Dijkstra marks nodes as settled the first time they are dequeued from the priority queue (`settled[v] = true`). When negative reduced costs exist, this greedy choice is invalid: a shorter path can reach an already-settled node later. Furthermore, subsequent residual graph potential updates ($\pi(v) \leftarrow \pi(v) + \text{dist}[v]$) become corrupt, leading to suboptimal match pairings or failed augmentations.
- **Expected**: Min-cost max-flow successive shortest path requires that all reduced costs in the residual graph are non-negative ($\hat{c}_{u,v} \ge 0$). Initial feasible potentials must be calculated before running Dijkstra (e.g., using Bellman-Ford or SPFA on the initial DAG of preference edges), or residual path exploration must utilize an algorithm capable of handling negative edge costs (such as SPFA) to guarantee that total community reputation is maximized.

### 3. Weekend Reading Stack Employs a Greedy Density Heuristic for 0/1 Selection

- **Bug location**: `src/service/reading_stack.cpp` in `bookloop::build_reading_stack()` at lines 9–14:
  ```cpp
  std::sort(ranked.begin(),ranked.end(),[](auto a,auto b){return a.first>b.first;});
  int used=0;
  // Intentional challenge defect: a fractional value-per-page ranking is used
  // for a 0/1 selection, so the chosen stack can miss the best total rating.
  for(auto entry:ranked) {
    auto it=std::find_if(books.begin(),books.end(),[&](const Book& b){return b.id==entry.second;});
    if(it!=books.end()&&used+it->pages<=page_budget){
      used+=it->pages;
      selected.push_back(entry.second);
    }
  }
  ```
- **How to observe it**:
  1. Send an HTTP request to `POST /api/reading-stack` with `{"pages": 700}`:
     ```http
     POST /api/reading-stack HTTP/1.1
     Content-Type: application/json

     {"pages": 700}
     ```
  2. Compare the returned books and total rating against the global optimum of all available books whose page sum is $\le 700$.
- **Failure**: The algorithm sorts books by fractional value density (`rating_tenths / pages`) and greedily includes them in descending order. Because individual books cannot be fractionally split, this greedy heuristic fails to find the optimal 0/1 knapsack combination, frequently selecting high-density short books that prevent fitting a combination of larger books with higher aggregate rating.
- **Expected**: The reading stack selection must be implemented using 0/1 knapsack dynamic programming:
  Let $dp[w]$ be the maximum rating achievable with page budget $w \le \text{page\_budget}$. By tracking chosen items during state transitions, the service will return the exact combination of available books that maximizes $\sum \text{rating\_tenths}$ subject to $\sum \text{pages} \le \text{page\_budget}$.

### 4. Race Condition in `LibraryLedger::borrow` Permits Inventory Overselling

- **Bug location**: `src/service/library_ledger.cpp` in `bookloop::LibraryLedger::borrow(int count)` at line 6, invoked via `src/main.cpp` line 40 for book ID 7:
  ```cpp
  bool LibraryLedger::borrow(int count) {
    if(count<=0)return false;
    int current=available_;
    if(current<count)return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    std::lock_guard<std::mutex> lock(mutex_);
    available_-=count;
    return true;
  }
  ```
- **How to observe it**:
  1. Send two simultaneous `POST /api/borrow` requests for book ID 7 (*Designing Data-Intensive Applications*, which starts with `copies: 1`, `available: 1`):
     ```bash
     curl -X POST http://127.0.0.1:8081/api/borrow -H "Content-Type: application/json" -d '{"id":7}' &
     curl -X POST http://127.0.0.1:8081/api/borrow -H "Content-Type: application/json" -d '{"id":7}' &
     ```
  2. Observe the responses and query `GET /api/state`.
- **Failure**: Both requests receive `{"accepted": true}` with HTTP 200. The ledger's available count drops to `-1`, overselling the single physical copy. The Time-of-Check to Time-of-Use (TOCTOU) gap occurs because `current = available_` and `current < count` are evaluated *before* acquiring `mutex_`, and the thread artificially yields with `std::this_thread::sleep_for(std::chrono::milliseconds(3))` before locking the mutex.
- **Expected**: The availability check and decrement must occur atomically inside the critical section protected by `mutex_`:
  ```cpp
  bool LibraryLedger::borrow(int count) {
    if(count<=0) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if(available_ < count) return false;
    available_ -= count;
    return true;
  }
  ```

### 5. Unvalidated Book Returns Generate Phantom Inventory and Exceed Physical Copy Limits

- **Bug location**: `src/main.cpp` in the `POST /api/return` route handler at line 41:
  ```cpp
  server.Post("/api/return",[](const auto& req,auto& res){
    json body;try{body=json::parse(req.body);}catch(...){body=json::object();}
    int id=body.value("id",0);
    auto b=find_book(id);
    if(!b){res.status=404;res.set_content(R"({"error":"Book not found"})","application/json");return;}
    bool ok;if(id==7)ok=featured_ledger.return_copy(1);else{++b->available;ok=true;}
    res.set_content(json{{"accepted",ok},{"book",book_json(*b)},{"message",ok?"Copy returned to the shelf.":"That return could not be matched to a loan."}}.dump(),"application/json");
  });
  ```
- **How to observe it**:
  1. Issue a POST request to `/api/return` for book ID 2 (*Kindred*, which starts with `copies: 2`, `available: 2`):
     ```bash
     curl -X POST http://127.0.0.1:8081/api/return -H "Content-Type: application/json" -d '{"id":2}'
     ```
  2. Observe the JSON response.
- **Failure**: The response returns `{"accepted": true, "book": {"id": 2, "copies": 2, "available": 3}}`. For any book other than ID 7, `++b->available` executes unconditionally. No verification is made that `b->available < b->copies`, nor is the return validated against an active loan registry. Repeated requests indefinitely increment availability beyond the total number of physical copies owned.
- **Expected**: The return handler must reject the return if `b->available >= b->copies` or if no active loan exists for the title, returning `{"accepted": false, "message": "That return could not be matched to a loan."}`.

### 6. Unsynchronized Concurrent Mutations and Data Races on Global Catalog State

- **Bug location**: `src/main.cpp` across the global catalog vector `books` (line 15) accessed in handlers for:
  - `POST /api/borrow` (line 40): `else if(b->available>0){--b->available;ok=true;}`
  - `POST /api/return` (line 41): `else{++b->available;ok=true;}`
  - `GET /api/state` (line 38): iterating and serializing `books`
  - `GET /api/search` (line 39): passing `books` by reference into `search_books()`
  - `POST /api/reading-stack` (line 43): passing `books` into `build_reading_stack()`
- **How to observe it**:
  1. Run the server under a high-concurrency test (or compile with ThreadSanitizer `-fsanitize=thread` on a Linux environment).
  2. Send concurrent borrow/return requests while polling `/api/state` or `/api/search`.
- **Failure**: `cpp-httplib` serves incoming HTTP requests using a thread pool. Handler callbacks run simultaneously on different threads. Because `std::vector<Book> books` and its individual `Book` members are read and mutated without synchronization primitives, concurrent execution causes data races (Undefined Behavior under the C++17 memory model). Symptoms include lost checkout updates, corrupted internal string representations, and torn reads.
- **Expected**: All read and write access to the catalog state must be synchronized. A global `std::shared_mutex` (allowing multiple concurrent readers and exclusive writers) or dedicated mutex must protect `books` access, or book copy state must be encapsulated within a thread-safe repository service.

---

## Expected Behaviour After Fixing All Bugs

When all six defects are addressed:

1. **Typo-Tolerant Search Accuracy**: Searching for `left hand darkness` via `GET /api/search` accurately returns *"The Left Hand of Darkness"* within edit distance 2, and `fuzzy_title_search` passes in the native test suite.
2. **Optimal Swap Pairings**: Successive shortest path augmentations in `match_exchanges()` operate with non-negative reduced costs, ensuring that residual paths are explored correctly and total community reputation is maximized.
3. **Exact 0/1 Reading Stack**: `POST /api/reading-stack` computes an exact 0/1 knapsack dynamic program that returns the highest aggregate rating available within the page budget.
4. **Thread-Safe Borrowing Without Overselling**: Simultaneous checkout requests for the final copy of a book are serialized atomically by `LibraryLedger`; exactly one succeeds and subsequent requests are rejected, preventing `available < 0`.
5. **Enforced Catalog Copy Invariants**: `POST /api/return` validates available copies against total catalog stock (`available <= copies`) and rejects phantom returns when no active checkout exists.
6. **Thread-Safe Multi-Worker Operations**: Concurrent reads and writes to catalog state across HTTP worker threads execute safely without data races or memory corruption.

### Automated Test Suite Status

Test execution command:
```bash
cmake --build build --target challenge_tests
./build/challenge_tests
# On Windows:
.\build\challenge_tests.exe
```

**Current Verification Status**:
Executed `.\build\challenge_tests.exe` against the current codebase:
```json
{"test":"fuzzy_title_search","passed":false}
{"test":"reading_stack_within_budget","passed":true}
{"test":"exchange_matching_finds_compatible_readers","passed":true}
{"summary":{"passed":2,"failed":1}}
```
- Current test execution confirms 1 failure (`fuzzy_title_search`) and 2 passes.
- After fixing Bug 1, all 3 tests in the test suite pass with a 0 exit code.
