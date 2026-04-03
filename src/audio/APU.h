#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>

#include <array>
#include <cstdint>
#include <fstream>

#include "../CgbConfig.h"
#include "../bit_ops.h"
#include "enums.h"

namespace GBC {

class address_bus;

constexpr uint32_t CPU_FREQUENCY = 4194304;
constexpr uint32_t SAMPLE_RATE = 44100;
constexpr size_t AUDIO_BUFFER_SAMPLES = 256;

class APU {
   public:
    explicit APU(address_bus &memory, CgbConfig &config);
    ~APU();

    void execute_cycle();
    void reset();

    void set_channel_mute(size_t channel, bool muted);
    bool is_channel_muted(size_t channel) const;

    void set_master_volume(float volume);
    float get_master_volume() const;

#ifndef __EMSCRIPTEN__
    void enable_audio_export(bool enable);
    void close_audio_export();
#endif

   private:
    struct SquareChannel {
        bool enabled = false;
        bool dacEnabled = false;
        byte duty = 0;
        byte dutyStep = 0;
        byte lengthCounter = 0;
        bool lengthEnabled = false;
        bool skipNextLengthClock = false;

        half timer = 0;

        byte envelopeVolume = 0;
        byte envelopeInitial = 0;
        byte envelopePeriod = 0;
        byte envelopeTimer = 0;
        bool envelopeIncrease = false;

        byte sweepPeriod = 0;
        byte sweepTimer = 0;
        byte sweepShift = 0;
        bool sweepNegate = false;
        half shadowFrequency = 0;
        bool sweepEnabled = false;
    };

    struct WaveChannel {
        bool enabled = false;
        bool dacEnabled = false;
        half lengthCounter = 0;
        bool lengthEnabled = false;
        bool skipNextLengthClock = false;

        half timer = 0;
        byte position = 0;
        byte currentSample = 0;
    };

    struct NoiseChannel {
        bool enabled = false;
        bool dacEnabled = false;
        byte lengthCounter = 0;
        bool lengthEnabled = false;
        bool skipNextLengthClock = false;

        byte envelopeVolume = 0;
        byte envelopeInitial = 0;
        byte envelopePeriod = 0;
        byte envelopeTimer = 0;
        bool envelopeIncrease = false;

        uint32_t timer = 0;
        half lfsr = 0;
    };

    void clock_frame_sequencer();
    void clock_length_units();
    void clock_sweep_unit();
    void clock_envelopes();
    void check_dac_status();

    void tick_square(SquareChannel &channel);
    void tick_wave();
    void tick_noise();

    int8_t sample_square(const SquareChannel &channel) const;
    int8_t sample_wave() const;
    int8_t sample_noise() const;

    void mix_and_output();
    void queue_audio(float left, float right);

    void power_on();
    void power_off();
    void update_status_bits();
    void load_boot_defaults();

   public:
    void flush_audio();
    void trigger_channel(half address);
    void update_length_enable(half address);
    byte read_register(half address) const;
    void write_register(half address, byte value);
    byte read_wave_byte(half address) const;
    void write_wave_byte(half address, byte value);
    bool is_wave_active() const;
    byte get_nr52_status() const;

   private:
    bool masterEnabled = false;

    address_bus &memory;
    CgbConfig &config;

    SquareChannel ch1{};
    SquareChannel ch2{};
    WaveChannel ch3{};
    NoiseChannel ch4{};

    uint32_t frameSequencerCounter = 0;
    byte frameSequencerStep = 0;

    uint64_t sampleAccumulator = 0;
    std::array<float, AUDIO_BUFFER_SAMPLES * 2> sampleBuffer{};
    size_t bufferedSamples = 0;

    float leftCapacitor = 0.0F;
    float rightCapacitor = 0.0F;

    std::array<bool, 4> channelMuted{};

    float masterVolume = 1.0F;
    float fadeLevel = 1.0F;

    bool audioExportEnabled = false;
#ifndef __EMSCRIPTEN__
    std::array<std::ofstream, 4> channelFiles{};
#endif
    uint32_t exportedSampleCount = 0;

    byte read_reg(AudioRegister reg) const;
    void write_reg(AudioRegister reg, byte value);
    std::array<byte, 16> waveRAM{};

    SDL_AudioStream *audioStream = nullptr;
    SDL_AudioSpec audioSpec{};
};

}  // namespace GBC

