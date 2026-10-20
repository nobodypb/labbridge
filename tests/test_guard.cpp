#include "labbridge/guard.hpp"
#include <cassert>
using namespace labbridge;
int main() {
  unsigned stops = 0;
  Guard g([&] { ++stops; });
  Limits lim{25, .2, 5, 3000, 5000};
  assert(g.acquire("a", 0, 1000));
  assert(!g.acquire("b", 0, 1000));
  g.configure(lim);
  g.observe({24, .1, 2400, 100, 1}, 100);
  assert(g.may_enable("a", 100));
  g.enabled(100);
  g.tick(1001);
  assert(stops == 1 && !g.may_enable("a", 1001));
  assert(g.acquire("b", 1100, 1000));
  g.observe({24, .1, 2400, 1100, 1}, 1100);
  g.enabled(1100);
  g.observe({24, .21, 5040, 1101, 1}, 1101);
  assert(stops == 2);
  // Freshness cannot be postponed by transaction recovery.
  g.release();
  assert(g.acquire("c", 1200, 10000));
  g.observe({24, .1, 2400, 1200, 1}, 1200);
  g.enabled(1200);
  g.tick(4201);
  assert(stops == 3);
  g.release();
  assert(g.acquire("d", 5000, 10000));
  g.observe({24, .1, 2400, 5000, 1}, 5000);
  g.enabled(5000);
  g.disconnect();
  assert(stops == 4);
  assert(!g.may_enable("d", 5001));
}
