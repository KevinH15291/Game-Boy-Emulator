#include "APU.h"

#ifdef __EMSCRIPTEN__
#include <SDL2/SDL_audio.h>
#else
#include <SDL3/SDL_audio.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>

#include "bus.h"

namespace GBC {

namespace {

constexpr std::array<std::array<int8_t, 8>, 4> DUTY_TABLE{{
    {{0, 1, 0, 0, 0, 0, 0, 0}},
    {{0, 1, 1, 0, 0, 0, 0, 0}},
    {{0, 1, 1, 1, 1, 0, 0, 0}},
    {{1, 0, 0, 1, 1, 1, 1, 1}},
}};

constexpr std::array<uint16_t, 8> DIVISOR_LOOKUP{8,  16, 32, 48,
                                                 64, 80, 96, 112};

static inline uint16_t noise_reload(uint8_t s, uint8_t r) {
    if (s >= 14) return 0;
    uint16_t base = (r == 0) ? 4 : (8 * r);
    return static_cast<uint16_t>(base) << s;
}

static inline bool sweep_would_overflow(uint16_t frequency, bool negate,
                                        uint8_t shift) {
    if (shift == 0) return false;
    uint16_t delta = frequency >> shift;
    uint32_t candidate = negate ? static_cast<uint32_t>(frequency) - delta
                                : static_cast<uint32_t>(frequency) + delta;
    return candidate > 2047;
}

inline uint16_t compute_square_period(uint16_t frequency) {
    return (2048 - (frequency & 0x7FF)) * 4;
}

inline uint16_t compute_wave_period(uint16_t frequency) {
    return (2048 - (frequency & 0x7FF)) * 2;
}

inline int8_t apply_volume_code(uint8_t sample, uint8_t volumeCode) {
    switch (volumeCode) {
        case 0:
            return 0;
        case 1:
            return sample;
        case 2:
            return sample >> 1;
        case 3:
            return sample >> 2;
        default:
            return sample;
    }
}

inline int16_t clamp16(int value) {
    if (value < -32768) return -32768;
    if (value > 32767) return 32767;
    return static_cast<int16_t>(value);
}

void write_wav_header(std::ofstream &file, uint32_t sampleCount) {
    file.write("RIFF", 4);
    uint32_t fileSize = 36 + sampleCount * 2;
    file.write(reinterpret_cast<const char *>(&fileSize), 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    uint32_t fmtSize = 16;
    file.write(reinterpret_cast<const char *>(&fmtSize), 4);
    uint16_t audioFormat = 1;
    file.write(reinterpret_cast<const char *>(&audioFormat), 2);
    uint16_t numChannels = 1;
    file.write(reinterpret_cast<const char *>(&numChannels), 2);
    uint32_t sampleRate = 44100;
    file.write(reinterpret_cast<const char *>(&sampleRate), 4);
    uint32_t byteRate = 44100 * 2;
    file.write(reinterpret_cast<const char *>(&byteRate), 4);
    uint16_t blockAlign = 2;
    file.write(reinterpret_cast<const char *>(&blockAlign), 2);
    uint16_t bitsPerSample = 16;
    file.write(reinterpret_cast<const char *>(&bitsPerSample), 2);
    file.write("data", 4);
    uint32_t dataSize = sampleCount * 2;
    file.write(reinterpret_cast<const char *>(&dataSize), 4);
}

}  // namespace

APU::APU(address_bus &memory) : memory(memory) {
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    std::memset(&audioSpec, 0, sizeof(audioSpec));
#ifdef __EMSCRIPTEN__
    audioSpec.format = AUDIO_F32SYS;
    audioSpec.channels = 2;
    audioSpec.freq = SAMPLE_RATE;
    audioSpec.samples = AUDIO_BUFFER_SAMPLES;
    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &audioSpec, nullptr, 0);
    if (audioDevice != 0) {
        SDL_PauseAudioDevice(audioDevice, 0);
    }
#else
    audioSpec.format = SDL_AUDIO_F32;
    audioSpec.channels = 2;
    audioSpec.freq = SAMPLE_RATE;
    audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                            &audioSpec, nullptr, nullptr);
    if (audioStream != nullptr) {
        SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(audioStream));
    }
#endif
    channelMuted.fill(false);
    reset();
}

APU::~APU() {
    flush_audio();
    close_audio_export();
#ifdef __EMSCRIPTEN__
    if (audioDevice != 0) {
        SDL_CloseAudioDevice(audioDevice);
    }
#else
    if (audioStream != nullptr) {
        SDL_DestroyAudioStream(audioStream);
    }
#endif
}

void APU::reset() {
    masterEnabled = false;
    power_off();

    ch1 = SquareChannel{};
    ch2 = SquareChannel{};
    ch3 = WaveChannel{};
    ch4 = NoiseChannel{};

    frameSequencerCounter = 0;
    frameSequencerStep = 0;
    sampleAccumulator = 0;
    bufferedSamples = 0;
}

void APU::trigger_channel(uint16_t address) {
    switch (address) {
        case addr(AudioRegister::NR14): {
            ch1.duty = nr11 >> 6;
            ch1.lengthCounter = 64 - (nr11 & 0x3F);
            ch1.dacEnabled = (nr12 & 0xF8) != 0;
            ch1.envelopeInitial = (nr12 >> 4) & 0x0F;
            ch1.envelopeVolume = ch1.envelopeInitial;
            ch1.envelopeIncrease = (nr12 & 0x08) != 0;
            ch1.envelopePeriod = nr12 & 0x07;
            ch1.lengthEnabled = (nr14 & 0x40) != 0;
            ch1.sweepPeriod = (nr10 >> 4) & 0x07;
            ch1.sweepNegate = (nr10 & 0x08) != 0;
            ch1.sweepShift = nr10 & 0x07;
            ch1.sweepEnabled = (ch1.sweepPeriod != 0 || ch1.sweepShift != 0);

            if (ch1.lengthCounter == 0) ch1.lengthCounter = 64;
            ch1.timer = compute_square_period(nr13 | ((nr14 & 0x07) << 8));
            ch1.dutyStep = 0;
            ch1.enabled = ch1.dacEnabled;
            ch1.envelopeTimer = ch1.envelopePeriod;
            ch1.shadowFrequency = nr13 | ((nr14 & 0x07) << 8);
            ch1.sweepTimer = ch1.sweepPeriod == 0 ? 8 : ch1.sweepPeriod;
            if (ch1.sweepShift != 0) {
                if (sweep_would_overflow(ch1.shadowFrequency, ch1.sweepNegate,
                                         ch1.sweepShift)) {
                    ch1.enabled = false;
                    update_status_bits();
                }
            }
            break;
        }
        case addr(AudioRegister::NR24): {
            ch2.duty = nr21 >> 6;
            ch2.lengthCounter = 64 - (nr21 & 0x3F);
            ch2.dacEnabled = (nr22 & 0xF8) != 0;
            ch2.envelopeInitial = (nr22 >> 4) & 0x0F;
            ch2.envelopeVolume = ch2.envelopeInitial;
            ch2.envelopeIncrease = (nr22 & 0x08) != 0;
            ch2.envelopePeriod = nr22 & 0x07;
            ch2.lengthEnabled = (nr24 & 0x40) != 0;

            if (ch2.lengthCounter == 0) ch2.lengthCounter = 64;
            ch2.timer = compute_square_period(nr23 | ((nr24 & 0x07) << 8));
            ch2.dutyStep = 0;
            ch2.enabled = ch2.dacEnabled;
            ch2.envelopeTimer = ch2.envelopePeriod;
            break;
        }
        case addr(AudioRegister::NR34): {
            ch3.lengthCounter = 256 - nr31;
            ch3.dacEnabled = (nr30 & 0x80) != 0;
            ch3.lengthEnabled = (nr34 & 0x40) != 0;
            if (ch3.lengthCounter == 0) ch3.lengthCounter = 256;
            ch3.timer = compute_wave_period(nr33 | ((nr34 & 0x07) << 8));
            ch3.position = 0;
            uint8_t waveByte = waveRAM[0];
            ch3.currentSample = waveByte >> 4;
            ch3.enabled = ch3.dacEnabled;
            break;
        }
        case addr(AudioRegister::NR44): {
            ch4.lengthCounter = 64 - (nr41 & 0x3F);
            ch4.dacEnabled = (nr42 & 0xF8) != 0;
            ch4.envelopeInitial = (nr42 >> 4) & 0x0F;
            ch4.envelopeVolume = ch4.envelopeInitial;
            ch4.envelopeIncrease = (nr42 & 0x08) != 0;
            ch4.envelopePeriod = nr42 & 0x07;
            ch4.lengthEnabled = (nr44 & 0x40) != 0;

            uint8_t clockShift = (nr43 >> 4) & 0x0F;
            uint8_t divisorCode = nr43 & 0x07;

            if (ch4.lengthCounter == 0) ch4.lengthCounter = 64;
            ch4.enabled = ch4.dacEnabled;
            ch4.envelopeTimer = ch4.envelopePeriod;
            ch4.lfsr = 0;
            {
                ch4.timer = noise_reload(clockShift, divisorCode);
                if (ch4.timer == 0) ch4.timer = 1;
            }
            break;
        }
        default:
            break;
    }
    update_status_bits();
}

void APU::update_length_enable(uint16_t address) {
    switch (address) {
        case addr(AudioRegister::NR14): {
            ch1.lengthEnabled = (nr14 & 0x40) != 0;
            break;
        }
        case addr(AudioRegister::NR24): {
            ch2.lengthEnabled = (nr24 & 0x40) != 0;
            break;
        }
        case addr(AudioRegister::NR34): {
            ch3.lengthEnabled = (nr34 & 0x40) != 0;
            break;
        }
        case addr(AudioRegister::NR44): {
            ch4.lengthEnabled = (nr44 & 0x40) != 0;
            break;
        }
        default:
            break;
    }
}

void APU::execute_cycle() {
    bool newMasterEnabled = (nr52 & 0x80) != 0;

    if (newMasterEnabled != masterEnabled) {
        masterEnabled = newMasterEnabled;
        update_status_bits();
    }

    if (!masterEnabled) {
        return;
    }

    frameSequencerCounter += 1;
    if (frameSequencerCounter >= CPU_FREQUENCY / 512) {
        frameSequencerCounter = 0;
        clock_frame_sequencer();
    }

    tick_square(ch1);
    tick_square(ch2);
    tick_wave();
    tick_noise();

    sampleAccumulator += SAMPLE_RATE;
    if (sampleAccumulator >= CPU_FREQUENCY) {
        sampleAccumulator -= CPU_FREQUENCY;
        mix_and_output();
    }
}

void APU::clock_frame_sequencer() {
    frameSequencerStep = (frameSequencerStep + 1) & 7;

    // Check DAC status every frame sequencer step
    check_dac_status();

    // Length @ 256 Hz on steps 0, 2, 4, 6
    if ((frameSequencerStep & 1) == 0) {
        clock_length_units();
    }

    // Sweep @ 128 Hz on steps 2 and 6
    if (frameSequencerStep == 2 || frameSequencerStep == 6) {
        clock_sweep_unit();
    }

    // Envelope @ 64 Hz on step 7
    if (frameSequencerStep == 7) {
        clock_envelopes();
    }
}

void APU::clock_length_units() {
    auto decrement_length = [this](auto &channel, bool lengthEnabled) {
        if (!lengthEnabled) return;
        if (channel.lengthCounter == 0) return;
        if (--channel.lengthCounter == 0) {
            channel.enabled = false;
            update_status_bits();
        }
    };

    // Use stored length enable flags from channel state
    decrement_length(ch1, ch1.lengthEnabled);
    decrement_length(ch2, ch2.lengthEnabled);
    decrement_length(ch3, ch3.lengthEnabled);
    decrement_length(ch4, ch4.lengthEnabled);
    update_status_bits();
}

void APU::clock_sweep_unit() {
    uint8_t sweepPeriod = (nr10 >> 4) & 0x07;
    uint8_t sweepShift = nr10 & 0x07;
    bool sweepEnabled = (sweepPeriod != 0 || sweepShift != 0);

    if (!sweepEnabled || sweepPeriod == 0) return;

    if (--ch1.sweepTimer == 0) {
        ch1.sweepTimer = sweepPeriod == 0 ? 8 : sweepPeriod;
        if (sweepPeriod) {
            uint16_t delta = ch1.shadowFrequency >> sweepShift;
            bool sweepNegate = (nr10 & 0x08) != 0;
            if (sweepNegate) {
                ch1.shadowFrequency -= delta;
            } else {
                ch1.shadowFrequency += delta;
            }

            if (ch1.shadowFrequency > 2047) {
                ch1.enabled = false;
                update_status_bits();
            } else {
                ch1.timer = compute_square_period(ch1.shadowFrequency & 0x7FF);

                if (sweepShift != 0) {
                    uint16_t second = ch1.shadowFrequency >> sweepShift;
                    if (sweepNegate)
                        second = ch1.shadowFrequency - second;
                    else
                        second = ch1.shadowFrequency + second;
                    if (second > 2047) {
                        ch1.enabled = false;
                        update_status_bits();
                    }
                }
            }
        }
    }
}

void APU::check_dac_status() {
    if (ch1.enabled) {
        if ((nr12 & 0xF8) == 0) {
            ch1.enabled = false;
            update_status_bits();
        }
    }

    if (ch2.enabled) {
        if ((nr22 & 0xF8) == 0) {
            ch2.enabled = false;
            update_status_bits();
        }
    }

    if (ch3.enabled) {
        if ((nr30 & 0x80) == 0) {
            ch3.enabled = false;
            update_status_bits();
        }
    }

    if (ch4.enabled) {
        if ((nr42 & 0xF8) == 0) {
            ch4.enabled = false;
            update_status_bits();
        }
    }
}

void APU::clock_envelopes() {
    if (ch1.enabled) {
        uint8_t envelopePeriod = nr12 & 0x07;
        bool envelopeIncrease = (nr12 & 0x08) != 0;

        if (envelopePeriod != 0) {
            if (--ch1.envelopeTimer == 0) {
                ch1.envelopeTimer = envelopePeriod;

                if (envelopeIncrease) {
                    if (ch1.envelopeVolume < 15) {
                        ++ch1.envelopeVolume;
                    }
                } else {
                    if (ch1.envelopeVolume > 0) {
                        --ch1.envelopeVolume;
                    }
                }
            }
        }
    }

    if (ch2.enabled) {
        uint8_t envelopePeriod = nr22 & 0x07;
        bool envelopeIncrease = (nr22 & 0x08) != 0;

        if (envelopePeriod != 0) {
            if (--ch2.envelopeTimer == 0) {
                ch2.envelopeTimer = envelopePeriod;

                if (envelopeIncrease) {
                    if (ch2.envelopeVolume < 15) {
                        ++ch2.envelopeVolume;
                    }
                } else {
                    if (ch2.envelopeVolume > 0) {
                        --ch2.envelopeVolume;
                    }
                }
            }
        }
    }

    if (ch4.enabled) {
        uint8_t envelopePeriod = nr42 & 0x07;
        bool envelopeIncrease = (nr42 & 0x08) != 0;

        if (envelopePeriod != 0) {
            if (--ch4.envelopeTimer == 0) {
                ch4.envelopeTimer = envelopePeriod;

                if (envelopeIncrease) {
                    if (ch4.envelopeVolume < 15) {
                        ++ch4.envelopeVolume;
                    }
                } else {
                    if (ch4.envelopeVolume > 0) {
                        --ch4.envelopeVolume;
                    }
                }
            }
        }
    }
}

void APU::tick_square(SquareChannel &channel) {
    if (!channel.enabled) return;

    uint16_t frequency = 0;
    if (&channel == &ch1) {
        frequency = nr13 | ((nr14 & 0x07) << 8);
    } else {
        frequency = nr23 | ((nr24 & 0x07) << 8);
    }

    if (channel.timer == 0) channel.timer = compute_square_period(frequency);

    if (--channel.timer == 0) {
        channel.timer = compute_square_period(frequency);
        channel.dutyStep = (channel.dutyStep + 1) & 7;
    }
}

void APU::tick_wave() {
    if (!ch3.enabled) return;

    uint16_t frequency = nr33 | ((nr34 & 0x07) << 8);

    if (ch3.timer > 0) {
        --ch3.timer;
    }

    if (ch3.timer == 0) {
        ch3.timer = compute_wave_period(frequency);
        uint8_t byte = waveRAM[ch3.position >> 1];
        ch3.currentSample = (ch3.position & 1) ? (byte & 0x0F) : (byte >> 4);
        ch3.position = (ch3.position + 1) & 0x1F;
    }
}

void APU::tick_noise() {
    if (!ch4.enabled) return;

    uint8_t clockShift = (nr43 >> 4) & 0x0F;
    bool widthMode = (nr43 & 0x08) != 0;
    uint8_t divisorCode = nr43 & 0x07;

    if (clockShift >= 14) {
        return;
    }

    if (ch4.timer == 0) {
        uint8_t bit0 = ch4.lfsr & 1;
        uint8_t bit1 = (ch4.lfsr >> 1) & 1;
        uint8_t newBit = (~(bit0 ^ bit1)) & 1;

        ch4.lfsr &= ~(1 << 15);
        ch4.lfsr |= static_cast<uint16_t>(newBit) << 15;

        if (widthMode) {
            ch4.lfsr &= ~(1 << 7);
            ch4.lfsr |= static_cast<uint16_t>(newBit) << 7;
        }

        ch4.lfsr >>= 1;

        ch4.timer = noise_reload(clockShift, divisorCode);
        if (ch4.timer == 0) return;
    }

    --ch4.timer;
}

int8_t APU::sample_square(const SquareChannel &channel) const {
    if (!channel.enabled || !channel.dacEnabled) return 0;

    uint8_t duty = 0;
    if (&channel == &ch1) {
        duty = (nr11 >> 6) & 0x03;
    } else {
        duty = (nr21 >> 6) & 0x03;
    }

    uint8_t output =
        DUTY_TABLE[duty][channel.dutyStep] ? channel.envelopeVolume : 0;
    int centered = 15 - (static_cast<int>(output) * 2);
    return static_cast<int8_t>(centered << 2);
}

int8_t APU::sample_wave() const {
    if (!ch3.enabled || !ch3.dacEnabled) return 0;

    uint8_t volumeCode = (nr32 >> 5) & 0x03;

    uint8_t sample = ch3.currentSample & 0x0F;
    switch (volumeCode) {
        case 0:
            return 0;
        case 2:
            sample >>= 1;
            break;
        case 3:
            sample >>= 2;
            break;
        default:
            break;
    }

    int centered = 15 - (static_cast<int>(sample) * 2);
    return static_cast<int8_t>(centered << 2);
}

int8_t APU::sample_noise() const {
    if (!ch4.enabled || !ch4.dacEnabled) return 0;
    uint8_t output = (~ch4.lfsr) & 1;
    int value = output ? ch4.envelopeVolume : 0;
    int centered = 15 - (static_cast<int>(value) * 2);
    return static_cast<int8_t>(centered << 2);
}

void APU::mix_and_output() {
    int8_t raw_s1 = sample_square(ch1);
    int8_t raw_s2 = sample_square(ch2);
    int8_t raw_s3 = sample_wave();
    int8_t raw_s4 = sample_noise();

    if (audioExportEnabled) {
        int16_t samples[4] = {static_cast<int16_t>(raw_s1 * 256),
                              static_cast<int16_t>(raw_s2 * 256),
                              static_cast<int16_t>(raw_s3 * 256),
                              static_cast<int16_t>(raw_s4 * 256)};
        for (int i = 0; i < 4; ++i) {
            if (channelFiles[i].is_open()) {
                channelFiles[i].write(
                    reinterpret_cast<const char *>(&samples[i]), 2);
            }
        }
        ++exportedSampleCount;
    }

    int8_t s1 = channelMuted[0] ? 0 : raw_s1;
    int8_t s2 = channelMuted[1] ? 0 : raw_s2;
    int8_t s3 = channelMuted[2] ? 0 : raw_s3;
    int8_t s4 = channelMuted[3] ? 0 : raw_s4;

    auto mix_channel = [&](int channelMask) {
        int mix = 0;
        if (channelMask & 0x01) mix += static_cast<int>(s1);
        if (channelMask & 0x02) mix += static_cast<int>(s2);
        if (channelMask & 0x04) mix += static_cast<int>(s3);
        if (channelMask & 0x08) mix += static_cast<int>(s4);
        return mix;
    };

    int leftMix = mix_channel(nr51 & 0x0F);
    int rightMix = mix_channel((nr51 >> 4) & 0x0F);

    float leftVolume = static_cast<float>(((nr50 >> 4) & 0x07) + 1) / 8.0f;
    float rightVolume = static_cast<float>((nr50 & 0x07) + 1) / 8.0f;

    float leftIn = static_cast<float>(leftMix) * leftVolume / 240.0f;
    float rightIn = static_cast<float>(rightMix) * rightVolume / 240.0f;

    bool dacsEnabled =
        ch1.dacEnabled || ch2.dacEnabled || ch3.dacEnabled || ch4.dacEnabled;

    float leftSample, rightSample;
    if (dacsEnabled) {
        leftSample = leftIn - leftCapacitor;
        leftCapacitor = leftIn - (leftSample * 0.998943f);
        rightSample = rightIn - rightCapacitor;
        rightCapacitor = rightIn - (rightSample * 0.998943f);
    } else {
        leftSample = 0.0f;
        rightSample = 0.0f;
        leftCapacitor = 0.0f;
        rightCapacitor = 0.0f;
    }

    leftSample = std::max(-1.0f, std::min(1.0f, leftSample));
    rightSample = std::max(-1.0f, std::min(1.0f, rightSample));

    queue_audio(leftSample, rightSample);
}

void APU::queue_audio(float left, float right) {
#ifdef __EMSCRIPTEN__
    if (audioDevice == 0) return;
#else
    if (audioStream == nullptr) return;
#endif
    sampleBuffer[bufferedSamples * 2] = left;
    sampleBuffer[bufferedSamples * 2 + 1] = right;
    ++bufferedSamples;

    if (bufferedSamples >= AUDIO_BUFFER_SAMPLES) {
#ifdef __EMSCRIPTEN__
        SDL_QueueAudio(
            audioDevice, sampleBuffer.data(),
            static_cast<Uint32>(bufferedSamples * sizeof(float) * 2));
#else
        SDL_PutAudioStreamData(
            audioStream, sampleBuffer.data(),
            static_cast<int>(bufferedSamples * sizeof(float) * 2));
#endif
        bufferedSamples = 0;
    }
}

void APU::flush_audio() {
#ifdef __EMSCRIPTEN__
    if (audioDevice == 0 || bufferedSamples == 0) return;
    SDL_QueueAudio(audioDevice, sampleBuffer.data(),
                   static_cast<Uint32>(bufferedSamples * sizeof(float) * 2));
#else
    if (audioStream == nullptr || bufferedSamples == 0) return;
    SDL_PutAudioStreamData(
        audioStream, sampleBuffer.data(),
        static_cast<int>(bufferedSamples * sizeof(float) * 2));
#endif
    bufferedSamples = 0;
}

void APU::enable_audio_export(bool enable) {
    if (enable == audioExportEnabled) return;

    if (enable) {
        const char *filenames[4] = {"channel1_square.wav",
                                    "channel2_square.wav", "channel3_wave.wav",
                                    "channel4_noise.wav"};
        for (int i = 0; i < 4; ++i) {
            channelFiles[i].open(filenames[i], std::ios::binary);
            if (channelFiles[i].is_open()) {
                write_wav_header(channelFiles[i], 0);
            } else {
                std::cerr << "Failed to open " << filenames[i] << " for writing"
                          << std::endl;
            }
        }
        exportedSampleCount = 0;
        audioExportEnabled = true;
        std::cout
            << "Audio export enabled. Writing to channel1_square.wav, "
               "channel2_square.wav, channel3_wave.wav, channel4_noise.wav"
            << std::endl;
    } else {
        close_audio_export();
    }
}

void APU::close_audio_export() {
    if (!audioExportEnabled) return;

    for (int i = 0; i < 4; ++i) {
        if (channelFiles[i].is_open()) {
            channelFiles[i].seekp(0, std::ios::beg);
            write_wav_header(channelFiles[i], exportedSampleCount);
            channelFiles[i].close();
        }
    }
    audioExportEnabled = false;
    std::cout << "Audio export closed. Exported " << exportedSampleCount
              << " samples per channel." << std::endl;
}

void APU::power_on() {
    masterEnabled = true;
    update_status_bits();
}

void APU::power_off() {
    masterEnabled = false;
    ch1 = SquareChannel{};
    ch2 = SquareChannel{};
    ch3 = WaveChannel{};
    ch4 = NoiseChannel{};
    update_status_bits();
}

void APU::update_status_bits() {
    // Status bits are now handled by the bus via get_nr52_status()
    // This method is kept for compatibility but does nothing
}

void APU::set_channel_mute(size_t channel, bool muted) {
    if (channel < channelMuted.size()) {
        channelMuted[channel] = muted;
    }
}

bool APU::is_channel_muted(size_t channel) const {
    if (channel < channelMuted.size()) {
        return channelMuted[channel];
    }
    return false;
}

uint8_t APU::read_register(uint16_t address) const {
    switch (address) {
        case addr(AudioRegister::NR10):
            return nr10;
        case addr(AudioRegister::NR11):
            return nr11;
        case addr(AudioRegister::NR12):
            return nr12;
        case addr(AudioRegister::NR13):
            return nr13;
        case addr(AudioRegister::NR14):
            return nr14;
        case addr(AudioRegister::NR21):
            return nr21;
        case addr(AudioRegister::NR22):
            return nr22;
        case addr(AudioRegister::NR23):
            return nr23;
        case addr(AudioRegister::NR24):
            return nr24;
        case addr(AudioRegister::NR30):
            return nr30;
        case addr(AudioRegister::NR31):
            return nr31;
        case addr(AudioRegister::NR32):
            return nr32;
        case addr(AudioRegister::NR33):
            return nr33;
        case addr(AudioRegister::NR34):
            return nr34;
        case addr(AudioRegister::NR41):
            return nr41;
        case addr(AudioRegister::NR42):
            return nr42;
        case addr(AudioRegister::NR43):
            return nr43;
        case addr(AudioRegister::NR44):
            return nr44;
        case addr(AudioRegister::NR50):
            return nr50;
        case addr(AudioRegister::NR51):
            return nr51;
        case addr(AudioRegister::NR52):
            return get_nr52_status();
        default:
            return 0xFF;
    }
}

void APU::write_register(uint16_t address, uint8_t value) {
    switch (address) {
        case addr(AudioRegister::NR10):
            nr10 = value;
            break;
        case addr(AudioRegister::NR11):
            nr11 = value;
            break;
        case addr(AudioRegister::NR12):
            nr12 = value;
            break;
        case addr(AudioRegister::NR13):
            nr13 = value;
            break;
        case addr(AudioRegister::NR14):
            nr14 = value;
            trigger_channel(address);
            break;
        case addr(AudioRegister::NR21):
            nr21 = value;
            break;
        case addr(AudioRegister::NR22):
            nr22 = value;
            break;
        case addr(AudioRegister::NR23):
            nr23 = value;
            break;
        case addr(AudioRegister::NR24):
            nr24 = value;
            trigger_channel(address);
            break;
        case addr(AudioRegister::NR30):
            nr30 = value;
            break;
        case addr(AudioRegister::NR31):
            nr31 = value;
            break;
        case addr(AudioRegister::NR32):
            nr32 = value;
            break;
        case addr(AudioRegister::NR33):
            nr33 = value;
            break;
        case addr(AudioRegister::NR34):
            nr34 = value;
            trigger_channel(address);
            break;
        case addr(AudioRegister::NR41):
            nr41 = value;
            break;
        case addr(AudioRegister::NR42):
            nr42 = value;
            break;
        case addr(AudioRegister::NR43):
            nr43 = value;
            break;
        case addr(AudioRegister::NR44):
            nr44 = value;
            trigger_channel(address);
            break;
        case addr(AudioRegister::NR50):
            nr50 = value;
            break;
        case addr(AudioRegister::NR51):
            nr51 = value;
            break;
        case addr(AudioRegister::NR52):
            nr52 = (value & 0xFE) | (nr52 & 1);
            if ((value & 0x80) == 0) {
                power_off();
            } else {
                power_on();
            }
            break;
    }
}

uint8_t APU::read_wave_byte(uint16_t address) const {
    if (address >= 0xFF30 && address <= 0xFF3F) {
        return waveRAM[address - 0xFF30];
    }
    return 0xFF;
}

void APU::write_wave_byte(uint16_t address, uint8_t value) {
    if (address >= 0xFF30 && address <= 0xFF3F) {
        waveRAM[address - 0xFF30] = value;
    }
}

bool APU::is_wave_active() const { return ch3.enabled && ch3.dacEnabled; }

uint8_t APU::get_nr52_status() const {
    uint8_t status = 0x70;  // Default bits (0x70, not 0x70)
    if (masterEnabled) status |= 0x80;
    if (ch1.enabled) status |= 0x01;
    if (ch2.enabled) status |= 0x02;
    if (ch3.enabled) status |= 0x04;
    if (ch4.enabled) status |= 0x08;
    return status;
}

}  // namespace GBC

