#include "service/catalog_search.hpp"
#include <algorithm>
#include <cctype>
#include <functional>
#include <numeric>
#include <unordered_map>
namespace bookloop { namespace {
struct Node { std::unordered_map<char,int> next; std::vector<int> ids; };
std::string norm(std::string value) { std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));}); return value; }
}
std::vector<BookHit> search_books(const std::vector<Book>& books,const std::string& raw) {
  const auto query=norm(raw); if(query.empty()) return {};
  std::vector<Node> trie(1);
  for(const auto& b:books) { auto title=norm(b.title); int n=0; for(char c:title) { auto it=trie[n].next.find(c); if(it==trie[n].next.end()){int next=static_cast<int>(trie.size());trie[n].next[c]=next;trie.emplace_back();n=next;}else n=it->second; } trie[n].ids.push_back(b.id); }
  std::vector<int> initial(query.size()+1); std::iota(initial.begin(),initial.end(),0);
  std::vector<BookHit> hits;
  std::function<void(int,int,const std::vector<int>&)> visit=[&](int node,int depth,const std::vector<int>& previous){
    for(int id:trie[node].ids) if(previous.back()<=2) hits.push_back({id,previous.back()});
    for(const auto& edge:trie[node].next) {
      std::vector<int> row(query.size()+1); row[0]=depth+1;
      for(std::size_t j=1;j<row.size();++j) row[j]=std::min({row[j-1]+1,previous[j]+1,previous[j-1]+(edge.first==query[j-1]?0:1)});
      // Intentional challenge defect: this prunes on the full-query column, not the row minimum.
      if(row.back()<=2) visit(edge.second,depth+1,row);
    }
  };
  visit(0,0,initial);
  std::sort(hits.begin(),hits.end(),[](const auto&a,const auto&b){return a.score<b.score;});
  return hits;
}
}
