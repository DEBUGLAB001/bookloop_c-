#pragma once
#include <string>
#include <vector>
namespace bookloop {
struct Book { int id; std::string title, author, genre, condition, owner; int year, rating_tenths, pages, copies, available; std::string description; };
struct BookHit { int id; int score; };
struct SwapOffer { std::string member; int offered_book, wanted_book, reputation; };
struct SwapMatch { std::string from_member, to_member; int offered_book, wanted_book, score; };
}
