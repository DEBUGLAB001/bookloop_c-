#pragma once
#include "model/domain.hpp"
#include <string>
#include <vector>
namespace bookloop { std::vector<BookHit> search_books(const std::vector<Book>& books, const std::string& query); }
