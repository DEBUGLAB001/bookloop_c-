#pragma once

#include "model/domain.hpp"
#include "service/library_ledger.hpp"

namespace bookloop {

bool process_return(Book& book, LibraryLedger* featured_ledger = nullptr);

}  // namespace bookloop
