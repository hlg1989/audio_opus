//
// Created by hlg on 19-4-16.
//

#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>
#include <stdio.h>


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

//默认音频头部数据
struct wave_pcm_hdr default_pcmwavhdr =
        {
                { 'R', 'I', 'F', 'F' },
                0,
                {'W', 'A', 'V', 'E'},
                {'f', 'm', 't', ' '},
                16,
                1,
                2,
                44100,
                176400,
                4,
                16,
                {'d', 'a', 't', 'a'},
                0
        };

int capture_pcm(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf("usage: %s capture.wav\n", argv[0]);
        return -1;
    }

    struct wave_pcm_hdr pcmwavhdr = default_pcmwavhdr;

    FILE * fp = fopen(argv[1], "wb");
    fwrite(&pcmwavhdr, sizeof(pcmwavhdr) ,1, fp);


    long loops;
    int rc;
    int size;
    snd_pcm_t* handle;
    snd_pcm_hw_params_t* params;
    unsigned int val;
    int dir;
    snd_pcm_uframes_t frames;
    char* buffer;

    rc = snd_pcm_open(&handle, "hw:hlg,1", SND_PCM_STREAM_CAPTURE, 0);
    if(rc < 0){
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        exit(1);
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);

    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);

    snd_pcm_hw_params_set_channels(handle, params, 2);

    val = 44100;
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);

    frames = 1024;
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

    rc = snd_pcm_hw_params(handle, params);
    if(rc < 0){
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
        exit(1);
    }

    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    size = frames * 4;
    buffer = (char* )malloc(size);

    snd_pcm_hw_params_get_period_time(params, &val, &dir);
    //loops = 20000000 / val;

    loops = 1000;
    while(loops > 0){
        loops--;
        rc = snd_pcm_readi(handle, buffer, frames);
        if(rc == -EPIPE){
            fprintf(stderr, "overrun occurred\n");
            snd_pcm_prepare(handle);
        }else if(rc < 0){
            fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
        }else if(rc != (int)frames){
            fprintf(stderr, "short read, read %d frames\n", rc);
        }

//        rc = write(1, buffer, size);
//        if(rc != size){
//            fprintf(stderr, "short write: wrote %d bytes\n", rc);
//        }

        {
            fwrite(buffer, size, 1, fp);
            pcmwavhdr.data_size += size;//修正pcm数据的大小
        }
    }

    pcmwavhdr.size_8 += pcmwavhdr.data_size + 36;

    fseek(fp, 4, 0);
    fwrite(&pcmwavhdr.size_8,sizeof(pcmwavhdr.size_8), 1, fp);
    fseek(fp, 40, 0);
    fwrite(&pcmwavhdr.data_size,sizeof(pcmwavhdr.data_size), 1, fp);
    fclose(fp);
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);

    return 0;
}