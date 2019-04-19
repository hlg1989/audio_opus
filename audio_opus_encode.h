//
// Created by hlg on 19-4-19.
//

#ifndef AUDIO_OPUS_AUDIO_OPUS_ENCODE_H
#define AUDIO_OPUS_AUDIO_OPUS_ENCODE_H

#include <mutex>
#include <deque>
#include <memory>
#include <thread>
#include <condition_variable>
#include "audioEncoderFFmpeg.h"
#include "generic.h"

class audioOpusEncode : public audioEncoderFFmpeg
{
public:
    audioOpusEncode(int src_sample_rate, int audio_bit_rate);
    ~audioOpusEncode();
    void start();
    void stop();
    bool init();
    bool encode(std::shared_ptr<grabData> data);

protected:
    int encode_init();
    void init_packet(AVPacket *packet);
    int encode_fini();
    bool encode_audio_frame(AVFrame *frame, int *got_packet);
    void encodeTask();
private:
    AVFrame *frame;
    AVPacket *m_pkt;
    AVFormatContext *m_format_context;

    bool m_encodeTaskEnd  = true;;
    std::thread m_encodeTask;

    std::deque<std::shared_ptr<grabData>> m_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_con;

    bool encodeData(std::shared_ptr<grabData> data);
};


#endif //AUDIO_OPUS_AUDIO_OPUS_ENCODE_H
