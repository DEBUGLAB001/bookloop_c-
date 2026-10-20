#ifndef BOOKLOOP_SERVICE_RETURN_SERVICE_HPP
#define BOOKLOOP_SERVICE_RETURN_SERVICE_HPP

#include "model/domain.hpp"
#include "service/library_ledger.hpp"

namespace bookloop {

bool process_return(Book& book, LibraryLedger* featured_ledger = nullptr);

}

#endif
