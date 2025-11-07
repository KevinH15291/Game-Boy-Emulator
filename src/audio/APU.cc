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

}  // namespace

APU::APU(address_bus &memory) : memory(memory) {
#ifdef __EMSCRIPTEN__
    SDL_Init(SDL_INIT_AUDIO);
    std::memset(&audioSpec, 0, sizeof(audioSpec));
    audioSpec.format = AUDIO_S16SYS;
    audioSpec.freq = SAMPLE_RATE;
    audioSpec.channels = 2;
    audioSpec.samples = AUDIO_BUFFER_SAMPLES;
    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &audioSpec, nullptr, 0);
    SDL_PauseAudioDevice(audioDevice, 0);
#else
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    std::memset(&audioSpec, 0, sizeof(audioSpec));
    audioSpec.format = SDL_AUDIO_S16;
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
            // Read current register values from bus
            uint8_t nr11 = memory.read_privileged(addr(AudioRegister::NR11));
            uint8_t nr12 = memory.read_privileged(addr(AudioRegister::NR12));
            uint8_t nr13 = memory.read_privileged(addr(AudioRegister::NR13));
            uint8_t nr14 = memory.read_privileged(addr(AudioRegister::NR14));
            uint8_t nr10 = memory.read_privileged(addr(AudioRegister::NR10));

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
                clock_sweep_unit();
            }
            break;
        }
        case addr(AudioRegister::NR24): {
            // Read current register values from bus
            uint8_t nr21 = memory.read_privileged(addr(AudioRegister::NR21));
            uint8_t nr22 = memory.read_privileged(addr(AudioRegister::NR22));
            uint8_t nr23 = memory.read_privileged(addr(AudioRegister::NR23));
            uint8_t nr24 = memory.read_privileged(addr(AudioRegister::NR24));

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
            // Read current register values from bus
            uint8_t nr31 = memory.read_privileged(addr(AudioRegister::NR31));
            uint8_t nr33 = memory.read_privileged(addr(AudioRegister::NR33));
            uint8_t nr34 = memory.read_privileged(addr(AudioRegister::NR34));
            uint8_t nr30 = memory.read_privileged(addr(AudioRegister::NR30));

            ch3.lengthCounter = 256 - nr31;
            ch3.dacEnabled = (nr30 & 0x80) != 0;
            ch3.lengthEnabled = (nr34 & 0x40) != 0;
            if (ch3.lengthCounter == 0) ch3.lengthCounter = 256;
            ch3.timer = compute_wave_period(nr33 | ((nr34 & 0x07) << 8));
            ch3.position = 0;
            // Read initial sample from wave RAM
            uint8_t waveByte = memory.read_privileged(0xFF30);
            ch3.currentSample = waveByte >> 4;
            ch3.enabled = ch3.dacEnabled;
            break;
        }
        case addr(AudioRegister::NR44): {
            // Read current register values from bus
            uint8_t nr41 = memory.read_privileged(addr(AudioRegister::NR41));
            uint8_t nr42 = memory.read_privileged(addr(AudioRegister::NR42));
            uint8_t nr43 = memory.read_privileged(addr(AudioRegister::NR43));
            uint8_t nr44 = memory.read_privileged(addr(AudioRegister::NR44));

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
            ch4.lfsr = 0xFFFF;
            {
                uint8_t divisor =
                    divisorCode == 0 ? 8 : DIVISOR_LOOKUP[divisorCode];
                ch4.timer = static_cast<uint32_t>(divisor) << clockShift;
                if (divisorCode == 0 && clockShift == 0) {
                    ch4.timer =
                        4;  // Special case: divisor 0.5 × 2^0 = 4 cycles
                }
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
            uint8_t nr14 = memory.read_privileged(addr(AudioRegister::NR14));
            ch1.lengthEnabled = (nr14 & 0x40) != 0;
            break;
        }
        case addr(AudioRegister::NR24): {
            uint8_t nr24 = memory.read_privileged(addr(AudioRegister::NR24));
            ch2.lengthEnabled = (nr24 & 0x40) != 0;
            break;
        }
        case addr(AudioRegister::NR34): {
            uint8_t nr34 = memory.read_privileged(addr(AudioRegister::NR34));
            ch3.lengthEnabled = (nr34 & 0x40) != 0;
            break;
        }
        case addr(AudioRegister::NR44): {
            uint8_t nr44 = memory.read_privileged(addr(AudioRegister::NR44));
            ch4.lengthEnabled = (nr44 & 0x40) != 0;
            break;
        }
        default:
            break;
    }
}

void APU::execute_cycle() {
    // Read master enable from bus
    bool newMasterEnabled =
        (memory.read_privileged(addr(AudioRegister::NR52)) & 0x80) != 0;

    if (newMasterEnabled != masterEnabled) {
        masterEnabled = newMasterEnabled;
        update_status_bits();
    }

    if (!masterEnabled) {
        return;
    }

    // Check DIV register for length timer (256 Hz)
    uint8_t currentDIV = memory.read_privileged(addr(IORegister::DIV));
    if ((currentDIV & 0x10) != (prevDIV & 0x10)) {
        clock_length_units();
    }
    prevDIV = currentDIV;

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
    // Read sweep settings from bus
    uint8_t nr10 = memory.read_privileged(addr(AudioRegister::NR10));
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
    // Check if DACs are turned off and disable channels accordingly
    // CH1
    if (ch1.enabled) {
        uint8_t nr12 = memory.read_privileged(addr(AudioRegister::NR12));
        if ((nr12 & 0xF8) == 0) {
            ch1.enabled = false;
            update_status_bits();
        }
    }

    // CH2
    if (ch2.enabled) {
        uint8_t nr22 = memory.read_privileged(addr(AudioRegister::NR22));
        if ((nr22 & 0xF8) == 0) {
            ch2.enabled = false;
            update_status_bits();
        }
    }

    // CH3
    if (ch3.enabled) {
        uint8_t nr30 = memory.read_privileged(addr(AudioRegister::NR30));
        if ((nr30 & 0x80) == 0) {
            ch3.enabled = false;
            update_status_bits();
        }
    }

    // CH4
    if (ch4.enabled) {
        uint8_t nr42 = memory.read_privileged(addr(AudioRegister::NR42));
        if ((nr42 & 0xF8) == 0) {
            ch4.enabled = false;
            update_status_bits();
        }
    }
}

void APU::clock_envelopes() {
    // Update CH1 envelope
    if (ch1.enabled) {
        uint8_t nr12 = memory.read_privileged(addr(AudioRegister::NR12));
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

    // Update CH2 envelope
    if (ch2.enabled) {
        uint8_t nr22 = memory.read_privileged(addr(AudioRegister::NR22));
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

    // Update CH4 envelope
    if (ch4.enabled) {
        uint8_t nr42 = memory.read_privileged(addr(AudioRegister::NR42));
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

    // Read frequency from bus
    uint16_t frequency = 0;
    if (&channel == &ch1) {
        frequency =
            memory.read_privileged(addr(AudioRegister::NR13)) |
            ((memory.read_privileged(addr(AudioRegister::NR14)) & 0x07) << 8);
    } else {
        frequency =
            memory.read_privileged(addr(AudioRegister::NR23)) |
            ((memory.read_privileged(addr(AudioRegister::NR24)) & 0x07) << 8);
    }

    if (channel.timer == 0) channel.timer = compute_square_period(frequency);

    if (--channel.timer == 0) {
        channel.timer = compute_square_period(frequency);
        channel.dutyStep = (channel.dutyStep + 1) & 7;
    }
}

void APU::tick_wave() {
    if (!ch3.enabled) return;

    // Read frequency from bus
    uint16_t frequency =
        memory.read_privileged(addr(AudioRegister::NR33)) |
        ((memory.read_privileged(addr(AudioRegister::NR34)) & 0x07) << 8);

    if (ch3.timer > 0) {
        --ch3.timer;
    }

    if (ch3.timer == 0) {
        ch3.timer = compute_wave_period(frequency);
        // Read wave RAM directly from bus
        uint8_t byte = memory.read_privileged(0xFF30 + (ch3.position >> 1));
        ch3.currentSample = (ch3.position & 1) ? (byte & 0x0F) : (byte >> 4);
        ch3.position = (ch3.position + 1) & 0x1F;
    }
}

void APU::tick_noise() {
    if (!ch4.enabled) return;

    // Read NR43 from bus
    uint8_t nr43 = memory.read_privileged(addr(AudioRegister::NR43));
    uint8_t clockShift = (nr43 >> 4) & 0x0F;
    bool widthMode = (nr43 & 0x08) != 0;
    uint8_t divisorCode = nr43 & 0x07;

    if (--ch4.timer == 0) {
        uint8_t bit0 = ch4.lfsr & 1;
        uint8_t bit1 = (ch4.lfsr >> 1) & 1;
        uint8_t newBit = (~(bit0 ^ bit1)) & 1;  // XNOR per hardware
        ch4.lfsr >>= 1;
        ch4.lfsr |= static_cast<uint16_t>(newBit) << 14;
        if (widthMode) {
            ch4.lfsr &= ~(1 << 6);
            ch4.lfsr |= static_cast<uint16_t>(newBit) << 6;
        }

        uint8_t divisor = divisorCode == 0 ? 8 : DIVISOR_LOOKUP[divisorCode];
        ch4.timer = static_cast<uint32_t>(divisor) << clockShift;
        if (divisorCode == 0 && clockShift == 0) {
            ch4.timer = 4;  // Special case: divisor 0.5 × 2^0 = 4 cycles
        }
        if (ch4.timer == 0) ch4.timer = 1;
    }
}

int8_t APU::sample_square(const SquareChannel &channel) const {
    if (!channel.enabled || !channel.dacEnabled) return 0;

    // Read duty from bus
    uint8_t duty = 0;
    if (&channel == &ch1) {
        duty = (memory.read_privileged(addr(AudioRegister::NR11)) >> 6) & 0x03;
    } else {
        duty = (memory.read_privileged(addr(AudioRegister::NR21)) >> 6) & 0x03;
    }

    uint8_t output =
        DUTY_TABLE[duty][channel.dutyStep] ? channel.envelopeVolume : 0;
    int centered = (output * 2) - channel.envelopeVolume;
    return static_cast<int8_t>(centered << 4);
}

int8_t APU::sample_wave() const {
    if (!ch3.enabled || !ch3.dacEnabled) return 0;

    // Read volume code from bus
    uint8_t volumeCode =
        (memory.read_privileged(addr(AudioRegister::NR32)) >> 5) & 0x03;

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

    int centered = static_cast<int>(sample) - 8;
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
    int8_t s1 = channelMuted[0] ? 0 : sample_square(ch1);
    int8_t s2 = channelMuted[1] ? 0 : sample_square(ch2);
    int8_t s3 = channelMuted[2] ? 0 : sample_wave();
    int8_t s4 = channelMuted[3] ? 0 : sample_noise();

    auto mix_channel = [&](int channelMask) {
        int mix = 0;
        if (channelMask & 0x01) mix += s1;
        if (channelMask & 0x02) mix += s2;
        if (channelMask & 0x04) mix += s3;
        if (channelMask & 0x08) mix += s4;
        return mix;
    };

    // Read panning and volume from bus
    uint8_t nr51 = memory.read_privileged(addr(AudioRegister::NR51));
    uint8_t nr50 = memory.read_privileged(addr(AudioRegister::NR50));

    int leftMix = mix_channel(nr51 & 0x0F);
    int rightMix = mix_channel((nr51 >> 4) & 0x0F);

    int leftVolume = ((nr50 >> 4) & 0x07) + 1;
    int rightVolume = (nr50 & 0x07) + 1;

    // Convert to 16-bit range with proper scaling
    // Game Boy DAC outputs 0-15 voltage levels, scale to ±32767
    int16_t leftSample = clamp16((leftMix * leftVolume * 2048) / 8);
    int16_t rightSample = clamp16((rightMix * rightVolume * 2048) / 8);

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
    // Read directly from bus
    return memory.read_privileged(address);
}

uint8_t APU::read_wave_byte(uint16_t address) const {
    // Read directly from bus wave RAM
    return memory.read_privileged(address);
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

