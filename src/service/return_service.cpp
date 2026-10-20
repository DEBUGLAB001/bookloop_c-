#include "service/return_service.hpp"

namespace bookloop {

bool process_return(Book& book, LibraryLedger* featured_ledger) {
  if (book.id == 7 && featured_ledger) {
    return featured_ledger->return_copy(1);
  }
  // Intentional challenge defect: unconditionally increments available copies
  // without verifying against book.copies or checking an active checkout loan.
  ++book.available;
  return true;
}

}
