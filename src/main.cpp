#include "httplib.h"
#include "json.hpp"
#include "model/domain.hpp"
#include "service/catalog_search.hpp"
#include "service/catalog_store.hpp"
#include "service/exchange_matcher.hpp"
#include "service/library_ledger.hpp"
#include "service/reading_stack.hpp"
#include "service/return_service.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

using json = nlohmann::json;
using namespace bookloop;

namespace {

CatalogStore catalog;
LibraryLedger featured_ledger(1);
std::vector<SwapOffer> offers{
    {"Mira", 1, 3, 5},
    {"Ishan", 3, 4, 5},
    {"Leena", 4, 1, 4},
    {"Sam", 2, 1, 3},
    {"Tara", 10, 2, 5},
    {"Ayan", 11, 10, 4},
};
std::vector<std::string> swap_requests;

json book_json(const Book& b) {
  return {
      {"id", b.id},
      {"title", b.title},
      {"author", b.author},
      {"genre", b.genre},
      {"condition", b.condition},
      {"owner", b.owner},
      {"year", b.year},
      {"rating", b.rating_tenths / 10.0},
      {"pages", b.pages},
      {"copies", b.copies},
      {"available", b.id == 7 ? featured_ledger.available() : b.available},
      {"description", b.description},
  };
}

}  // namespace

int main() {
  httplib::Server server;
  server.set_mount_point("/", "./public");

  server.Get("/api/health", [](const auto&, auto& res) {
    res.set_content(R"({"status":"ok"})", "application/json");
  });

  server.Get("/api/state", [](const auto&, auto& res) {
    json rows = json::array();
    for (const auto& b : catalog.get_books()) {
      rows.push_back(book_json(b));
    }

    auto matches = match_exchanges(offers);
    json swaps = json::array();
    for (const auto& m : matches) {
      auto* offered = catalog.find_book(m.offered_book);
      auto* wanted = catalog.find_book(m.wanted_book);
      if (offered && wanted) {
        swaps.push_back({
            {"from", m.from_member},
            {"to", m.to_member},
            {"offered", book_json(*offered)},
            {"wanted", book_json(*wanted)},
            {"score", m.score},
        });
      }
    }

    json state = {
        {"status", "ONLINE"},
        {"books", rows},
        {"swaps", swaps},
        {"swap_requests", swap_requests.size()},
        {"members", 18},
        {"circulating", 12},
    };
    res.set_content(state.dump(), "application/json");
  });

  server.Get("/api/search", [](const auto& req, auto& res) {
    std::string q = req.has_param("q") ? req.get_param_value("q") : "";
    auto hits = search_books(catalog.get_books(), q);

    json rows = json::array();
    for (const auto& h : hits) {
      if (auto* b = catalog.find_book(h.id)) {
        rows.push_back({
            {"book", book_json(*b)},
            {"distance", h.score},
        });
      }
    }

    json result = {
        {"query", q},
        {"results", rows},
    };
    res.set_content(result.dump(), "application/json");
  });

  server.Post("/api/borrow", [](const auto& req, auto& res) {
    json body;
    try {
      body = json::parse(req.body);
    } catch (...) {
      body = json::object();
    }

    int id = body.value("id", 0);
    auto* b = catalog.find_book(id);
    if (!b) {
      res.status = 404;
      res.set_content(R"({"error":"Book not found"})", "application/json");
      return;
    }

    bool ok = false;
    if (id == 7) {
      ok = featured_ledger.borrow(1);
    } else {
      ok = catalog.borrow_copy(id);
    }

    json response = {
        {"accepted", ok},
        {"book", book_json(*b)},
        {"message", ok ? "Copy checked out. Happy reading!" : "No available copy at the moment."},
    };
    res.set_content(response.dump(), "application/json");
  });

  server.Post("/api/return", [](const auto& req, auto& res) {
    json body;
    try {
      body = json::parse(req.body);
    } catch (...) {
      body = json::object();
    }

    int id = body.value("id", 0);
    auto* b = catalog.find_book(id);
    if (!b) {
      res.status = 404;
      res.set_content(R"({"error":"Book not found"})", "application/json");
      return;
    }

    bool ok = process_return(*b, &featured_ledger);
    json response = {
        {"accepted", ok},
        {"book", book_json(*b)},
        {"message", ok ? "Copy returned to the shelf." : "That return could not be matched to a loan."},
    };
    res.set_content(response.dump(), "application/json");
  });

  server.Post("/api/exchange/request", [](const auto& req, auto& res) {
    json body;
    try {
      body = json::parse(req.body);
    } catch (...) {
      body = json::object();
    }

    auto member = body.value("member", std::string{});
    bool known = std::any_of(offers.begin(), offers.end(), [&](const auto& o) {
      return o.member == member;
    });

    if (!known) {
      res.status = 404;
      res.set_content(R"({"error":"Reader not found"})", "application/json");
      return;
    }

    swap_requests.push_back(member);
    json response = {
        {"accepted", true},
        {"member", member},
        {"message", "Your swap note was sent to " + member + "."},
    };
    res.set_content(response.dump(), "application/json");
  });

  server.Post("/api/reading-stack", [](const auto& req, auto& res) {
    json body;
    try {
      body = json::parse(req.body);
    } catch (...) {
      body = json::object();
    }

    int budget = std::clamp(body.value("pages", 700), 100, 1800);
    auto ids = build_reading_stack(catalog.get_books(), budget);

    json rows = json::array();
    int pages = 0;
    for (int id : ids) {
      if (auto* b = catalog.find_book(id)) {
        rows.push_back(book_json(*b));
        pages += b->pages;
      }
    }

    json response = {
        {"budget", budget},
        {"pages", pages},
        {"books", rows},
    };
    res.set_content(response.dump(), "application/json");
  });

  int port = 8081;
  if (const char* env = std::getenv("PORT")) {
    port = std::atoi(env);
  }

  std::cout << "BookLoop listening on 0.0.0.0:" << port << std::endl;
  return server.listen("0.0.0.0", port) ? 0 : 1;
}
