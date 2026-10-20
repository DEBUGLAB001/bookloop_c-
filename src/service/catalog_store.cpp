#include "service/catalog_store.hpp"
#include <algorithm>

namespace bookloop {

CatalogStore::CatalogStore() {
  books_ = {
    {1,"The Left Hand of Darkness","Ursula K. Le Guin","Speculative fiction","Very good","Mira",1969,47,304,3,3,"A landmark journey through a world shaped by climate, politics, and belonging."},
    {2,"Kindred","Octavia E. Butler","Science fiction","Good","Sam",1979,48,287,2,2,"A sharp, moving story of family, history, and survival."},
    {3,"The Dispossessed","Ursula K. Le Guin","Speculative fiction","Very good","Ishan",1974,46,341,1,1,"Two worlds, one physicist, and a question about what freedom costs."},
    {4,"A Psalm for the Wild-Built","Becky Chambers","Science fiction","Like new","Leena",2021,45,160,2,2,"A tea monk and a robot take a gentle walk toward a new question."},
    {5,"The Overstory","Richard Powers","Literary fiction","Good","Mira",2018,43,512,1,1,"Nine lives become entangled around the quiet intelligence of trees."},
    {6,"Algorithms to Live By","Brian Christian & Tom Griffiths","Computing","Very good","Dev",2016,44,368,2,2,"Computer science ideas illuminate everyday decisions."},
    {7,"Designing Data-Intensive Applications","Martin Kleppmann","Computing","Good","Noor",2017,49,616,1,1,"A practical tour of the systems behind reliable data applications."},
    {8,"The Pragmatic Programmer","David Thomas & Andrew Hunt","Computing","Good","Ravi",1999,46,352,2,2,"A field guide to thoughtful, adaptable software work."},
    {9,"Braiding Sweetgrass","Robin Wall Kimmerer","Nature writing","Very good","Asha",2013,48,408,1,1,"Botany and Indigenous knowledge meet in essays about reciprocity."},
    {10,"Piranesi","Susanna Clarke","Literary fiction","Like new","Tara",2020,45,272,2,2,"A solitary scholar maps an endless house filled with tides and statues."},
    {11,"The Fifth Season","N. K. Jemisin","Fantasy","Good","Ayan",2015,47,512,2,2,"A broken world tests the bonds between a mother and her daughter."},
    {12,"An Immense World","Ed Yong","Science","Very good","Priya",2022,46,464,1,1,"A vivid exploration of the sensory worlds other animals inhabit."}
  };
}

std::vector<Book>& CatalogStore::get_books() {
  return books_;
}

const std::vector<Book>& CatalogStore::get_books() const {
  return books_;
}

Book* CatalogStore::find_book(int id) {
  auto i = std::find_if(books_.begin(), books_.end(), [&](const Book& b){ return b.id == id; });
  return i == books_.end() ? nullptr : &*i;
}

bool CatalogStore::borrow_copy(int id) {
  auto b = find_book(id);
  if (!b) return false;
  // Intentional challenge defect: concurrent updates without mutex synchronization
  if (b->available > 0) {
    --b->available;
    return true;
  }
  return false;
}

}
