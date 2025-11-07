#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "GBC.h"

static GBC::GBC* g_gbc = nullptr;

#ifdef __EMSCRIPTEN__
extern "C" {
int upload_rom(uint8_t* data, size_t length) {
    if (!g_gbc) return 0;
    try {
        g_gbc->addresses.load_ROM_buffer(data, length);
        g_gbc->addresses.set_boot_complete(true);
        g_gbc->cpu.RA = 0x01;
        g_gbc->cpu.RB = 0xFF;
        g_gbc->cpu.RC = 0x13;
        g_gbc->cpu.RD = 0x00;
        g_gbc->cpu.RE = 0xC1;
        g_gbc->cpu.RH = 0x84;
        g_gbc->cpu.RL = 0x03;
        g_gbc->cpu.pc = 0x100;
        g_gbc->cpu.sp = 0xFFFE;
        return 1;
    } catch (...) {
        return 0;
    }
}

void set_button_state(int button, int pressed) {
    if (!g_gbc || button < 0 || button >= 8) return;
    g_gbc->buttonState[button] = pressed ? 1 : 0;
}

void set_channel_muted(int channel, int muted) {
    if (!g_gbc || channel < 0 || channel >= 4) return;
    g_gbc->apu.set_channel_mute(static_cast<size_t>(channel), muted != 0);
}

int is_channel_muted(int channel) {
    if (!g_gbc || channel < 0 || channel >= 4) return 0;
    return g_gbc->apu.is_channel_muted(static_cast<size_t>(channel)) ? 1 : 0;
}

void set_master_volume(float volume) {
    if (!g_gbc) return;
    g_gbc->apu.set_master_volume(volume);
}

float get_master_volume() {
    if (!g_gbc) return 1.0f;
    return g_gbc->apu.get_master_volume();
}

int get_frame_count() {
    if (!g_gbc) return 0;
    return g_gbc->frame;
}

void* get_cart_ram_ptr() {
    if (!g_gbc) return nullptr;
    return g_gbc->addresses.get_cartRAM();
}

int get_cart_ram_size() { return 32 * 1024; }

int import_save(uint8_t* data, size_t length) {
    if (!g_gbc) return 0;
    try {
        g_gbc->addresses.load_RAM_buffer(data, length);
        return 1;
    } catch (...) {
        return 0;
    }
}

void emscripten_frame_loop(void* arg) {
    GBC::GBC* gbc = static_cast<GBC::GBC*>(arg);
    gbc->execute_frame();
}
}
#endif

int main(int argc, char* argv[]) {
    bool exportAudio = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--export-audio" || arg == "-e") {
            exportAudio = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options] [rom_path]\n"
                      << "Options:\n"
                      << "  --export-audio, -e    Export individual audio "
                         "channels to WAV files\n"
                      << "  --help, -h            Show this help message\n";
            return 0;
        }
    }

    GBC::GBC gbc;
    g_gbc = &gbc;

    if (exportAudio) {
        gbc.apu.enable_audio_export(true);
    }

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(emscripten_frame_loop, &gbc, 0, 1);
    return 0;
#else
    std::string path;
    if (argc > 1 && argv[argc - 1][0] != '-') {
        path = argv[argc - 1];
    } else {
        std::cout << "enter path to rom: " << '\n';
        std::cin >> path;
    }
    gbc.addresses.load_ROM(path.c_str(), GBC::KB * GBC::KB);
    // std::string savepath =
    //     std::string("cartRAMdump_").append(path.c_str()).append("(save).bin");
    // if (std::filesystem::exists("cartRAMdump(save).bin")) {
    //     FILE *fp = fopen("cartRAMdump(save).bin", "r");
    //     for (int i = 0; i < 32 * GBC::KB; ++i) {
    //         if (feof(fp) == EOF) break;
    //         gbc.addresses.get_cartRAM()[i] = fgetc(fp);
    //     }
    //     fclose(fp);
    // }
    gbc.run();
    return 0;
#endif
}