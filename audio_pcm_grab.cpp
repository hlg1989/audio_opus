#include <string>
#include "audio_pcm_grab.h"
#include "generic.h"
#include <condition_variable>

#define ENABLE_PCM_FILE 1

AudioPcmGrab::AudioPcmGrab(int src_sample_rate, int channel)
    : m_samplerate(src_sample_rate)
    , m_channels(channel)
    , m_frames(1024)
    , m_encoder(NULL)
    , m_grabTaskEnd(false)
    , m_retrieveTaskEnd(false)
{
#ifdef ENABLE_PCM_FILE
    default_pcmwavhdr =
            {
                    { 'R', 'I', 'F', 'F' },
                    0,
                    {'W', 'A', 'V', 'E'},
                    {'f', 'm', 't', ' '},
                    16,
                    1,
                    m_channels,
                    src_sample_rate,
                    src_sample_rate * 4,
                    4,
                    16,
                    {'d', 'a', 't', 'a'},
                    0
            };

    std::string pcm_filename = "grab_loopback.wav";
    m_fp = fopen(pcm_filename.c_str(), "wb");
    if(m_fp)
        fwrite(&default_pcmwavhdr, sizeof(default_pcmwavhdr) ,1, m_fp);
#endif

    std::unique_ptr<AudioFifo> pfifo(new AudioFifo(m_channels, 2));
    m_fifo = std::move(pfifo);
    init();
}

AudioPcmGrab::~AudioPcmGrab()
{
    finit();
}

void AudioPcmGrab::init()
{
    std::string loop_device = "hw:hlg,1";
    int rc = snd_pcm_open(&m_handle, loop_device.c_str(), SND_PCM_STREAM_CAPTURE, 0);
    if(rc < 0){
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        exit(1);
    }
    snd_pcm_hw_params_alloca(&m_params);
    snd_pcm_hw_params_any(m_handle, m_params);

    snd_pcm_hw_params_set_access(m_handle, m_params, SND_PCM_ACCESS_RW_INTERLEAVED);

    snd_pcm_hw_params_set_format(m_handle, m_params, SND_PCM_FORMAT_S16_LE);


    snd_pcm_hw_params_set_channels(m_handle, m_params, m_channels);

    int dir;
    snd_pcm_hw_params_set_rate_near(m_handle, m_params, &m_samplerate, &dir);

    snd_pcm_hw_params_set_period_size_near(m_handle, m_params, &m_frames, &dir);
    //snd_pcm_hw_params_set_period_size(m_handle, m_params, m_frames, dir);

    rc = snd_pcm_hw_params(m_handle, m_params);
    if(rc < 0){
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
        exit(1);
    }

    snd_pcm_hw_params_get_period_size(m_params, &m_frames, &dir);

    m_buffer = (char* )malloc(m_frames * 4);

}

void AudioPcmGrab::finit()
{
    snd_pcm_drain(m_handle);
    snd_pcm_close(m_handle);
}

bool AudioPcmGrab::start()
{
    m_grabTaskEnd = false;
    m_grabTask = std::thread(&AudioPcmGrab::grabTask, this);

    m_retrieveTaskEnd = false;
    m_retrieveTask = std::thread(&AudioPcmGrab::retrieveTask, this);

    return true;
}

bool AudioPcmGrab::stop()
{

    m_retrieveTaskEnd = true;
    if (m_retrieveTask.joinable()) {
        try {
            m_retrieveTask.join();
        }
        catch (std::exception &e) {
            fprintf(stderr, "m_retrieveTask join failed, %s", e.what());
        }
    }

    fprintf(stderr, "m_retrieveTask has stopped.\n");
    m_grabTaskEnd = true;

    if (m_grabTask.joinable()) {
        try {
            m_grabTask.join();
        }
        catch (std::exception &e) {
            fprintf(stderr, "m_grabTask join failed, %s", e.what());
        }
    }

    fprintf(stderr, "m_grabTask has stopped.\n");


    return true;
}


void AudioPcmGrab::grabTask()
{
    while(!m_grabTaskEnd){
        int rc = snd_pcm_readi(m_handle, m_buffer, m_frames);
        if(rc == -EPIPE){
            fprintf(stderr, "overrun occurred\n");
            snd_pcm_prepare(m_handle);
        }else if(rc < 0){
            fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
        }else if(rc != (int)m_frames){
            fprintf(stderr, "short read, read %d frames\n", rc);
            continue;
        }

        {
#ifdef ENABLE_PCM_FILE
            if(m_fp){
                int size = m_frames * m_channels * 2;
                fwrite(m_buffer, size, 1, m_fp);
                default_pcmwavhdr.data_size += size;//修正pcm数据的大小
            }
#endif
            std::unique_ptr<AudioFifo> &audiofifo = m_fifo;
            audiofifo->AddSamples(m_buffer, m_frames);
        }
    }

#ifdef ENABLE_PCM_FILE
    default_pcmwavhdr.size_8 += default_pcmwavhdr.data_size + 36;

    if(m_fp) {
        fseek(m_fp, 4, 0);
        fwrite(&default_pcmwavhdr.size_8, sizeof(default_pcmwavhdr.size_8), 1, m_fp);
        fseek(m_fp, 40, 0);
        fwrite(&default_pcmwavhdr.data_size, sizeof(default_pcmwavhdr.data_size), 1, m_fp);
        fclose(m_fp);
        m_fp = NULL;
    }

#endif
}


void AudioPcmGrab::retrieveTask()
{
    const int OPUS_FRAMES_PERSECOND = 50;
    const int OPUS_FRAME_SIZE = m_samplerate / OPUS_FRAMES_PERSECOND;
    int src_frame_size = OPUS_FRAME_SIZE;

    int numSamples;
    int numSamplesRead;
    std::unique_ptr<AudioFifo> &audiofifo = m_fifo;
    while (!m_retrieveTaskEnd) {
        std::unique_lock<std::mutex> lck(audiofifo->m_fifo_mt);

        if(audiofifo->m_fifo_con.wait_for(lck, std::chrono::milliseconds(10)) == std::cv_status::timeout) {
            continue;
        }
        //audiofifo->m_fifo_con.wait_for(lck, std::chrono::milliseconds(20));
        if (audiofifo->GetNumSamples() >= src_frame_size) {
            if (src_frame_size != 0) {
                // must encode exactly frame_szie samples each time
                numSamples = src_frame_size;
            } else {
                // can encode any number of samples
                numSamples = audiofifo->GetNumSamples();
            }


            if (numSamples == 0) {
                continue;
            }


            int size = numSamples * 2 * m_channels;
            std::shared_ptr<grabData> data(new grabData(new char[size], size));
            data->auto_delete_data = true;

            numSamplesRead = audiofifo->GetSamples((char *) data->data, numSamples);
            if (numSamplesRead != numSamples) {
                fprintf(stdout, "Error: numSamplesRead != numSamples");
            }

            if(m_encoder){
                m_encoder->encode(data);
            }
        }
    }

}



void AudioPcmGrab::setEncoder(std::shared_ptr<audioOpusEncode> encoder)
{
    if(!m_encoder){
        m_encoder = encoder;
    }
}