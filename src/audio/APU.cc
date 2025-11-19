#include "APU.h"

#ifdef __EMSCRIPTEN__
#include <SDL_audio.h>
#else
#include <SDL3/SDL_audio.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <utility>

#include "../bit_ops.h"
#include "bus.h"

namespace GBC {

byte APU::read_reg(AudioRegister reg) const {
    return memory.IOrange[addr(reg) - addr(MemoryRegion::IO_REGISTERS)];
}

void APU::write_reg(AudioRegister reg, byte value) {
    memory.IOrange[addr(reg) - addr(MemoryRegion::IO_REGISTERS)] = value;
}

namespace {

constexpr std::array<std::array<int8_t, 8>, 4> DUTY_TABLE{{
    {{0, 1, 0, 0, 0, 0, 0, 0}},
    {{0, 1, 1, 0, 0, 0, 0, 0}},
    {{0, 1, 1, 1, 1, 0, 0, 0}},
    {{1, 0, 0, 1, 1, 1, 1, 1}},
}};

constexpr std::array<half, 8> DIVISOR_LOOKUP{8, 16, 32, 48, 64, 80, 96, 112};

static inline half noise_reload(byte s, byte r) {
    if (s >= 14) return 0;
    half base = (r == 0) ? 4 : (8 * r);
    return static_cast<half>(base) << s;
}

static inline bool sweep_would_overflow(half frequency, bool negate,
                                        byte shift) {
    if (shift == 0) return false;
    half delta = frequency >> shift;
    uint32_t candidate = negate ? static_cast<uint32_t>(frequency) - delta
                                : static_cast<uint32_t>(frequency) + delta;
    return candidate > 2047;
}

inline half compute_square_period(half frequency) {
    return (2048 - (frequency & 0x7FF)) * 4;
}

inline half compute_wave_period(half frequency) {
    return (2048 - (frequency & 0x7FF)) * 2;
}

constexpr std::array<std::pair<AudioRegister, byte>, 20> BOOT_REG_DEFAULTS{{
    {AudioRegister::NR10, 0x80}, {AudioRegister::NR11, 0xBF},
    {AudioRegister::NR12, 0xF3}, {AudioRegister::NR13, 0xFF},
    {AudioRegister::NR14, 0xBF}, {AudioRegister::NR21, 0x3F},
    {AudioRegister::NR22, 0x00}, {AudioRegister::NR23, 0xFF},
    {AudioRegister::NR24, 0xBF}, {AudioRegister::NR30, 0x7F},
    {AudioRegister::NR31, 0xFF}, {AudioRegister::NR32, 0x9F},
    {AudioRegister::NR33, 0xFF}, {AudioRegister::NR34, 0xBF},
    {AudioRegister::NR41, 0xFF}, {AudioRegister::NR42, 0x00},
    {AudioRegister::NR43, 0x00}, {AudioRegister::NR44, 0xBF},
    {AudioRegister::NR50, 0x77}, {AudioRegister::NR51, 0xF3},
}};

inline int8_t apply_volume_code(byte sample, byte volumeCode) {
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

#ifndef __EMSCRIPTEN__
void write_wav_header(std::ofstream &file, uint32_t sampleCount) {
    file.write("RIFF", 4);
    uint32_t fileSize = 36 + sampleCount * 2;
    file.write(reinterpret_cast<const char *>(&fileSize), 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    uint32_t fmtSize = 16;
    file.write(reinterpret_cast<const char *>(&fmtSize), 4);
    half audioFormat = 1;
    file.write(reinterpret_cast<const char *>(&audioFormat), 2);
    half numChannels = 1;
    file.write(reinterpret_cast<const char *>(&numChannels), 2);
    uint32_t sampleRate = 44100;
    file.write(reinterpret_cast<const char *>(&sampleRate), 4);
    uint32_t byteRate = 44100 * 2;
    file.write(reinterpret_cast<const char *>(&byteRate), 4);
    half blockAlign = 2;
    file.write(reinterpret_cast<const char *>(&blockAlign), 2);
    half bitsPerSample = 16;
    file.write(reinterpret_cast<const char *>(&bitsPerSample), 2);
    file.write("data", 4);
    uint32_t dataSize = sampleCount * 2;
    file.write(reinterpret_cast<const char *>(&dataSize), 4);
}
#endif

}  // namespace

APU::APU(address_bus &memory, CgbConfig &config)
    : memory(memory), config(config) {
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
#ifndef __EMSCRIPTEN__
    close_audio_export();
#endif
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
    load_boot_defaults();
    power_on();
}

void APU::trigger_channel(half address) {
    if (!masterEnabled) {
        return;
    }

    switch (address) {
        case addr(AudioRegister::NR14): {
            byte nr14_val = read_reg(AudioRegister::NR14);
            if ((nr14_val & 0x80) == 0) {
                break;
            }
            byte nr11_val = read_reg(AudioRegister::NR11);
            byte nr12_val = read_reg(AudioRegister::NR12);
            byte nr10_val = read_reg(AudioRegister::NR10);
            byte nr13_val = read_reg(AudioRegister::NR13);
            ch1.duty = nr11_val >> 6;
            ch1.lengthCounter = 64 - (nr11_val & 0x3F);
            ch1.dacEnabled = (nr12_val & 0xF8) != 0;
            if (!ch1.dacEnabled) {
                ch1.enabled = false;
                update_status_bits();
                break;
            }
            ch1.envelopeInitial = getBitRange(nr12_val, 4, 4);
            ch1.envelopeVolume = ch1.envelopeInitial;
            ch1.envelopeIncrease = isBitSet(nr12_val, 3);
            ch1.envelopePeriod = getBitRange(nr12_val, 0, 3);
            ch1.lengthEnabled = isBitSet(nr14_val, 6);
            ch1.sweepPeriod = getBitRange(nr10_val, 4, 3);
            ch1.sweepNegate = isBitSet(nr10_val, 3);
            ch1.sweepShift = getBitRange(nr10_val, 0, 3);
            ch1.sweepEnabled = (ch1.sweepPeriod != 0 || ch1.sweepShift != 0);

            if (ch1.lengthCounter == 0) ch1.lengthCounter = 64;
            ch1.timer =
                compute_square_period(nr13_val | ((nr14_val & 0x07) << 8));
            ch1.dutyStep = 0;
            ch1.enabled = true;
            ch1.skipNextLengthClock = (frameSequencerStep & 1) != 0;
            ch1.envelopeTimer = ch1.envelopePeriod;
            ch1.shadowFrequency = nr13_val | ((nr14_val & 0x07) << 8);
            byte effectivePeriod = ch1.sweepPeriod == 0 ? 8 : ch1.sweepPeriod;
            ch1.sweepTimer = effectivePeriod;
            byte nextStep = (frameSequencerStep + 1) & 7;
            if (nextStep == 2 || nextStep == 6) {
                ch1.sweepTimer--;
            }
            if (ch1.sweepShift != 0 && ch1.sweepPeriod != 0) {
                if (sweep_would_overflow(ch1.shadowFrequency, ch1.sweepNegate,
                                         ch1.sweepShift)) {
                    ch1.enabled = false;
                    update_status_bits();
                }
            }
            break;
        }
        case addr(AudioRegister::NR24): {
            byte nr24_val = read_reg(AudioRegister::NR24);
            if ((nr24_val & 0x80) == 0) {
                break;
            }
            byte nr21_val = read_reg(AudioRegister::NR21);
            byte nr22_val = read_reg(AudioRegister::NR22);
            byte nr23_val = read_reg(AudioRegister::NR23);
            ch2.duty = nr21_val >> 6;
            ch2.lengthCounter = 64 - (nr21_val & 0x3F);
            ch2.dacEnabled = (nr22_val & 0xF8) != 0;
            if (!ch2.dacEnabled) {
                ch2.enabled = false;
                update_status_bits();
                break;
            }
            ch2.envelopeInitial = getBitRange(nr22_val, 4, 4);
            ch2.envelopeVolume = ch2.envelopeInitial;
            ch2.envelopeIncrease = isBitSet(nr22_val, 3);
            ch2.envelopePeriod = getBitRange(nr22_val, 0, 3);
            ch2.lengthEnabled = isBitSet(nr24_val, 6);

            if (ch2.lengthCounter == 0) ch2.lengthCounter = 64;
            ch2.timer =
                compute_square_period(nr23_val | ((nr24_val & 0x07) << 8));
            ch2.dutyStep = 0;
            ch2.enabled = true;
            ch2.skipNextLengthClock = (frameSequencerStep & 1) != 0;
            ch2.envelopeTimer = ch2.envelopePeriod;
            break;
        }
        case addr(AudioRegister::NR34): {
            byte nr34_val = read_reg(AudioRegister::NR34);
            if ((nr34_val & 0x80) == 0) {
                break;
            }
            byte nr31_val = read_reg(AudioRegister::NR31);
            byte nr30_val = read_reg(AudioRegister::NR30);
            byte nr33_val = read_reg(AudioRegister::NR33);
            ch3.lengthCounter = 256 - nr31_val;
            ch3.dacEnabled = (nr30_val & 0x80) != 0;
            if (!ch3.dacEnabled) {
                ch3.enabled = false;
                update_status_bits();
                break;
            }
            ch3.lengthEnabled = isBitSet(nr34_val, 6);
            if (ch3.lengthCounter == 0) ch3.lengthCounter = 256;
            ch3.timer =
                compute_wave_period(nr33_val | ((nr34_val & 0x07) << 8));
            ch3.position = 0;
            byte waveByte = waveRAM[0];
            ch3.currentSample = waveByte >> 4;
            ch3.enabled = true;
            ch3.skipNextLengthClock = (frameSequencerStep & 1) != 0;
            break;
        }
        case addr(AudioRegister::NR44): {
            byte nr44_val = read_reg(AudioRegister::NR44);
            if ((nr44_val & 0x80) == 0) {
                break;
            }
            byte nr41_val = read_reg(AudioRegister::NR41);
            byte nr42_val = read_reg(AudioRegister::NR42);
            byte nr43_val = read_reg(AudioRegister::NR43);
            ch4.lengthCounter = 64 - (nr41_val & 0x3F);
            ch4.dacEnabled = (nr42_val & 0xF8) != 0;
            if (!ch4.dacEnabled) {
                ch4.enabled = false;
                update_status_bits();
                break;
            }
            ch4.envelopeInitial = getBitRange(nr42_val, 4, 4);
            ch4.envelopeVolume = ch4.envelopeInitial;
            ch4.envelopeIncrease = isBitSet(nr42_val, 3);
            ch4.envelopePeriod = getBitRange(nr42_val, 0, 3);
            ch4.lengthEnabled = isBitSet(nr44_val, 6);

            byte clockShift = getBitRange(nr43_val, 4, 4);
            byte divisorCode = getBitRange(nr43_val, 0, 3);

            if (ch4.lengthCounter == 0) ch4.lengthCounter = 64;
            ch4.enabled = true;
            ch4.skipNextLengthClock = (frameSequencerStep & 1) != 0;
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

void APU::update_length_enable(half address) {
    auto handle_length_enable = [this](auto &channel, bool newLengthEnabled) {
        bool wasEnabled = channel.lengthEnabled;
        channel.lengthEnabled = newLengthEnabled;

        if (!wasEnabled && newLengthEnabled && channel.enabled) {
            if ((frameSequencerStep & 1) == 0) {
                if (channel.lengthCounter > 0) {
                    if (--channel.lengthCounter == 0) {
                        channel.enabled = false;
                    }
                }
            }
        }
    };

    switch (address) {
        case addr(AudioRegister::NR14): {
            handle_length_enable(ch1,
                                 (read_reg(AudioRegister::NR14) & 0x40) != 0);
            break;
        }
        case addr(AudioRegister::NR24): {
            handle_length_enable(ch2,
                                 (read_reg(AudioRegister::NR24) & 0x40) != 0);
            break;
        }
        case addr(AudioRegister::NR34): {
            handle_length_enable(ch3,
                                 (read_reg(AudioRegister::NR34) & 0x40) != 0);
            break;
        }
        case addr(AudioRegister::NR44): {
            handle_length_enable(ch4,
                                 (read_reg(AudioRegister::NR44) & 0x40) != 0);
            break;
        }
        default:
            break;
    }
}

void APU::execute_cycle() {
    bool newMasterEnabled = (read_reg(AudioRegister::NR52) & 0x80) != 0;

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

    sampleAccumulator += (1ULL << 16);
    constexpr uint64_t CYCLES_PER_SAMPLE_FP =
        ((uint64_t)CPU_FREQUENCY << 16) / SAMPLE_RATE;
    if (sampleAccumulator >= CYCLES_PER_SAMPLE_FP) {
        sampleAccumulator -= CYCLES_PER_SAMPLE_FP;
        mix_and_output();
    }
}

void APU::clock_frame_sequencer() {
    frameSequencerStep = (frameSequencerStep + 1) & 7;

    check_dac_status();

    if ((frameSequencerStep & 1) == 0) {
        clock_length_units();
    }

    if (frameSequencerStep == 2 || frameSequencerStep == 6) {
        clock_sweep_unit();
    }

    if (frameSequencerStep == 7) {
        clock_envelopes();
    }
}

void APU::clock_length_units() {
    auto decrement_length = [this](auto &channel, bool lengthEnabled) {
        if (!lengthEnabled) return;
        if (channel.skipNextLengthClock) {
            channel.skipNextLengthClock = false;
            return;
        }
        if (channel.lengthCounter > 0) {
            if (--channel.lengthCounter == 0) {
                channel.enabled = false;
            }
        }
    };

    decrement_length(ch1, ch1.lengthEnabled);
    decrement_length(ch2, ch2.lengthEnabled);
    decrement_length(ch3, ch3.lengthEnabled);
    decrement_length(ch4, ch4.lengthEnabled);
}

void APU::clock_sweep_unit() {
    byte nr10_val = read_reg(AudioRegister::NR10);
    byte sweepPeriod = getBitRange(nr10_val, 4, 3);
    byte sweepShift = getBitRange(nr10_val, 0, 3);
    bool sweepEnabled = (sweepPeriod != 0 || sweepShift != 0);

    if (!sweepEnabled) return;

    byte effectivePeriod = sweepPeriod == 0 ? 8 : sweepPeriod;
    if (--ch1.sweepTimer == 0) {
        ch1.sweepTimer = effectivePeriod;
        if (sweepPeriod != 0) {
            half delta = ch1.shadowFrequency >> sweepShift;
            bool sweepNegate = (nr10_val & 0x08) != 0;
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
                    half second = ch1.shadowFrequency >> sweepShift;
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
        if ((read_reg(AudioRegister::NR12) & 0xF8) == 0) {
            ch1.enabled = false;
            update_status_bits();
        }
    }

    if (ch2.enabled) {
        if ((read_reg(AudioRegister::NR22) & 0xF8) == 0) {
            ch2.enabled = false;
            update_status_bits();
        }
    }

    if (ch3.enabled) {
        if ((read_reg(AudioRegister::NR30) & 0x80) == 0) {
            ch3.enabled = false;
            update_status_bits();
        }
    }

    if (ch4.enabled) {
        if ((read_reg(AudioRegister::NR42) & 0xF8) == 0) {
            ch4.enabled = false;
            update_status_bits();
        }
    }
}

void APU::clock_envelopes() {
    if (ch1.enabled) {
        byte nr12_val = read_reg(AudioRegister::NR12);
        byte envelopePeriod = nr12_val & 0x07;
        bool envelopeIncrease = (nr12_val & 0x08) != 0;

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
        byte nr22_val = read_reg(AudioRegister::NR22);
        byte envelopePeriod = nr22_val & 0x07;
        bool envelopeIncrease = (nr22_val & 0x08) != 0;

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
        byte nr42_val = read_reg(AudioRegister::NR42);
        byte envelopePeriod = nr42_val & 0x07;
        bool envelopeIncrease = (nr42_val & 0x08) != 0;

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

    half frequency = 0;
    if (&channel == &ch1) {
        frequency = read_reg(AudioRegister::NR13) |
                    ((read_reg(AudioRegister::NR14) & 0x07) << 8);
    } else {
        frequency = read_reg(AudioRegister::NR23) |
                    ((read_reg(AudioRegister::NR24) & 0x07) << 8);
    }

    if (channel.timer == 0) channel.timer = compute_square_period(frequency);

    if (--channel.timer == 0) {
        channel.timer = compute_square_period(frequency);
        channel.dutyStep = (channel.dutyStep + 1) & 7;
    }
}

void APU::tick_wave() {
    if (!ch3.enabled) return;

    half frequency = read_reg(AudioRegister::NR33) |
                     ((read_reg(AudioRegister::NR34) & 0x07) << 8);

    if (ch3.timer > 0) {
        --ch3.timer;
    }

    if (ch3.timer == 0) {
        ch3.timer = compute_wave_period(frequency);
        byte byte = waveRAM[ch3.position >> 1];
        ch3.currentSample = (ch3.position & 1) ? (byte & 0x0F) : (byte >> 4);
        ch3.position = (ch3.position + 1) & 0x1F;
    }
}

void APU::tick_noise() {
    if (!ch4.enabled) return;

    byte nr43_val = read_reg(AudioRegister::NR43);
    byte clockShift = getBitRange(nr43_val, 4, 4);
    bool widthMode = isBitSet(nr43_val, 3);
    byte divisorCode = getBitRange(nr43_val, 0, 3);

    if (clockShift >= 14) {
        return;
    }

    if (ch4.timer == 0) {
        byte bit0 = isBitSet(ch4.lfsr, 0) ? 1 : 0;
        byte bit1 = isBitSet(ch4.lfsr, 1) ? 1 : 0;
        byte newBit = (~(bit0 ^ bit1)) & 1;

        ch4.lfsr = clearBit(ch4.lfsr, 15);
        if (newBit != 0) {
            ch4.lfsr = setBit(ch4.lfsr, 15);
        }

        if (widthMode) {
            ch4.lfsr = clearBit(ch4.lfsr, 7);
            if (newBit != 0) {
                ch4.lfsr = setBit(ch4.lfsr, 7);
            }
        }

        ch4.lfsr >>= 1;

        ch4.timer = noise_reload(clockShift, divisorCode);
        if (ch4.timer == 0) return;
    }

    --ch4.timer;
}

int8_t APU::sample_square(const SquareChannel &channel) const {
    if (!channel.enabled || !channel.dacEnabled) return 0;

    byte duty = 0;
    if (&channel == &ch1) {
        duty = getBitRange(read_reg(AudioRegister::NR11), 6, 2);
    } else {
        duty = getBitRange(read_reg(AudioRegister::NR21), 6, 2);
    }

    byte output =
        DUTY_TABLE[duty][channel.dutyStep] ? channel.envelopeVolume : 0;
    int centered = 15 - (static_cast<int>(output) * 2);
    return static_cast<int8_t>(centered << 2);
}

int8_t APU::sample_wave() const {
    if (!ch3.enabled || !ch3.dacEnabled) return 0;

    byte volumeCode = getBitRange(read_reg(AudioRegister::NR32), 5, 2);

    byte sample = ch3.currentSample & 0x0F;
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
    byte output = (~ch4.lfsr) & 1;
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
        std::array<int16_t, 4> samples = {static_cast<int16_t>(raw_s1 * 256),
                                          static_cast<int16_t>(raw_s2 * 256),
                                          static_cast<int16_t>(raw_s3 * 256),
                                          static_cast<int16_t>(raw_s4 * 256)};
#ifndef __EMSCRIPTEN__
        for (int i = 0; i < 4; ++i) {
            if (channelFiles[i].is_open()) {
                channelFiles[i].write(
                    reinterpret_cast<const char *>(&samples[i]), 2);
            }
        }
#endif
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

    byte nr51_val = read_reg(AudioRegister::NR51);
    byte nr50_val = read_reg(AudioRegister::NR50);
    int leftMix = mix_channel(getBitRange(nr51_val, 0, 4));
    int rightMix = mix_channel(getBitRange(nr51_val, 4, 4));

    float leftVolume =
        static_cast<float>(getBitRange(nr50_val, 4, 3) + 1) / 8.0f;
    float rightVolume =
        static_cast<float>(getBitRange(nr50_val, 0, 3) + 1) / 8.0f;

    float leftIn = static_cast<float>(leftMix) * leftVolume / 240.0f;
    float rightIn = static_cast<float>(rightMix) * rightVolume / 240.0f;

    leftIn = std::max(-1.0f, std::min(1.0f, leftIn));
    rightIn = std::max(-1.0f, std::min(1.0f, rightIn));

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

    queue_audio(leftSample * masterVolume, rightSample * masterVolume);
}

void APU::set_master_volume(float volume) {
    masterVolume = std::max(0.0f, std::min(1.0f, volume));
}

float APU::get_master_volume() const { return masterVolume; }

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
        Uint32 queued = SDL_GetQueuedAudioSize(audioDevice);
        constexpr Uint32 BYTES_PER_BUFFER =
            AUDIO_BUFFER_SAMPLES * sizeof(float) * 2;
        constexpr Uint32 MAX_QUEUED = BYTES_PER_BUFFER * 5;
        if (queued > MAX_QUEUED) {
            bufferedSamples = 0;
            return;
        }
        SDL_QueueAudio(
            audioDevice, sampleBuffer.data(),
            static_cast<Uint32>(bufferedSamples * sizeof(float) * 2));
#else
        int queued = SDL_GetAudioStreamQueued(audioStream);
        constexpr int MAX_QUEUED =
            AUDIO_BUFFER_SAMPLES * sizeof(float) * 2 * 10;
        constexpr int MIN_QUEUED = AUDIO_BUFFER_SAMPLES * sizeof(float) * 2 * 2;
        if (queued < MIN_QUEUED) {
            SDL_PutAudioStreamData(
                audioStream, sampleBuffer.data(),
                static_cast<int>(bufferedSamples * sizeof(float) * 2));
            bufferedSamples = 0;
            return;
        }
        if (queued > MAX_QUEUED) {
            bufferedSamples = 0;
            return;
        }
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

#ifndef __EMSCRIPTEN__
void APU::enable_audio_export(bool enable) {
    if (enable == audioExportEnabled) return;

    if (enable) {
        std::array<const char *, 4> filenames = {
            "channel1_square.wav", "channel2_square.wav", "channel3_wave.wav",
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
    std::cout << "Audio export closed. Exported " << exportedSampleCount
              << " samples per channel." << std::endl;
    audioExportEnabled = false;
}
#endif

void APU::load_boot_defaults() {
    for (const auto &entry : BOOT_REG_DEFAULTS) {
        write_reg(entry.first, entry.second);
    }
}

void APU::power_on() {
    masterEnabled = true;
    const byte nr52_value = config.cgb_mode ? 0xF1 : 0xF0;
    write_reg(AudioRegister::NR52, nr52_value);
    update_status_bits();
}

void APU::power_off() {
    masterEnabled = false;
    ch1 = SquareChannel{};
    ch2 = SquareChannel{};
    ch3 = WaveChannel{};
    ch4 = NoiseChannel{};
    waveRAM.fill(0);

    write_reg(AudioRegister::NR10, 0);
    write_reg(AudioRegister::NR11, 0);
    write_reg(AudioRegister::NR12, 0);
    write_reg(AudioRegister::NR13, 0);
    write_reg(AudioRegister::NR14, 0);
    write_reg(AudioRegister::NR21, 0);
    write_reg(AudioRegister::NR22, 0);
    write_reg(AudioRegister::NR23, 0);
    write_reg(AudioRegister::NR24, 0);
    write_reg(AudioRegister::NR30, 0);
    write_reg(AudioRegister::NR31, 0);
    write_reg(AudioRegister::NR32, 0);
    write_reg(AudioRegister::NR33, 0);
    write_reg(AudioRegister::NR34, 0);
    write_reg(AudioRegister::NR41, 0);
    write_reg(AudioRegister::NR42, 0);
    write_reg(AudioRegister::NR43, 0);
    write_reg(AudioRegister::NR44, 0);
    write_reg(AudioRegister::NR50, 0);
    write_reg(AudioRegister::NR51, 0);
    byte old_nr52 = read_reg(AudioRegister::NR52);
    write_reg(AudioRegister::NR52, old_nr52 & 0x7F);

    update_status_bits();
}

void APU::update_status_bits() {}

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

byte APU::read_register(half address) const {
    byte value = memory.IOrange[address - addr(MemoryRegion::IO_REGISTERS)];

    switch (address) {
        case addr(AudioRegister::NR10):
            return value | 0x80;
        case addr(AudioRegister::NR11):
            return value | 0x3F;
        case addr(AudioRegister::NR12):
            return value;
        case addr(AudioRegister::NR13):
            return 0xFF;
        case addr(AudioRegister::NR14):
            return value | 0xBF;
        case addr(AudioRegister::NR21):
            return value | 0x3F;
        case addr(AudioRegister::NR22):
            return value;
        case addr(AudioRegister::NR23):
            return 0xFF;
        case addr(AudioRegister::NR24):
            return value | 0xBF;
        case addr(AudioRegister::NR30):
            return value | 0x7F;
        case addr(AudioRegister::NR31):
            return 0xFF;
        case addr(AudioRegister::NR32):
            return value | 0x9F;
        case addr(AudioRegister::NR33):
            return 0xFF;
        case addr(AudioRegister::NR34):
            return value | 0xBF;
        case addr(AudioRegister::NR41):
            return 0xFF;
        case addr(AudioRegister::NR42):
            return value;
        case addr(AudioRegister::NR43):
            return value;
        case addr(AudioRegister::NR44):
            return value | 0xBF;
        case addr(AudioRegister::NR50):
            return value;
        case addr(AudioRegister::NR51):
            return value;
        case addr(AudioRegister::NR52):
            return get_nr52_status();
        default:
            return value;
    }
}

void APU::write_register(half address, byte value) {
    if (!masterEnabled && address != addr(AudioRegister::NR52) &&
        address != addr(AudioRegister::NR41)) {
        return;
    }

    switch (address) {
        case addr(AudioRegister::NR10):
        case addr(AudioRegister::NR12):
        case addr(AudioRegister::NR13):
        case addr(AudioRegister::NR22):
        case addr(AudioRegister::NR23):
        case addr(AudioRegister::NR30):
        case addr(AudioRegister::NR32):
        case addr(AudioRegister::NR33):
        case addr(AudioRegister::NR42):
        case addr(AudioRegister::NR43):
        case addr(AudioRegister::NR50):
        case addr(AudioRegister::NR51):
            write_reg(static_cast<AudioRegister>(address), value);
            break;
        case addr(AudioRegister::NR11): {
            write_reg(AudioRegister::NR11, value);
            if (masterEnabled && ch1.enabled && ch1.lengthEnabled) {
                byte newLength = 64 - (value & 0x3F);
                if (newLength == 0) newLength = 64;
                ch1.lengthCounter = newLength;
                if ((frameSequencerStep & 1) == 0) {
                    if (ch1.lengthCounter > 0) {
                        if (--ch1.lengthCounter == 0) {
                            ch1.enabled = false;
                        }
                    }
                }
            }
            break;
        }
        case addr(AudioRegister::NR21): {
            write_reg(AudioRegister::NR21, value);
            if (masterEnabled && ch2.enabled && ch2.lengthEnabled) {
                byte newLength = 64 - (value & 0x3F);
                if (newLength == 0) newLength = 64;
                ch2.lengthCounter = newLength;
                if ((frameSequencerStep & 1) == 0) {
                    if (ch2.lengthCounter > 0) {
                        if (--ch2.lengthCounter == 0) {
                            ch2.enabled = false;
                        }
                    }
                }
            }
            break;
        }
        case addr(AudioRegister::NR31): {
            write_reg(AudioRegister::NR31, value);
            if (masterEnabled && ch3.enabled && ch3.lengthEnabled) {
                half newLength = static_cast<half>(256 - value);
                if (newLength == 0) newLength = 256;
                ch3.lengthCounter = newLength;
                if ((frameSequencerStep & 1) == 0) {
                    if (ch3.lengthCounter > 0) {
                        if (--ch3.lengthCounter == 0) {
                            ch3.enabled = false;
                        }
                    }
                }
            }
            break;
        }
        case addr(AudioRegister::NR41): {
            write_reg(AudioRegister::NR41, value);
            if (masterEnabled && ch4.enabled && ch4.lengthEnabled) {
                byte newLength = 64 - (value & 0x3F);
                if (newLength == 0) newLength = 64;
                ch4.lengthCounter = newLength;
                if ((frameSequencerStep & 1) == 0) {
                    if (ch4.lengthCounter > 0) {
                        if (--ch4.lengthCounter == 0) {
                            ch4.enabled = false;
                        }
                    }
                }
            }
            break;
        }
        case addr(AudioRegister::NR14):
            write_reg(AudioRegister::NR14, value);
            update_length_enable(address);
            if (masterEnabled && (value & 0x80) != 0) {
                trigger_channel(address);
            }
            break;
        case addr(AudioRegister::NR24):
            write_reg(AudioRegister::NR24, value);
            update_length_enable(address);
            if (masterEnabled && (value & 0x80) != 0) {
                trigger_channel(address);
            }
            break;
        case addr(AudioRegister::NR34):
            write_reg(AudioRegister::NR34, value);
            update_length_enable(address);
            if (masterEnabled && (value & 0x80) != 0) {
                trigger_channel(address);
            }
            break;
        case addr(AudioRegister::NR44):
            write_reg(AudioRegister::NR44, value);
            update_length_enable(address);
            if (masterEnabled && (value & 0x80) != 0) {
                trigger_channel(address);
            }
            break;
        case addr(AudioRegister::NR52):
            if ((value & 0x80) == 0) {
                power_off();
            } else {
                if (!masterEnabled) {
                    power_on();
                }
                byte old_nr52 = read_reg(AudioRegister::NR52);
                write_reg(AudioRegister::NR52, (value & 0xFE) | (old_nr52 & 1));
            }
            break;
    }
}

byte APU::read_wave_byte(half address) const {
    if (!masterEnabled) {
        return 0xFF;
    }
    if (address >= 0xFF30 && address <= 0xFF3F) {
        byte index = address - 0xFF30;
        if (ch3.enabled && ch3.dacEnabled) {
            byte byteIndex = ch3.position >> 1;
            if (index == byteIndex) {
                const byte wave_byte = waveRAM[byteIndex];
                byte sample_value;
                if (ch3.position & 1) {
                    sample_value = wave_byte & 0x0F;
                } else {
                    sample_value = wave_byte >> 4;
                }
                return static_cast<byte>((sample_value << 4) | sample_value);
            }
        }
        return waveRAM[index];
    }
    return 0xFF;
}

void APU::write_wave_byte(half address, byte value) {
    if (!masterEnabled) {
        return;
    }

    if (address >= 0xFF30 && address <= 0xFF3F) {
        waveRAM[address - 0xFF30] = value;
    }
}

bool APU::is_wave_active() const { return ch3.enabled && ch3.dacEnabled; }

byte APU::get_nr52_status() const {
    byte status = 0x70;  // Default bits (0x70, not 0x70)
    if (masterEnabled) status |= 0x80;
    if (ch1.enabled) status |= 0x01;
    if (ch2.enabled) status |= 0x02;
    if (ch3.enabled) status |= 0x04;
    if (ch4.enabled) status |= 0x08;
    return status;
}

}  // namespace GBC

