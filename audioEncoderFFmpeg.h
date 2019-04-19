#pragma once
extern "C" {
#include <inttypes.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avstring.h>
}
#include <chrono>


struct AVPacket;

struct AudioOutputStream {

	AVCodecContext *codec_context;

	int samples_count;

	AVFrame *src_frame;
	AVFrame *dest_frame;



	SwsContext *sws_ctx;
	SwrContext *swr_ctx;
};



class audioEncoderFFmpeg
{
public:
	audioEncoderFFmpeg(int src_sample_rate, int audio_bit_rate)
		:
		m_audio_codec(NULL),
		m_codec_context(NULL),
		m_sample_rate(src_sample_rate),
		m_bitrate(audio_bit_rate),
		m_audio_frames_count(0)
	{
		av_register_all();
		avformat_network_init();
		memset(&m_audio_st, 0, sizeof(AudioOutputStream));
	};


	virtual ~audioEncoderFFmpeg()
	{

		avcodec_close(m_audio_st.codec_context);
		av_free(m_audio_st.codec_context);
		if (m_audio_st.src_frame) {
			av_frame_free(&m_audio_st.src_frame);
			av_free(m_audio_st.src_frame);
		}


		av_frame_free(&m_audio_st.dest_frame);
		av_free(m_audio_st.dest_frame);

		swr_free(&m_audio_st.swr_ctx);

		sws_freeContext(m_audio_st.sws_ctx);
	}

protected:

	AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
		uint64_t channel_layout,
		int sample_rate, int nb_samples)
	{
		AVFrame *frame = av_frame_alloc();
		int ret;

		if (!frame) {
			exit(1);
		}

		frame->format = sample_fmt;
		frame->channel_layout = channel_layout;
		frame->sample_rate = sample_rate;
		frame->nb_samples = nb_samples;

		if (nb_samples) {
			ret = av_frame_get_buffer(frame, 0);

			if (ret < 0) {
				exit(1);
			}
		}

		return frame;
	}


	virtual bool init()
	{
		m_audio_codec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
		if (!(m_audio_codec)) {

			return false;
		}

		int i;
		m_codec_context = avcodec_alloc_context3(m_audio_codec);
		if (m_codec_context && m_audio_codec->type == AVMEDIA_TYPE_AUDIO) {
			m_codec_context->sample_fmt = m_audio_codec->sample_fmts ? m_audio_codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
			m_codec_context->bit_rate = m_bitrate;
			m_codec_context->sample_rate = m_sample_rate;
			if (m_audio_codec->supported_samplerates) {
				m_codec_context->sample_rate = m_audio_codec->supported_samplerates[0];
				for (i = 0; m_audio_codec->supported_samplerates[i]; i++) {

					if (m_audio_codec->supported_samplerates[i] == m_sample_rate)
						m_codec_context->sample_rate = m_sample_rate;
				}
			}
			m_codec_context->channels = 2;
			m_codec_context->channel_layout = av_get_default_channel_layout(m_codec_context->channels); AV_CH_LAYOUT_STEREO;
			if (m_audio_codec->channel_layouts) {
				m_codec_context->channel_layout = m_audio_codec->channel_layouts[0];
				for (i = 0; m_audio_codec->channel_layouts[i]; i++) {
					if (m_audio_codec->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
						m_codec_context->channel_layout = AV_CH_LAYOUT_STEREO;
				}
			}

			m_codec_context->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;


			m_audio_st.codec_context = m_codec_context;

			char tmp[AV_ERROR_MAX_STRING_SIZE] = { 0 };

			int ret = avcodec_open2(m_codec_context, m_audio_codec, NULL);
			int nb_samples;

			if (m_codec_context->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
				nb_samples = 10000;
			else
				nb_samples = m_codec_context->frame_size;


			m_audio_st.src_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, m_codec_context->channel_layout,
				m_sample_rate, round(nb_samples * 1.0 *  m_sample_rate / m_codec_context->sample_rate));

			m_audio_st.dest_frame = alloc_audio_frame(m_codec_context->sample_fmt, m_codec_context->channel_layout,
				m_codec_context->sample_rate, nb_samples);

			/* create resampler context */
			m_audio_st.swr_ctx = swr_alloc_set_opts(NULL,
				m_codec_context->channel_layout,
				m_codec_context->sample_fmt,
				m_codec_context->sample_rate,
				m_codec_context->channel_layout,
				AV_SAMPLE_FMT_S16,
				m_sample_rate,
				0, NULL);
			if (!m_audio_st.swr_ctx) {

				return false;
			}

			/* set options */
			av_opt_set_int(m_audio_st.swr_ctx, "in_channel_count", m_codec_context->channels, 0);
			av_opt_set_int(m_audio_st.swr_ctx, "out_channel_count", m_codec_context->channels, 0);


			/* initialize the resampling context */
			if ((ret = swr_init(m_audio_st.swr_ctx)) < 0) {

				return false;
			}

			return true;

		}

		return false;
	}




protected:
	AudioOutputStream m_audio_st;
	AVCodec *m_audio_codec;
	AVCodecContext *m_codec_context;

	int m_sample_rate;
	int m_bitrate;
	long m_audio_frames_count;


};
