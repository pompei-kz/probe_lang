
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <filesystem>
#include <iostream>
#include "resources.hpp"

int main(int argc, char *argv[])
{
  std::cout << "y8dMCKbb9Q :: SomeTextFile_txt" << std::endl;

  std::cout << resources::SomeTextFile_txt << std::endl;


  return 0;
}
