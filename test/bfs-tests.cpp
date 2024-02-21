#include <catch2/catch_test_macros.hpp>
#include <iostream>

TEST_CASE("Foo", "[bfs]") {
  SECTION("A section") { std::cout << "ok" << std::endl; }
}