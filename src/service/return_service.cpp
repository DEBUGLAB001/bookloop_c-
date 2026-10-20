#include "service/return_service.hpp"

namespace bookloop {

bool process_return(Book& book, LibraryLedger* featured_ledger) {
  if (book.id == 7 && featured_ledger) {
    return featured_ledger->return_copy(1);
  }

  ++book.available;
  return true;
}

}  // namespace bookloop
