#include <iostream>
#include "audio_opus_encode.h"
#include "audio_pcm_grab.h"
#include <memory>
#include <chrono>
#include <thread>

int main() {

    int sample_rate = 44100;
    int channels = 2;
    int bitrate = 96000;

    std::shared_ptr<audioOpusEncode>audio_encoder(new audioOpusEncode(sample_rate, bitrate));
    std::shared_ptr<AudioPcmGrab>audio_grabber(new AudioPcmGrab(sample_rate, channels));

    fprintf(stderr, "~~~ start process: begin to grab and encode ~~~.\n");

    audio_grabber->setEncoder(audio_encoder);
    audio_encoder->start();
    audio_grabber->start();

    std::this_thread::sleep_for(std::chrono::seconds(10));
    fprintf(stderr, "~~~ stop process: end to encode and grab ~~~.\n");
    audio_encoder->stop();
    audio_encoder->stop();



    return 0;
}