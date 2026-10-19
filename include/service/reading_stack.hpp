#pragma once
#include "model/domain.hpp"
#include <vector>
namespace bookloop { std::vector<int> build_reading_stack(const std::vector<Book>& books, int page_budget); }
