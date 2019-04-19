//
// Created by hlg on 19-4-19.
//

#ifndef AUDIO_OPUS_AUDIO_PCM_GRAB_H
#define AUDIO_OPUS_AUDIO_PCM_GRAB_H

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <thread>
#include <memory>
#include <atomic>
#include "audiofifo.h"
#include "audio_opus_encode.h"

#pragma pack(push)
#pragma pack(1)     //1字节对齐

struct wave_pcm_hdr
{
    char            riff[4];                        // = "RIFF"
    unsigned int        size_8;                         // = FileSize - 8
    char            wave[4];                        // = "WAVE"
    char            fmt[4];                         // = "fmt "
    unsigned int        dwFmtSize;                      // = 下一个结构体的大小 : 16

    unsigned short         format_tag;              // = PCM : 1
    unsigned short         channels;                // = 通道数 : 1
    unsigned int        samples_per_sec;         // = 采样率 : 8000 | 6000 | 11025 | 16000
    unsigned int        avg_bytes_per_sec;       // = 每秒字节数 : dwSamplesPerSec * wBitsPerSample / 8
    unsigned short         block_align;             // = 每采样点字节数 : wBitsPerSample / 8
    unsigned short         bits_per_sample;         // = 量化比特数: 8 | 16

    char            data[4];                 // = "data";
    unsigned int        data_size;               // = 纯数据长度 : FileSize - 44
} ;

#pragma pack(pop) /* 恢复先前的pack设置 */

class AudioPcmGrab{
public:
    AudioPcmGrab(int src_sample_rate, int channel);
    ~AudioPcmGrab();

    void init();
    void finit();
    void setEncoder(std::shared_ptr<audioOpusEncode>encoder);
    bool start();
    bool stop();

private:
    void grabTask();
    void retrieveTask();

private:
    unsigned int m_samplerate;
    int m_channels;
    snd_pcm_t* m_handle;
    snd_pcm_hw_params_t* m_params;
    snd_pcm_uframes_t m_frames;
    char* m_buffer;
    FILE* m_fp;

    wave_pcm_hdr default_pcmwavhdr;

    std::atomic<bool> m_grabTaskEnd;
    std::thread m_grabTask;

    std::atomic<bool> m_retrieveTaskEnd;
    std::thread m_retrieveTask;

    std::unique_ptr<AudioFifo> m_fifo;
    std::shared_ptr<audioOpusEncode> m_encoder;

};

#endif //AUDIO_OPUS_AUDIO_PCM_GRAB_H
