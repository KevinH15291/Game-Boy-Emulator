#pragma once

#ifdef __EMSCRIPTEN__
#include <SDL.h>
#include <SDL_audio.h>
#else
#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#endif

#include <array>
#include <cstdint>
#include <fstream>

namespace GBC {

class address_bus;

constexpr uint32_t CPU_FREQUENCY = 4194304;
constexpr uint32_t SAMPLE_RATE = 44100;
constexpr size_t AUDIO_BUFFER_SAMPLES = 256;

class APU {
   public:
    explicit APU(address_bus &memory);
    ~APU();

    void execute_cycle();
    void reset();

    void set_channel_mute(size_t channel, bool muted);
    bool is_channel_muted(size_t channel) const;

    void set_master_volume(float volume);
    float get_master_volume() const;

    void enable_audio_export(bool enable);
    void close_audio_export();

   private:
    struct SquareChannel {
        bool enabled = false;
        bool dacEnabled = false;
        uint8_t duty = 0;
        uint8_t dutyStep = 0;
        uint8_t lengthCounter = 0;
        bool lengthEnabled = false;

        uint16_t timer = 0;

        uint8_t envelopeVolume = 0;
        uint8_t envelopeInitial = 0;
        uint8_t envelopePeriod = 0;
        uint8_t envelopeTimer = 0;
        bool envelopeIncrease = false;

        uint8_t sweepPeriod = 0;
        uint8_t sweepTimer = 0;
        uint8_t sweepShift = 0;
        bool sweepNegate = false;
        uint16_t shadowFrequency = 0;
        bool sweepEnabled = false;
    };

    struct WaveChannel {
        bool enabled = false;
        bool dacEnabled = false;
        uint16_t lengthCounter = 0;
        bool lengthEnabled = false;

        uint16_t timer = 0;
        uint8_t position = 0;
        uint8_t currentSample = 0;
    };

    struct NoiseChannel {
        bool enabled = false;
        bool dacEnabled = false;
        uint8_t lengthCounter = 0;
        bool lengthEnabled = false;

        uint8_t envelopeVolume = 0;
        uint8_t envelopeInitial = 0;
        uint8_t envelopePeriod = 0;
        uint8_t envelopeTimer = 0;
        bool envelopeIncrease = false;

        uint32_t timer = 0;
        uint16_t lfsr = 0;
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

   public:
    void flush_audio();
    void trigger_channel(uint16_t address);
    void update_length_enable(uint16_t address);
    bool masterEnabled = false;

    address_bus &memory;

    SquareChannel ch1{};
    SquareChannel ch2{};
    WaveChannel ch3{};
    NoiseChannel ch4{};

    uint32_t frameSequencerCounter = 0;
    uint8_t frameSequencerStep = 0;

    // Square channel timing counter (1048576 Hz = every 4 CPU cycles)
    uint8_t squareClockCounter = 0;
    // Wave channel timing counter (2097152 Hz = every 2 CPU cycles)
    uint8_t waveClockCounter = 0;

    uint64_t sampleAccumulator = 0;
    std::array<float, AUDIO_BUFFER_SAMPLES * 2> sampleBuffer{};
    size_t bufferedSamples = 0;

    float leftCapacitor = 0.0f;
    float rightCapacitor = 0.0f;

    std::array<bool, 4> channelMuted{};

    float masterVolume = 1.0f;

    bool audioExportEnabled = false;
    std::ofstream channelFiles[4];
    uint32_t exportedSampleCount = 0;

   private:
    uint8_t nr10 = 0;
    uint8_t nr11 = 0;
    uint8_t nr12 = 0;
    uint8_t nr13 = 0;
    uint8_t nr14 = 0;
    uint8_t nr21 = 0;
    uint8_t nr22 = 0;
    uint8_t nr23 = 0;
    uint8_t nr24 = 0;
    uint8_t nr30 = 0;
    uint8_t nr31 = 0;
    uint8_t nr32 = 0;
    uint8_t nr33 = 0;
    uint8_t nr34 = 0;
    uint8_t nr41 = 0;
    uint8_t nr42 = 0;
    uint8_t nr43 = 0;
    uint8_t nr44 = 0;
    uint8_t nr50 = 0;
    uint8_t nr51 = 0;
    uint8_t nr52 = 0;
    std::array<uint8_t, 16> waveRAM{};

   public:
    uint8_t read_register(uint16_t address) const;
    void write_register(uint16_t address, uint8_t value);
    uint8_t read_wave_byte(uint16_t address) const;
    void write_wave_byte(uint16_t address, uint8_t value);
    bool is_wave_active() const;
    uint8_t get_nr52_status() const;

#ifdef __EMSCRIPTEN__
    SDL_AudioDeviceID audioDevice = 0;
#else
    SDL_AudioStream *audioStream = nullptr;
#endif
    SDL_AudioSpec audioSpec{};
};

}  // namespace GBC

