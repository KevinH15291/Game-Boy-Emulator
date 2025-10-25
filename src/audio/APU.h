#pragma once

#ifdef __EMSCRIPTEN__
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#else
#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#endif

#include <array>
#include <cstdint>

namespace GBC {

class address_bus;

constexpr uint32_t CPU_FREQUENCY = 4194304;
constexpr uint32_t SAMPLE_RATE = 44100;
constexpr size_t AUDIO_BUFFER_SAMPLES = 1024;

class APU {
   public:
    explicit APU(address_bus &memory);
    ~APU();

    void execute_cycle();
    void reset();

   private:
    struct SquareChannel {
        bool enabled = false;
        bool dacEnabled = false;
        uint8_t duty = 0;
        uint8_t dutyStep = 0;
        uint8_t lengthCounter = 0;
        bool lengthEnabled = false;

        uint16_t frequency = 0;
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

        uint16_t frequency = 0;
        uint16_t timer = 0;
        uint8_t position = 0;
        uint8_t volumeCode = 0;
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

        uint16_t timer = 0;
        uint16_t lfsr = 0x7FFF;
        uint8_t divisorCode = 0;
        uint8_t clockShift = 0;
        bool widthMode = false;
    };

    void clock_frame_sequencer();
    void clock_length_units();
    void clock_sweep_unit();
    void clock_envelopes();

    void tick_square(SquareChannel &channel);
    void tick_wave();
    void tick_noise();

    int8_t sample_square(const SquareChannel &channel) const;
    int8_t sample_wave() const;
    int8_t sample_noise() const;

    void mix_and_output();
    void queue_audio(int16_t left, int16_t right);
    void flush_audio();

    void power_on();
    void power_off();
    void update_status_bits();

    void trigger_channel(uint16_t address);
    void apply_register(uint16_t address, uint8_t value, bool trigger);
    void poll_bus();

    bool masterEnabled = false;

    address_bus &memory;

    SquareChannel ch1{};
    SquareChannel ch2{};
    WaveChannel ch3{};
    NoiseChannel ch4{};

    std::array<uint8_t, 0x30> prevRegs{};

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

    uint32_t frameSequencerCounter = 0;
    uint8_t frameSequencerStep = 0;

    uint64_t sampleAccumulator = 0;
    std::array<int16_t, AUDIO_BUFFER_SAMPLES * 2> sampleBuffer{};
    size_t bufferedSamples = 0;

#ifdef __EMSCRIPTEN__
    SDL_AudioDeviceID audioDevice = 0;
#else
    SDL_AudioStream *audioStream = nullptr;
#endif
    SDL_AudioSpec audioSpec{};
};

}  // namespace GBC

