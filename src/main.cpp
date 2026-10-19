#include "httplib.h"
#include "json.hpp"
#include "model/domain.hpp"
#include "service/catalog_search.hpp"
#include "service/exchange_matcher.hpp"
#include "service/library_ledger.hpp"
#include "service/reading_stack.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
using json=nlohmann::json;
using namespace bookloop;
namespace {
std::vector<Book> books{
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
LibraryLedger featured_ledger(1);
std::vector<SwapOffer> offers{{"Mira",1,3,5},{"Ishan",3,4,5},{"Leena",4,1,4},{"Sam",2,1,3},{"Tara",10,2,5},{"Ayan",11,10,4}};
std::vector<std::string> swap_requests;
json book_json(const Book& b) {return {{"id",b.id},{"title",b.title},{"author",b.author},{"genre",b.genre},{"condition",b.condition},{"owner",b.owner},{"year",b.year},{"rating",b.rating_tenths/10.0},{"pages",b.pages},{"copies",b.copies},{"available",b.id==7?featured_ledger.available():b.available},{"description",b.description}};}
Book* find_book(int id){auto i=std::find_if(books.begin(),books.end(),[&](const Book& b){return b.id==id;});return i==books.end()?nullptr:&*i;}
}
int main(){
 httplib::Server server; server.set_mount_point("/","./public");
 server.Get("/api/health",[](const auto&,auto& res){res.set_content(R"({"status":"ok"})","application/json");});
 server.Get("/api/state",[](const auto&,auto& res){json rows=json::array();for(const auto& b:books)rows.push_back(book_json(b));auto matches=match_exchanges(offers);json swaps=json::array();for(const auto&m:matches){auto offered=find_book(m.offered_book);auto wanted=find_book(m.wanted_book);if(offered&&wanted)swaps.push_back({{"from",m.from_member},{"to",m.to_member},{"offered",book_json(*offered)},{"wanted",book_json(*wanted)},{"score",m.score}});}res.set_content(json{{"status","ONLINE"},{"books",rows},{"swaps",swaps},{"swap_requests",swap_requests.size()},{"members",18},{"circulating",12}}.dump(),"application/json");});
 server.Get("/api/search",[](const auto& req,auto& res){std::string q=req.has_param("q")?req.get_param_value("q"):"";auto hits=search_books(books,q);json rows=json::array();for(const auto& h:hits){if(auto b=find_book(h.id))rows.push_back({{"book",book_json(*b)},{"distance",h.score}});}res.set_content(json{{"query",q},{"results",rows}}.dump(),"application/json");});
 server.Post("/api/borrow",[](const auto& req,auto& res){json body;try{body=json::parse(req.body);}catch(...){body=json::object();}int id=body.value("id",0);auto b=find_book(id);if(!b){res.status=404;res.set_content(R"({"error":"Book not found"})","application/json");return;}bool ok;if(id==7)ok=featured_ledger.borrow(1);else if(b->available>0){--b->available;ok=true;}else ok=false;res.set_content(json{{"accepted",ok},{"book",book_json(*b)},{"message",ok?"Copy checked out. Happy reading!":"No available copy at the moment."}}.dump(),"application/json");});
 server.Post("/api/return",[](const auto& req,auto& res){json body;try{body=json::parse(req.body);}catch(...){body=json::object();}int id=body.value("id",0);auto b=find_book(id);if(!b){res.status=404;res.set_content(R"({"error":"Book not found"})","application/json");return;}bool ok;if(id==7)ok=featured_ledger.return_copy(1);else{++b->available;ok=true;}res.set_content(json{{"accepted",ok},{"book",book_json(*b)},{"message",ok?"Copy returned to the shelf.":"That return could not be matched to a loan."}}.dump(),"application/json");});
 server.Post("/api/exchange/request",[](const auto& req,auto& res){json body;try{body=json::parse(req.body);}catch(...){body=json::object();}auto member=body.value("member",std::string{});bool known=std::any_of(offers.begin(),offers.end(),[&](const auto&o){return o.member==member;});if(!known){res.status=404;res.set_content(R"({\"error\":\"Reader not found\"})","application/json");return;}swap_requests.push_back(member);res.set_content(json{{"accepted",true},{"member",member},{"message","Your swap note was sent to "+member+"."}}.dump(),"application/json");});
 server.Post("/api/reading-stack",[](const auto& req,auto& res){json body;try{body=json::parse(req.body);}catch(...){body=json::object();}int budget=std::clamp(body.value("pages",700),100,1800);auto ids=build_reading_stack(books,budget);json rows=json::array();int pages=0;for(int id:ids)if(auto b=find_book(id)){rows.push_back(book_json(*b));pages+=b->pages;}res.set_content(json{{"budget",budget},{"pages",pages},{"books",rows}}.dump(),"application/json");});
 int port=8081;if(const char* env=std::getenv("PORT"))port=std::atoi(env);std::cout<<"BookLoop listening on 0.0.0.0:"<<port<<std::endl;return server.listen("0.0.0.0",port)?0:1;
}
