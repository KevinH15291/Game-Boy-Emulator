#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

#include "GBC.h"

int main() {
    GBC::GBC gbc;

#ifdef __EMSCRIPTEN__
    gbc.run();
    return 0;
#else
    std::string path;
    std::cout << "enter path to rom: " << '\n';

    std::cin >> path;
    gbc.addresses.load_ROM(path.c_str(), GBC::KB * GBC::KB);
    std::string savepath =
        std::string("cartRAMdump_").append(path.c_str()).append("(save).bin");
    if (std::filesystem::exists("cartRAMdump(save).bin")) {
        FILE *fp = fopen("cartRAMdump(save).bin", "r");
        for (int i = 0; i < 32 * GBC::KB; ++i) {
            if (feof(fp) == EOF) break;
            gbc.addresses.get_cartRAM()[i] = fgetc(fp);
        }
        fclose(fp);
    }
    gbc.run();
    return 0;
#endif
}