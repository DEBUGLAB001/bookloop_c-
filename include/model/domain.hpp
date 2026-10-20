#pragma once

#include <string>
#include <vector>

namespace bookloop {

struct Book {
  int id;
  std::string title;
  std::string author;
  std::string genre;
  std::string condition;
  std::string owner;
  int year;
  int rating_tenths;
  int pages;
  int copies;
  int available;
  std::string description;
};

struct BookHit {
  int id;
  int score;
};

struct SwapOffer {
  std::string member;
  int offered_book;
  int wanted_book;
  int reputation;
};

struct SwapMatch {
  std::string from_member;
  std::string to_member;
  int offered_book;
  int wanted_book;
  int score;
};

}  // namespace bookloop
