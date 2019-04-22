#include "audio_opus_encode.h"
#include "generic.h"
#include <sys/timeb.h>
#include <codecvt>
#include <chrono>
#include <thread>
#include <memory>
#define DUMP_OUTPUT

audioOpusEncode::audioOpusEncode(int src_sample_rate, int audio_bit_rate)
    : audioEncoderFFmpeg(src_sample_rate, audio_bit_rate)
{
    m_pkt = new AVPacket;
    m_queue.clear();
    encode_init();
}

audioOpusEncode::~audioOpusEncode()
{
    if (!m_encodeTaskEnd)
    {
        stop();
    }

    delete m_pkt;
    encode_fini();
}


int audioOpusEncode::encode_init()
{

#ifdef DUMP_OUTPUT
    char audioFileName[256];
    sprintf(audioFileName, "%s_%s.opus", "encode", "loopback");
    AVIOContext *io_context = NULL;
    int ret = avio_open(&io_context, audioFileName, AVIO_FLAG_WRITE);
    if (ret < 0) {
        fprintf(stdout, "Could not open output file %s\n", audioFileName);
        return -1;
    }

    m_format_context = avformat_alloc_context();
    if (!m_format_context) {
        fprintf(stdout, "Could not allocate output format context\n");
        avio_closep(&io_context);
        return -1;
    }

    m_format_context->pb = io_context;

    m_format_context->oformat = av_guess_format(NULL, audioFileName, NULL);
    m_format_context->oformat->video_codec = AV_CODEC_ID_H264;

    av_strlcpy(m_format_context->filename, audioFileName, sizeof(m_format_context->filename));



    AVCodec * fdk_aac = avcodec_find_encoder_by_name("libopus");
    if (!fdk_aac) {
        fprintf(stdout, "Could not find libfdk aac encoder\n");
    }

    AVStream * stream = avformat_new_stream(m_format_context, NULL);
    if (!stream) {
        fprintf(stdout, "Could not create new stream\n");
    }

    AVCodecContext *codec_context = avcodec_alloc_context3(fdk_aac);
    if (!codec_context) {
        fprintf(stdout, "Could not allocate an encoding context\n");
    }



    /*
    * Set the basic encoder parameters.
    */
    codec_context->channels = 2; // assume 2
    codec_context->channel_layout = av_get_default_channel_layout(codec_context->channels);
    codec_context->sample_rate = 48000;
    codec_context->sample_fmt = AV_SAMPLE_FMT_S16;
    codec_context->bit_rate = 96000;

    // allow experimental AAC encoder, not necessary
    codec_context->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    stream->time_base.den = 48000;
    stream->time_base.num = 1;

    if (m_format_context->oformat->flags & AVFMT_GLOBALHEADER) {
        codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ret = avcodec_open2(codec_context, fdk_aac, NULL);
    if (ret < 0) {
        fprintf(stdout, "Could not open output codec\n");
    }

    ret = avcodec_parameters_from_context(stream->codecpar, codec_context);
    if (ret < 0) {
        fprintf(stdout, "Could not init stream parameters.\n");
    }

    ret = avformat_write_header(m_format_context, NULL);
    if (ret < 0) {
    }
#endif

    return 0;
}

void audioOpusEncode::start()
{

    init();

    m_encodeTaskEnd = false;
    m_encodeTask = std::thread(&audioOpusEncode::encodeTask, this);

}

void audioOpusEncode::stop()
{
    m_encodeTaskEnd = true;
    if (m_encodeTask.joinable())
    {
        m_encodeTask.join();
    }

    if(m_queue.size()){
        m_queue.clear();
    }

}

bool audioOpusEncode::init()
{
    audioEncoderFFmpeg::init();
    frame = m_audio_st.src_frame;

    return true;
}

void audioOpusEncode::init_packet(AVPacket *packet)
{
    av_init_packet(packet);
    /** Set the packet data and size so that it is recognized as being empty. */
    packet->data = NULL;
    packet->size = 0;
}



//bool audioEncoderOpus::encodeData(boost::shared_ptr<grabData> data)
bool audioOpusEncode::encodeData(std::shared_ptr<grabData> data)
{

    frame->data[0] = (uint8_t *)data->data;
    int got_packet = 0;

    static struct timeb start, end;
    ftime(&start);
    encode_audio_frame(frame, &got_packet);
    ftime(&end);
    int diff = (int)(1000.0 * (end.time - start.time) + (end.millitm - start.millitm));
    if (diff > 20) {
    }

    return true;
}


bool audioOpusEncode::encode_audio_frame(AVFrame *frame, int *got_packet) {
    AVCodecContext *codec_context;
    //AVPacket *output_packet;
    int ret;

    //output_packet = (AVPacket *)malloc(sizeof(AVPacket));

    codec_context = m_codec_context;
    *got_packet = 0;
    init_packet(m_pkt);


    if (frame) {

        frame->pts = m_audio_st.samples_count;
        m_audio_st.samples_count += frame->nb_samples;

        /* convert samples from native format to destination codec format, using the resampler */
        /* compute destination number of samples */

        auto ret = swr_get_delay(m_audio_st.swr_ctx, m_audio_st.codec_context->sample_rate);
        int dst_nb_samples = av_rescale_rnd(frame->nb_samples,
                                            m_audio_st.codec_context->sample_rate, m_sample_rate, AV_ROUND_UP);

        //int dst_nb_samples = 960;


        /* when we pass a frame to the encoder, it may keep a reference to it
        * internally;
        * make sure we do not overwrite it here
        */
        ret = av_frame_make_writable(m_audio_st.dest_frame);
        if (ret < 0)
            exit(1);

        /* convert to destination format */
        ret = swr_convert(m_audio_st.swr_ctx,
                          m_audio_st.dest_frame->data, dst_nb_samples,
                          (const uint8_t **)frame->data, frame->nb_samples);
        if (ret < 0) {

            exit(1);
        }
        frame = m_audio_st.dest_frame;



        frame->pts = m_audio_frames_count * 960;;

    }

#pragma warning(disable : 4996)
    /**
    * Encode the audio frame and store it in the temporary packet.
    * The output audio stream encoder is used to do this.
    */
    ret = avcodec_encode_audio2(codec_context, m_pkt, frame, got_packet);
    if (ret < 0) {
        //free(output_packet);
        av_packet_unref(m_pkt);
        return false;
    }


    if (m_pkt->size < 0) {
        return false;
    }





    if (*got_packet) {

        m_audio_frames_count++;

#ifdef DUMP_OUTPUT
        //if(m_audio_frames_count < 500) {
            av_write_frame(m_format_context, m_pkt);
        //}
        //else if(m_audio_frames_count == 5000)
        //{
        //    av_write_trailer(m_format_context);
        //}
#endif

        av_packet_unref(m_pkt);
        return true;
    }


}



int audioOpusEncode::encode_fini()
{


    if (m_queue.size())
    {
        {
            std::unique_lock<std::mutex> lck(m_queue_mutex);
            if (m_queue.size())
            {
                m_queue.clear();
            }
        }

    }

    while (true) {
        m_pkt->data = NULL;
        m_pkt->size = 0;
        av_init_packet(m_pkt);
        int ret = 0;
        int got_packet = 0;
        ret = avcodec_encode_audio2(m_codec_context, m_pkt, NULL, &got_packet);
        av_frame_free(NULL);
        if (ret < 0)
            break;
        if (!got_packet){
            ret=0;
            break;
        }
        //printf("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n",m_pkt->size);
        av_write_frame(m_format_context, m_pkt);
        if (ret < 0)
            break;
    }

    //printf("encode loopback frame count: %d\n", m_audio_frames_count);

#ifdef DUMP_OUTPUT
    av_write_trailer(m_format_context);
    if (m_format_context) {
        avformat_free_context(m_format_context);
        m_format_context = NULL;
    }
#endif

    return 0;
}


bool audioOpusEncode::encode(std::shared_ptr<grabData> data)
{
    std::unique_lock<std::mutex> lck(m_queue_mutex);
    m_queue.push_back(data);
    m_con.notify_one();
    return true;
}

void audioOpusEncode::encodeTask()
{

    while (!m_encodeTaskEnd) {

        std::shared_ptr<grabData> data;
        if (m_queue.size() == 0)
        {
            std::unique_lock<std::mutex> lck(m_queue_mutex);
            if (m_queue.size())
            {
                data = m_queue.front();
                m_queue.pop_front();
            }
            else {
                m_con.wait_for(lck, std::chrono::milliseconds(20));
                if (m_queue.size())
                {
                    data = m_queue.front();
                    m_queue.pop_front();
                }
            }

        }
        else {
            std::unique_lock<std::mutex> lck(m_queue_mutex);
            data = m_queue.front();
            m_queue.pop_front();
        }
        if (data)
        {
            auto ret = encodeData(data);
        }

    }
}
