#pragma once
#include <list>
#include <memory>
#include <mutex>
#include <condition_variable>

class SamplesChunk;

typedef std::list<std::shared_ptr<SamplesChunk>> SamplesList;

class AudioFifo {
public:
	AudioFifo(int channels, int bytesPerSample);
	~AudioFifo();

	/* Add numSamples per channel sample data pointed by samples. */
	void AddSamples(char *samples, int numSamples);
	/* Get numSampales per channel sample data from FIFO. Actual number of samples read is returned. Return all
	 * available samples if numSamples is 0. Caller allocates buffer to hold the data.
	 */
	int GetSamples(char *data,  const int numSamples = 0);
	int GetNumSamples();
	int GetCurSamples();
	std::condition_variable m_fifo_con;
	std::mutex m_fifo_mt;

private:
	int m_sampleSize;
	int m_numSamples;
	SamplesList m_samplesList;
	//std::mutex m_fifo_mt;
};