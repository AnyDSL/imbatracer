#include <iostream>
#include <fstream>
#include "compiler.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage : js2impala file.json" << std::endl;
        return 1;
    }

    std::ifstream in(argv[1]);
    if (!in) return 1;

    compile(in, std::cout);
    return 0;
}

