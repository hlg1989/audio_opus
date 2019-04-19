#include "audiofifo.h"
//#include <vld.h> 
#include <algorithm>

#include <string.h>

using namespace std;

class SamplesChunk {
private:
	int m_numSamples;
	int m_numSamplesRead;
	const int m_sampleSize; // for all channels
	char *m_samples;

	SamplesChunk();
	SamplesChunk(const SamplesChunk &obj);

	int NumSamplesToBytes(const int numSamples) {
		return numSamples * m_sampleSize;
	}

public:
	SamplesChunk(char *samples, const int numSamples, const int sampleSize) : m_numSamples(numSamples), m_sampleSize(sampleSize), m_numSamplesRead(0) {
		m_samples = (char *)malloc(NumSamplesToBytes(numSamples));
		memcpy(m_samples, samples, NumSamplesToBytes(numSamples));
	}

	~SamplesChunk() {
		free(m_samples);
	}

	int ReadSamples(char *data, int numSamples = 0) {
		if (numSamples == 0) {
			numSamples = GetNumAvailSamples();
		}

		numSamples = min(numSamples, GetNumAvailSamples());
		if (numSamples) {
			// read all samples
			memcpy(data, m_samples + NumSamplesToBytes(m_numSamplesRead), NumSamplesToBytes(numSamples));
		}

		m_numSamplesRead += numSamples;
		return numSamples;
	}

	int GetNumAvailSamples() {
		return m_numSamples - m_numSamplesRead;
	}
};

AudioFifo::AudioFifo(int channels, int bytesPerChannelSample) : m_numSamples(0) {
	m_sampleSize = channels * bytesPerChannelSample;
}

AudioFifo::~AudioFifo() {
}

void AudioFifo::AddSamples(char *samples, int numSamples) {
#define AUDIO_NUM_SAMPLES 882
	std::unique_lock<std::mutex> lck(m_fifo_mt);
	shared_ptr<SamplesChunk> chunk = make_shared<SamplesChunk>(samples, numSamples, m_sampleSize);
	m_samplesList.push_back(chunk);
	m_numSamples += numSamples;
	if (m_numSamples >= AUDIO_NUM_SAMPLES) {
		m_fifo_con.notify_one();
	}
		
}

int AudioFifo::GetSamples(char *data, const int numSamples) {
	//std::unique_lock<std::mutex> lck(m_fifo_mt);
	int numSamplesRead = numSamples;
	if (numSamplesRead == 0) {
		numSamplesRead = m_numSamples;
	}

	if (numSamplesRead != 0) {
		int numReadSoFar = 0;
		for (SamplesList::iterator it = m_samplesList.begin(); it != m_samplesList.end(); ++it, m_samplesList.pop_front()) {
			shared_ptr<SamplesChunk> chunk = *it;
			numReadSoFar += chunk->ReadSamples(data + m_sampleSize * numReadSoFar, numSamplesRead - numReadSoFar);
			if (numReadSoFar == numSamplesRead) {
				if (chunk->GetNumAvailSamples() == 0) {
					m_samplesList.pop_front();
				}
				break;
			}
		}
	}
	m_numSamples -= numSamplesRead;
	return numSamplesRead;
}

int AudioFifo::GetNumSamples() {
	//std::unique_lock<std::mutex> lck(m_fifo_mt);
	return m_numSamples;
}


int AudioFifo::GetCurSamples()
{
#define AUDIO_NUM_SAMPLES 882
    std::unique_lock<std::mutex> lck(m_fifo_mt);
    if (m_numSamples >= AUDIO_NUM_SAMPLES) {
        m_fifo_con.notify_one();
    }
}
