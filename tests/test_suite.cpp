#include "model/domain.hpp"
#include "service/catalog_search.hpp"
#include "service/exchange_matcher.hpp"
#include "service/reading_stack.hpp"
#include <iostream>
#include <string>
#include <vector>
using namespace bookloop;
int main(){
  std::vector<Book> books{{1,"The Left Hand of Darkness","Ursula K. Le Guin","Speculative fiction","Very good","Mira",1969,47,304,3,3,""},{2,"Kindred","Octavia Butler","Science fiction","Good","Sam",1979,48,287,2,2,""},{3,"The Dispossessed","Ursula K. Le Guin","Speculative fiction","Very good","Ishan",1974,46,341,1,1,""},{4,"Piranesi","Susanna Clarke","Literary fiction","Like new","Tara",2020,45,272,2,2,""}};
  int passed=0,failed=0;
  auto check=[&](const std::string& name,bool ok){std::cout<<"{\"test\":\""<<name<<"\",\"passed\":"<<(ok?"true":"false")<<"}\n";if(ok)++passed;else ++failed;};
  auto search=search_books(books,"left hand darkness");check("fuzzy_title_search",!search.empty());
  auto stack=build_reading_stack(books,700);int pages=0;for(int id:stack)for(const auto&b:books)if(b.id==id)pages+=b.pages;check("reading_stack_within_budget",pages<=700);
  std::vector<SwapOffer> offers{{"A",1,3,5},{"B",3,4,5},{"C",4,1,4}};auto matches=match_exchanges(offers);check("exchange_matching_finds_compatible_readers",!matches.empty());
  std::cout<<"{\"summary\":{\"passed\":"<<passed<<",\"failed\":"<<failed<<"}}\n";return failed?1:0;
}
