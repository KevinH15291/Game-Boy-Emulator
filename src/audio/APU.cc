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

inline uint16_t compute_period(uint16_t frequency) {
    return (2048 - frequency) * 4;
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

}  // namespace

APU::APU(address_bus &memory) : memory(memory) {
#ifdef __EMSCRIPTEN__
    SDL_Init(SDL_INIT_AUDIO);
    SDL_zero(audioSpec);
    audioSpec.format = AUDIO_S16SYS;
    audioSpec.freq = SAMPLE_RATE;
    audioSpec.channels = 2;
    audioSpec.samples = AUDIO_BUFFER_SAMPLES;
    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &audioSpec, nullptr, 0);
    SDL_PauseAudioDevice(audioDevice, 0);
#else
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_zero(audioSpec);
    audioSpec.format = SDL_AUDIO_S16;
    audioSpec.channels = 2;
    audioSpec.freq = SAMPLE_RATE;
    audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                            &audioSpec, nullptr, nullptr);
    if (audioStream != nullptr) {
        SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(audioStream));
    }
#endif

    reset();
}

APU::~APU() {
    flush_audio();
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

    std::fill(prevRegs.begin(), prevRegs.end(), 0);
    std::fill(waveRAM.begin(), waveRAM.end(), 0);

    nr10 = nr11 = nr12 = nr13 = nr14 = 0;
    nr21 = nr22 = nr23 = nr24 = 0;
    nr30 = nr31 = nr32 = nr33 = nr34 = 0;
    nr41 = nr42 = nr43 = nr44 = 0;
    nr50 = nr51 = nr52 = 0;
    frameSequencerCounter = 0;
    frameSequencerStep = 0;
    sampleAccumulator = 0;
    bufferedSamples = 0;
}

void APU::poll_bus() {
    auto update = [&](uint16_t address) {
        uint8_t value = memory.readIO(address);
        uint16_t idx = address - addr(AudioRegister::NR10);
        if (idx < prevRegs.size()) {
            bool trigger =
                ((value & 0x80) != 0) && ((prevRegs[idx] & 0x80) == 0);
            prevRegs[idx] = value;
            apply_register(address, value, trigger);
        }
    };

    update(addr(AudioRegister::NR10));
    update(addr(AudioRegister::NR11));
    update(addr(AudioRegister::NR12));
    update(addr(AudioRegister::NR13));
    update(addr(AudioRegister::NR14));

    update(addr(AudioRegister::NR21));
    update(addr(AudioRegister::NR22));
    update(addr(AudioRegister::NR23));
    update(addr(AudioRegister::NR24));

    update(addr(AudioRegister::NR30));
    update(addr(AudioRegister::NR31));
    update(addr(AudioRegister::NR32));
    update(addr(AudioRegister::NR33));
    update(addr(AudioRegister::NR34));

    update(addr(AudioRegister::NR41));
    update(addr(AudioRegister::NR42));
    update(addr(AudioRegister::NR43));
    update(addr(AudioRegister::NR44));

    update(addr(AudioRegister::NR50));
    update(addr(AudioRegister::NR51));
    update(addr(AudioRegister::NR52));

    for (uint16_t address = 0xFF30; address <= 0xFF3F; ++address) {
        waveRAM[address - 0xFF30] = memory.readIO(address);
    }
}

void APU::apply_register(uint16_t address, uint8_t value, bool trigger) {
    switch (address) {
        case addr(AudioRegister::NR10):
            nr10 = value & 0x7F;
            ch1.sweepPeriod = (value >> 4) & 0x07;
            ch1.sweepNegate = (value & 0x08) != 0;
            ch1.sweepShift = value & 0x07;
            ch1.sweepEnabled = ch1.sweepPeriod || ch1.sweepShift;
            ch1.sweepTimer = ch1.sweepPeriod ? ch1.sweepPeriod : 8;
            break;
        case addr(AudioRegister::NR11):
            nr11 = value;
            ch1.duty = value >> 6;
            ch1.lengthCounter = 64 - (value & 0x3F);
            break;
        case addr(AudioRegister::NR12):
            nr12 = value;
            ch1.dacEnabled = (value & 0xF8) != 0;
            if (!ch1.dacEnabled) ch1.enabled = false;
            ch1.envelopeInitial = (value >> 4) & 0x0F;
            ch1.envelopeVolume = ch1.envelopeInitial;
            ch1.envelopeIncrease = (value & 0x08) != 0;
            ch1.envelopePeriod = value & 0x07;
            break;
        case addr(AudioRegister::NR13):
            nr13 = value;
            ch1.frequency = (ch1.frequency & 0x0700) | value;
            break;
        case addr(AudioRegister::NR14):
            nr14 = value & 0x40;
            ch1.lengthEnabled = (value & 0x40) != 0;
            ch1.frequency = (ch1.frequency & 0x00FF) | ((value & 0x07) << 8);
            break;
        case addr(AudioRegister::NR21):
            nr21 = value;
            ch2.duty = value >> 6;
            ch2.lengthCounter = 64 - (value & 0x3F);
            break;
        case addr(AudioRegister::NR22):
            nr22 = value;
            ch2.dacEnabled = (value & 0xF8) != 0;
            if (!ch2.dacEnabled) ch2.enabled = false;
            ch2.envelopeInitial = (value >> 4) & 0x0F;
            ch2.envelopeVolume = ch2.envelopeInitial;
            ch2.envelopeIncrease = (value & 0x08) != 0;
            ch2.envelopePeriod = value & 0x07;
            break;
        case addr(AudioRegister::NR23):
            nr23 = value;
            ch2.frequency = (ch2.frequency & 0x0700) | value;
            break;
        case addr(AudioRegister::NR24):
            nr24 = value & 0x40;
            ch2.lengthEnabled = (value & 0x40) != 0;
            ch2.frequency = (ch2.frequency & 0x00FF) | ((value & 0x07) << 8);
            break;
        case addr(AudioRegister::NR30):
            nr30 = value | 0x7F;
            ch3.dacEnabled = (value & 0x80) != 0;
            if (!ch3.dacEnabled) ch3.enabled = false;
            break;
        case addr(AudioRegister::NR31):
            nr31 = value;
            ch3.lengthCounter = 256 - value;
            break;
        case addr(AudioRegister::NR32):
            nr32 = value | 0x9F;
            ch3.volumeCode = (value >> 5) & 0x03;
            break;
        case addr(AudioRegister::NR33):
            nr33 = value;
            ch3.frequency = (ch3.frequency & 0x0700) | value;
            break;
        case addr(AudioRegister::NR34):
            nr34 = value & 0x40;
            ch3.lengthEnabled = (value & 0x40) != 0;
            ch3.frequency = (ch3.frequency & 0x00FF) | ((value & 0x07) << 8);
            break;
        case addr(AudioRegister::NR41):
            nr41 = value & 0x3F;
            ch4.lengthCounter = 64 - (value & 0x3F);
            break;
        case addr(AudioRegister::NR42):
            nr42 = value;
            ch4.dacEnabled = (value & 0xF8) != 0;
            if (!ch4.dacEnabled) ch4.enabled = false;
            ch4.envelopeInitial = (value >> 4) & 0x0F;
            ch4.envelopeVolume = ch4.envelopeInitial;
            ch4.envelopeIncrease = (value & 0x08) != 0;
            ch4.envelopePeriod = value & 0x07;
            break;
        case addr(AudioRegister::NR43):
            nr43 = value;
            ch4.clockShift = (value >> 4) & 0x0F;
            ch4.widthMode = (value & 0x08) != 0;
            ch4.divisorCode = value & 0x07;
            break;
        case addr(AudioRegister::NR44):
            nr44 = value & 0x40;
            ch4.lengthEnabled = (value & 0x40) != 0;
            break;
        case addr(AudioRegister::NR50):
            nr50 = value;
            break;
        case addr(AudioRegister::NR51):
            nr51 = value;
            break;
        case addr(AudioRegister::NR52):
            masterEnabled = (value & 0x80) != 0;
            nr52 = (value & 0x80) | 0x70;
            if (!masterEnabled) {
                power_off();
            } else {
                power_on();
            }
            break;
        default:
            if (address >= 0xFF30 && address <= 0xFF3F) {
                waveRAM[address - 0xFF30] = value;
            }
            break;
    }

    if (trigger) {
        trigger_channel(address);
    }
}

void APU::trigger_channel(uint16_t address) {
    switch (address) {
        case addr(AudioRegister::NR14):
            if (ch1.lengthCounter == 0) ch1.lengthCounter = 64;
            ch1.timer = compute_period(ch1.frequency);
            ch1.dutyStep = 0;
            ch1.enabled = ch1.dacEnabled;
            ch1.envelopeVolume = ch1.envelopeInitial;
            ch1.envelopeTimer =
                ch1.envelopePeriod == 0 ? 8 : ch1.envelopePeriod;
            ch1.shadowFrequency = ch1.frequency;
            ch1.sweepTimer = ch1.sweepPeriod == 0 ? 8 : ch1.sweepPeriod;
            ch1.sweepEnabled = (ch1.sweepPeriod != 0 || ch1.sweepShift != 0);
            if (ch1.sweepShift != 0) {
                clock_sweep_unit();
            }
            break;
        case addr(AudioRegister::NR24):
            if (ch2.lengthCounter == 0) ch2.lengthCounter = 64;
            ch2.timer = compute_period(ch2.frequency);
            ch2.dutyStep = 0;
            ch2.enabled = ch2.dacEnabled;
            ch2.envelopeVolume = ch2.envelopeInitial;
            ch2.envelopeTimer =
                ch2.envelopePeriod == 0 ? 8 : ch2.envelopePeriod;
            break;
        case addr(AudioRegister::NR34):
            if (ch3.lengthCounter == 0) ch3.lengthCounter = 256;
            ch3.timer = compute_period(ch3.frequency);
            ch3.position = 0;
            ch3.enabled = ch3.dacEnabled;
            break;
        case addr(AudioRegister::NR44):
            if (ch4.lengthCounter == 0) ch4.lengthCounter = 64;
            ch4.enabled = ch4.dacEnabled;
            ch4.envelopeVolume = ch4.envelopeInitial;
            ch4.envelopeTimer =
                ch4.envelopePeriod == 0 ? 8 : ch4.envelopePeriod;
            ch4.lfsr = 0x7FFF;
            {
                uint8_t divisor =
                    ch4.divisorCode == 0 ? 8 : DIVISOR_LOOKUP[ch4.divisorCode];
                ch4.timer = divisor << ch4.clockShift;
            }
            break;
        default:
            break;
    }
    update_status_bits();
}

void APU::execute_cycle() {
    poll_bus();

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

    if (frameSequencerStep % 2 == 0) {
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
    auto decrement_length = [](auto &channel, auto /*maxLength*/) {
        if (!channel.lengthEnabled) return;
        if (channel.lengthCounter == 0) return;
        if (--channel.lengthCounter == 0) {
            channel.enabled = false;
        }
    };

    decrement_length(ch1, 64);
    decrement_length(ch2, 64);
    decrement_length(ch3, 256);
    decrement_length(ch4, 64);
    update_status_bits();
}

void APU::clock_sweep_unit() {
    if (!ch1.sweepEnabled || ch1.sweepPeriod == 0) return;

    if (--ch1.sweepTimer == 0) {
        ch1.sweepTimer = ch1.sweepPeriod == 0 ? 8 : ch1.sweepPeriod;
        if (ch1.sweepPeriod) {
            uint16_t delta = ch1.shadowFrequency >> ch1.sweepShift;
            if (ch1.sweepNegate) {
                ch1.shadowFrequency -= delta;
            } else {
                ch1.shadowFrequency += delta;
            }

            if (ch1.shadowFrequency > 2047) {
                ch1.enabled = false;
            } else {
                ch1.frequency = ch1.shadowFrequency & 0x7FF;
                nr13 = ch1.frequency & 0xFF;
                nr14 = (nr14 & 0xF8) | ((ch1.frequency >> 8) & 0x07);
                ch1.timer = compute_period(ch1.frequency);

                if (ch1.sweepShift != 0) {
                    uint16_t second = ch1.shadowFrequency >> ch1.sweepShift;
                    if (ch1.sweepNegate)
                        second = ch1.shadowFrequency - second;
                    else
                        second = ch1.shadowFrequency + second;
                    if (second > 2047) {
                        ch1.enabled = false;
                    }
                }
            }
        }
    }
}

void APU::clock_envelopes() {
    auto update_envelope = [](auto &channel) {
        if (!channel.enabled) return;
        if (channel.envelopePeriod == 0) return;
        if (--channel.envelopeTimer > 0) return;
        channel.envelopeTimer =
            channel.envelopePeriod == 0 ? 8 : channel.envelopePeriod;

        if (channel.envelopeIncrease) {
            if (channel.envelopeVolume < 15) {
                ++channel.envelopeVolume;
            }
        } else {
            if (channel.envelopeVolume > 0) {
                --channel.envelopeVolume;
            }
        }
    };

    update_envelope(ch1);
    update_envelope(ch2);
    update_envelope(ch4);
}

void APU::tick_square(SquareChannel &channel) {
    if (!channel.enabled) return;
    if (channel.timer == 0) channel.timer = compute_period(channel.frequency);

    if (--channel.timer == 0) {
        channel.timer = compute_period(channel.frequency);
        channel.dutyStep = (channel.dutyStep + 1) & 7;
    }
}

void APU::tick_wave() {
    if (!ch3.enabled) return;
    if (ch3.timer == 0) ch3.timer = compute_period(ch3.frequency);
    if (--ch3.timer == 0) {
        ch3.timer = compute_period(ch3.frequency);
        ch3.position = (ch3.position + 1) & 0x1F;
    }
}

void APU::tick_noise() {
    if (!ch4.enabled) return;
    if (ch4.timer == 0) {
        uint8_t divisor =
            ch4.divisorCode == 0 ? 8 : DIVISOR_LOOKUP[ch4.divisorCode];
        ch4.timer = divisor << ch4.clockShift;
    }

    if (--ch4.timer == 0) {
        uint8_t xorResult = ((ch4.lfsr & 1) ^ ((ch4.lfsr >> 1) & 1));
        ch4.lfsr = (ch4.lfsr >> 1) | (xorResult << 14);
        if (ch4.widthMode) {
            ch4.lfsr = (ch4.lfsr & ~(1 << 6)) | (xorResult << 6);
        }
        uint8_t divisor =
            ch4.divisorCode == 0 ? 8 : DIVISOR_LOOKUP[ch4.divisorCode];
        ch4.timer = divisor << ch4.clockShift;
    }
}

int8_t APU::sample_square(const SquareChannel &channel) const {
    if (!channel.enabled || !channel.dacEnabled) return 0;
    uint8_t output =
        DUTY_TABLE[channel.duty][channel.dutyStep] ? channel.envelopeVolume : 0;
    int centered = (output * 2) - channel.envelopeVolume;
    return static_cast<int8_t>(centered << 4);
}

int8_t APU::sample_wave() const {
    if (!ch3.enabled || !ch3.dacEnabled) return 0;
    uint8_t byte = waveRAM[ch3.position >> 1];
    uint8_t sample = (ch3.position & 1) ? (byte & 0x0F) : (byte >> 4);
    sample = apply_volume_code(sample, ch3.volumeCode);
    int centered = sample - 8;
    return static_cast<int8_t>(centered << 4);
}

int8_t APU::sample_noise() const {
    if (!ch4.enabled || !ch4.dacEnabled) return 0;
    uint8_t output = (~ch4.lfsr) & 1;
    int value = output ? ch4.envelopeVolume : 0;
    int centered = (value * 2) - ch4.envelopeVolume;
    return static_cast<int8_t>(centered << 4);
}

void APU::mix_and_output() {
    int8_t s1 = sample_square(ch1);
    int8_t s2 = sample_square(ch2);
    int8_t s3 = sample_wave();
    int8_t s4 = sample_noise();

    auto mix_channel = [&](int channelMask) {
        int mix = 0;
        if (channelMask & 0x01) mix += s1;
        if (channelMask & 0x02) mix += s2;
        if (channelMask & 0x04) mix += s3;
        if (channelMask & 0x08) mix += s4;
        return mix;
    };

    int leftMix = mix_channel(nr51 & 0x0F);
    int rightMix = mix_channel((nr51 >> 4) & 0x0F);

    int leftVolume = ((nr50 >> 4) & 0x07) + 1;
    int rightVolume = (nr50 & 0x07) + 1;

    int16_t leftSample = clamp16(leftMix * leftVolume * 256);
    int16_t rightSample = clamp16(rightMix * rightVolume * 256);

    queue_audio(leftSample, rightSample);
}

void APU::queue_audio(int16_t left, int16_t right) {
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
        SDL_QueueAudio(audioDevice, sampleBuffer.data(),
                       bufferedSamples * sizeof(int16_t) * 2);
#else
        SDL_PutAudioStreamData(audioStream, sampleBuffer.data(),
                               bufferedSamples * sizeof(int16_t) * 2);
#endif
        bufferedSamples = 0;
    }
}

void APU::flush_audio() {
#ifdef __EMSCRIPTEN__
    if (audioDevice == 0 || bufferedSamples == 0) return;
    SDL_QueueAudio(audioDevice, sampleBuffer.data(),
                   bufferedSamples * sizeof(int16_t) * 2);
#else
    if (audioStream == nullptr || bufferedSamples == 0) return;
    SDL_PutAudioStreamData(audioStream, sampleBuffer.data(),
                           bufferedSamples * sizeof(int16_t) * 2);
#endif
    bufferedSamples = 0;
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
    std::fill(prevRegs.begin(), prevRegs.end(), 0);
    update_status_bits();
}

void APU::update_status_bits() {
    uint8_t status = 0x70;
    if (masterEnabled) status |= 0x80;
    if (ch1.enabled) status |= 0x01;
    if (ch2.enabled) status |= 0x02;
    if (ch3.enabled) status |= 0x04;
    if (ch4.enabled) status |= 0x08;
    nr52 = status;
}

}  // namespace GBC

