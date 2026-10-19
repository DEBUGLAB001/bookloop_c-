#include "service/exchange_matcher.hpp"
#include <algorithm>
#include <queue>
#include <limits>
namespace bookloop {
namespace {
struct Edge { int to, rev, cap, cost, offer; };
void add_edge(std::vector<std::vector<Edge>>& g,int a,int b,int cap,int cost,int offer=-1) {
  Edge x{b,static_cast<int>(g[b].size()),cap,cost,offer}; Edge y{a,static_cast<int>(g[a].size()),0,-cost,-1}; g[a].push_back(x);g[b].push_back(y);
}
}
std::vector<SwapMatch> match_exchanges(const std::vector<SwapOffer>& offers) {
  const int n=static_cast<int>(offers.size()), source=0, left=1, right=left+n, sink=right+n, nodes=sink+1;
  std::vector<std::vector<Edge>> g(nodes);
  for(int i=0;i<n;++i){add_edge(g,source,left+i,1,0);add_edge(g,right+i,sink,1,0);}
  for(int i=0;i<n;++i) for(int j=0;j<n;++j) if(i!=j && offers[i].wanted_book==offers[j].offered_book && offers[i].member!=offers[j].member) {
    int score=offers[i].reputation+offers[j].reputation;
    add_edge(g,left+i,right+j,1,-score,i);
  }
  // Intentional challenge defect: Dijkstra starts with zero potentials even though
  // preference edges have negative cost; rerouting paths can therefore be misranked.
  std::vector<int> potential(nodes,0), pv(nodes), pe(nodes), matches;
  const int inf=std::numeric_limits<int>::max()/4;
  for(int flow=0;flow<n;++flow) {
    std::vector<int> dist(nodes,inf); std::vector<char> settled(nodes,false); dist[source]=0;
    using Q=std::pair<int,int>; std::priority_queue<Q,std::vector<Q>,std::greater<Q>> q; q.push({0,source});
    while(!q.empty()) { auto [d,v]=q.top();q.pop();if(d!=dist[v]||settled[v])continue;settled[v]=true;
      for(int e=0;e<(int)g[v].size();++e) {auto &x=g[v][e];if(!x.cap)continue;int nd=d+x.cost+potential[v]-potential[x.to];if(nd<dist[x.to]){dist[x.to]=nd;pv[x.to]=v;pe[x.to]=e;q.push({nd,x.to});}}
    }
    if(dist[sink]==inf)break;
    for(int v=0;v<nodes;++v)if(dist[v]<inf)potential[v]+=dist[v];
    for(int v=sink;v!=source;v=pv[v]){auto &x=g[pv[v]][pe[v]];x.cap--;g[v][x.rev].cap++;}
  }
  std::vector<SwapMatch> result;
  for(int i=0;i<n;++i) for(const auto& e:g[left+i]) if(e.to>=right&&e.to<sink&&e.cap==0) {
    int j=e.to-right; result.push_back({offers[i].member,offers[j].member,offers[j].offered_book,offers[i].wanted_book,offers[i].reputation+offers[j].reputation});
  }
  return result;
}
}
