#pragma once
#include <iostream>
#include <stdexcept>
#include <string>

#define CHECK(...) do { if (!(__VA_ARGS__)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #__VA_ARGS__); } while (false)
template<class Exception, class Function> void Throws(Function&& function) {
  try { function(); } catch (const Exception&) { return; }
  throw std::runtime_error("Expected exception was not thrown");
}
template<class Function> int Run(Function&& function) {
  try { function(); std::cout << "All checks passed\n"; return 0; }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
