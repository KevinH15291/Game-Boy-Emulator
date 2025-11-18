#include <cstddef>
#include <cstdio>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <iostream>
#endif

#include "GBC.h"

static GBC::GBC* g_gbc = nullptr;

#ifdef __EMSCRIPTEN__
extern "C" {
int upload_rom(uint8_t* data, size_t length) {
    if (!g_gbc) return 0;
    g_gbc->addresses.load_ROM_buffer(data, length);
    g_gbc->addresses.set_boot_complete(true);
    g_gbc->reset_after_rom_load();
    return 1;
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
    g_gbc->addresses.load_RAM_buffer(data, length);
    return 1;
}

void emscripten_frame_loop(void* arg) {
    GBC::GBC* gbc = static_cast<GBC::GBC*>(arg);
    gbc->execute_frame();
}

void shutdown_emulator() {
    emscripten_cancel_main_loop();
    emscripten_force_exit(0);
}
}
#endif

#ifdef __EMSCRIPTEN__
int main(int /*argc*/, char*[] /*argv*/) {
    GBC::GBC gbc(true);
    g_gbc = &gbc;
    emscripten_set_main_loop_arg(emscripten_frame_loop, &gbc, 60, 1);
    return 0;
}
#else
int main(int argc, char* argv[]) {
    bool exportAudio = false;
    bool headless = false;
    int maxFrames = 0;
    std::string rom_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--export-audio" || arg == "-e") {
            exportAudio = true;
        } else if (arg == "--headless") {
            headless = true;
        } else if (arg == "--frames" && i + 1 < argc) {
            maxFrames = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options] [rom_path]\n"
                      << "Options:\n"
                      << "  --export-audio, -e    Export individual audio "
                         "channels to WAV files\n"
                      << "  --headless            Skip SDL window creation\n"
                      << "  --frames <n>          Frames to run in headless "
                         "mode (default 60)\n"
                      << "  --help, -h            Show this help message\n";
            return 0;
        } else if (!arg.empty() && arg[0] != '-') {
            rom_path = arg;
        }
    }

    GBC::GBC gbc(!headless);
    g_gbc = &gbc;

    if (exportAudio) {
        gbc.apu.enable_audio_export(true);
    }

    std::string path = rom_path;
    if (headless && path.empty()) {
        std::cerr << "Headless mode requires a ROM path argument.\n";
        return 1;
    }
    if (path.empty()) {
        std::cout << "enter path to rom: " << '\n';
        std::cin >> path;
    }
    gbc.addresses.load_ROM(path.c_str());
    gbc.reset_after_rom_load();
    if (headless) {
        const int frames_to_run = maxFrames > 0 ? maxFrames : 60;
        for (int frame = 0; frame < frames_to_run; ++frame) {
            gbc.execute_frame();
        }
        return 0;
    }
    gbc.run();
    return 0;
}
#endif