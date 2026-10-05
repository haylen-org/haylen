#include <iostream>
#include <string>
#include <vector>

#include "content/ContentTool.hpp"

// Entry point of `haylen-content`, which runs the command line it was given.
int main(int argc, char* argv[]) {
    return haylen::content::ContentTool(std::cout, std::cerr).run(std::vector<std::string>(argv + 1, argv + argc));
}
