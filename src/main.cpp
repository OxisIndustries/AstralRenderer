#include "astral/application.hpp"
#include <iostream>

int main() {
  try {
    astral::Application app;
    app.run();
  } catch (const std::exception &e) {
    std::cerr << "Fatal Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
