# BookLoop — Neighbourhood Library & Book Exchange

BookLoop is a community library and book exchange web platform that enables neighbours to discover books on local shelves, borrow and return physical copies, and set up reader-to-reader swaps. Readers can explore circulating titles across diverse genres, search for specific books, coordinate swaps with nearby members, and assemble custom reading stacks to match their available weekend reading time.

## 1. Application Overview

### Core Functionality

- Browse Catalog: Explore neighbourhood books with details on title, author, genre, condition, contributor, page count, and physical copy availability.
- Search Books: Find books quickly using title and keyword searches that tolerate minor typing mistakes.
- Borrow & Return Books: Check out available books from the neighbourhood shelf and return them once finished reading.
- Peer-to-Peer Book Swaps: Connect with neighbours to exchange books based on mutual reading preferences and contributor reputations.
- Weekend Reading Stack: Specify an available page budget to receive a recommended stack of books tailored for a weekend read.

## 2. Debugging Challenge

BookLoop contains six defects that cause errors during search, book swaps, reading stack recommendations, and book checkout and return operations. Although the interface loads smoothly, catalog updates and calculations produce inaccurate or corrupted data under specific actions. The objective is to resolve these defects so the platform operates reliably for readers.

### Reported Issues & Tasks:

#### Issue 1: Trie-Guided Fuzzy Catalog Search Prematurely Prunes Valid Title Matches

- User Symptom: Searching for a title such as "left hand darkness" yields no results even though the book exists on the shelf.
- Task: Ensure search queries find matching books even when the reader makes minor typos or searches with partial phrases.

#### Issue 2: Weighted Exchange Matching Misranks Residual Rerouting Paths Due to Zero-Potential Dijkstra Initialization

- User Symptom: The community swap system matches readers sub-optimally or misses viable pairings between readers who want each other's books.
- Task: Ensure the exchange matching system pairs readers to achieve the highest possible overall community satisfaction.

#### Issue 3: Weekend Reading Stack Employs a Greedy Density Heuristic for 0/1 Selection

- User Symptom: Requesting a reading stack for a given page budget misses combinations of books that offer a higher total rating within the budget.
- Task: Ensure the reading stack builder selects the set of available books that maximizes total reader rating without exceeding the page limit.

#### Issue 4: Race Condition in LibraryLedger::borrow Permits Inventory Overselling

- User Symptom: When two readers attempt to borrow the last remaining copy of a book at the same time, both checkouts succeed, resulting in a negative inventory count.
- Task: Ensure simultaneous checkout attempts allow only one borrower to take the final copy while rejecting subsequent requests.

#### Issue 5: Unvalidated Book Returns Generate Phantom Inventory and Exceed Physical Copy Limits

- User Symptom: Returning a book that was never borrowed or already returned increases the available copy count beyond the total number of books owned by the library.
- Task: Reject returns when all copies are already on the shelf or when no matching loan is active.

#### Issue 6: Unsynchronized Concurrent Mutations and Data Races on Global Catalog State

- User Symptom: When multiple readers borrow, return, or browse books at the same time, catalog information can become corrupted or display outdated copy counts.
- Task: Coordinate simultaneous catalog updates so multiple readers can browse, borrow, and return books without causing data conflicts.

## 3. Expected Behavior After Fixing Bugs

1. Searching for "left hand darkness" successfully returns "The Left Hand of Darkness".
2. Community book swaps find optimal pairings that maximize reader exchange satisfaction.
3. The weekend reading stack recommends the combination of books with the highest rating that fits within the requested page budget.
4. Simultaneous checkouts for the last available copy grant the book to one reader and inform other readers that the copy is unavailable.
5. Returning a book only succeeds if a copy is currently checked out, preventing available inventory from exceeding total library ownership.
6. Simultaneous requests from multiple readers execute smoothly without displaying corrupted or mismatched catalog numbers.

The automated test suite can be run using:
```bash
cmake --build build --target challenge_tests
./build/challenge_tests
# On Windows:
.\build\challenge_tests.exe
```

After fixing the defects, all catalog operations, inventory transitions, and recommendation services will operate consistently and accurately across all reader interactions.
