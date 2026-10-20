#include "labbridge/timing.hpp"
#include <cassert>
using namespace labbridge;
int main() {
  assert(delay_ticks(5, 100) == 1);
  assert(delay_ticks(5, 1000) == 5);
  assert(delay_ticks(1, 250) == 1);
  assert(delay_ticks(200, 100) == 20);
  assert(delay_ticks(0, 100) == 1);
}
