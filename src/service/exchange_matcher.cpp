#include "service/exchange_matcher.hpp"

#include <algorithm>
#include <limits>
#include <queue>

namespace bookloop {
namespace {

struct Edge {
  int to;
  int rev;
  int cap;
  int cost;
  int offer;
};

void add_edge(std::vector<std::vector<Edge>>& g, int a, int b, int cap, int cost, int offer = -1) {
  Edge forward{b, static_cast<int>(g[b].size()), cap, cost, offer};
  Edge backward{a, static_cast<int>(g[a].size()), 0, -cost, -1};
  g[a].push_back(forward);
  g[b].push_back(backward);
}

}  // namespace

std::vector<SwapMatch> match_exchanges(const std::vector<SwapOffer>& offers) {
  const int n = static_cast<int>(offers.size());
  const int source = 0;
  const int left = 1;
  const int right = left + n;
  const int sink = right + n;
  const int nodes = sink + 1;

  std::vector<std::vector<Edge>> g(nodes);
  for (int i = 0; i < n; ++i) {
    add_edge(g, source, left + i, 1, 0);
    add_edge(g, right + i, sink, 1, 0);
  }

  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      if (i != j && offers[i].wanted_book == offers[j].offered_book &&
          offers[i].member != offers[j].member) {
        int score = offers[i].reputation + offers[j].reputation;
        add_edge(g, left + i, right + j, 1, -score, i);
      }
    }
  }

  std::vector<int> potential(nodes, 0);
  std::vector<int> pv(nodes);
  std::vector<int> pe(nodes);
  const int inf = std::numeric_limits<int>::max() / 4;

  for (int flow = 0; flow < n; ++flow) {
    std::vector<int> dist(nodes, inf);
    std::vector<char> settled(nodes, false);
    dist[source] = 0;

    using QueueElement = std::pair<int, int>;
    std::priority_queue<QueueElement, std::vector<QueueElement>, std::greater<QueueElement>> q;
    q.push({0, source});

    while (!q.empty()) {
      auto [d, v] = q.top();
      q.pop();

      if (d != dist[v] || settled[v]) {
        continue;
      }
      settled[v] = true;

      for (int e = 0; e < static_cast<int>(g[v].size()); ++e) {
        auto& edge = g[v][e];
        if (!edge.cap) {
          continue;
        }

        int nd = d + edge.cost + potential[v] - potential[edge.to];
        if (nd < dist[edge.to]) {
          dist[edge.to] = nd;
          pv[edge.to] = v;
          pe[edge.to] = e;
          q.push({nd, edge.to});
        }
      }
    }

    if (dist[sink] == inf) {
      break;
    }

    for (int v = 0; v < nodes; ++v) {
      if (dist[v] < inf) {
        potential[v] += dist[v];
      }
    }

    for (int v = sink; v != source; v = pv[v]) {
      auto& edge = g[pv[v]][pe[v]];
      edge.cap--;
      g[v][edge.rev].cap++;
    }
  }

  std::vector<SwapMatch> result;
  for (int i = 0; i < n; ++i) {
    for (const auto& edge : g[left + i]) {
      if (edge.to >= right && edge.to < sink && edge.cap == 0) {
        int j = edge.to - right;
        result.push_back({
            offers[i].member,
            offers[j].member,
            offers[j].offered_book,
            offers[i].wanted_book,
            offers[i].reputation + offers[j].reputation,
        });
      }
    }
  }

  return result;
}

}  // namespace bookloop
