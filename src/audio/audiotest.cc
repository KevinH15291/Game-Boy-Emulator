#include "SDL3/SDL_timer.h"
#include <cstdint>
#include <cstring>
#include <math.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include <stdexcept>

constexpr float AMPLITUDE = 0.5;
constexpr int SAMPLE_RATE = 44032;

// void audio_callback(void *user_data, Uint8 *raw_buffer, int bytes)
// {
//     int8_t *buffer = (int8_t*)raw_buffer;
//     int length = bytes ; // 2 bytes per sample for AUDIO_S16SYS
//     square &info(*(square*)user_data);
//     int &sample_nr = info.sample_nr;
//     float tone = info.tone;
//     float amplitude = info.amplitude;

//     int8_t wave[16] = {127 ,111,95,79,63,47,31,15,-1, -17, -33, -49, -65, -81, -97, -113};
//     for(int i = 0; i < length; i++, sample_nr++)
//     {
//         int j = ((i + sample_nr)/16)%4;
//         double time = (double)sample_nr / (double)SAMPLE_RATE;

//         buffer[i] = wave[j];
//     }
// }

// void SDLCALL MyNewAudioCallbac   k(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
// {
//     if (additional_amount > 0) {
//         Uint8 *data = SDL_stack_alloc(Uint8, additional_amount);
//         if (data) {
//             audio_callback(userdata, data, additional_amount);
//             SDL_PutAudioStreamData(stream, data, additional_amount);
//             SDL_stack_free(data);
//         }
//     }
// }

struct square {
    int sample_nr = 0;
    uint8_t duty = 0;
    float tone = 0;
    float amplitude = 0;
};

float square_wave(double n, uint8_t duty) {
    switch(duty) {
        case 0:
            return ((int)n%8 == 0) ? -1.0f : 1.0f;
        case 1:
            return ((int)n%8 < 2) ? -1.0f : 1.0f;
        case 2:
            return ((int)n%8 < 4) ? 1.0f : -1.0f;
        case 3:
            return ((int)n%8 < 2) ? 1.0f : -1.0f;
        default: 
            throw std::runtime_error("should not be here");
    }
}


int main() {
    SDL_Init(SDL_INIT_AUDIO);
    SDL_InitSubSystem(SDL_INIT_AUDIO);


    square c1 = {0, 2, 440, 30}, c2 = {0, 1, 440, 0.5};

    const SDL_AudioSpec c1spec = { SDL_AUDIO_S8, 1, SAMPLE_RATE};
    SDL_AudioStream *channel1 = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &c1spec, NULL, &c1);
    if (channel1 == NULL) {
        printf("Uhoh, stream failed to create: %s\n", SDL_GetError());
    }
    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(channel1));

    square info = c1;
    int &sample_nr = info.sample_nr;
    float tone = info.tone;

    uint8_t buffer[44032];
    for(int i = 0; i < 44032; i++, sample_nr++)
    {
        double time = (double)sample_nr / (double)SAMPLE_RATE;
        buffer[i] = (info.amplitude * square_wave( 8*tone * time, info.duty));
    }

    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(channel1));
    SDL_PutAudioStreamData(channel1, buffer, 44032);

    SDL_Delay(1500);



    //tetr
    // const SDL_AudioSpec c2spec = { SDL_AUDIO_F32LE, 1, SAMPLE_RATE };
    // SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(channel2));
    // *channel2 = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &c2spec, MyNewAudioCallback, &c2);

    // c2.tone = 659;
    // c1.tone = 494;
    
    // c1.tone = 494;
    // c2.tone = 415;
    // SDL_Delay(200);

    // c1.tone = 525;
    // c2.tone = 440;
    // SDL_Delay(200);

    // c1.tone = 587;
    // c2.amplitude = 0;
    // SDL_Delay(200);

    // c1.tone = 659;
    // SDL_Delay(100);

    // c1.tone = 587;
    // SDL_Delay(100);

    // c1.tone = 525;
    // c2.tone = 440;
    // c2.amplitude = 0.5;
    // SDL_Delay(200);

    // c1.tone = 494;
    // c2.tone = 415;
    // SDL_Delay(200);

    // c1.tone = 440;
    // c2.tone = 330;
    // SDL_Delay(200);



    return 0;
}