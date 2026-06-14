
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "resources.hpp"
#include <filesystem>
#include <iostream>

#include <fstream>

int main(int argc, char *argv[])
{
  std::cout << "y8dMCKbb9Q :: SomeTextFile_txt" << std::endl;

  std::cout << resources::SomeTextFile_txt << std::endl;

  std::span<const uint8_t> fontBold = resources::fonts::Roboto_Bold_ttf;

  std::cout << "zw3Mv1yz0c :: fontBold.size = " << fontBold.size_bytes() << " bytes" << std::endl;

  std::ofstream fontBoldOut("fontBold.bin", std::ios::binary);

  if (!fontBoldOut) {
    throw std::runtime_error("9jIqQw0tFF :: Cannot open file");
  }

  fontBoldOut.write(reinterpret_cast<const char *>(fontBold.data()), //
                    static_cast<std::streamsize>(fontBold.size()));

  if (!fontBoldOut) {
    throw std::runtime_error("KLhZ8AAKdC :: Write error");
  }

  return 0;
}
