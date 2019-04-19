//
// Created by hlg on 19-4-19.
//

#ifndef AUDIO_OPUS_GENERIC_H
#define AUDIO_OPUS_GENERIC_H

struct genericData{
    genericData(void* data = NULL, int size = 0)
    {
        this->data = data;
        this->size = size;
    }

    ~genericData()
    {
        if(auto_delete_data){
            if(data) {
                free(data);
                data = NULL;
            }
        }
    }

    void* data;
    int size;
    bool auto_delete_data;
};

typedef genericData grabData;
typedef genericData encodeData;

#endif //AUDIO_OPUS_GENERIC_H
