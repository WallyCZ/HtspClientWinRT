#include "HtspQueue.h"


using namespace Wally::HtspClient;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;


HtspQueue::HtspQueue()
{
	bufferLength = 0;
}


HtspQueue::~HtspQueue()
{
}


bool HtspQueue::ReadSample(HtspMediaSample& sample)
{
	if (m_queue.try_pop(sample))
	{
		bufferLength -= sample.duration;
		return true;
	}

	std::unique_lock<std::mutex> lk(cv_m);
	if (cv.wait_for(lk, std::chrono::milliseconds(3000)) == std::cv_status::no_timeout)
	{
		if (m_queue.try_pop(sample))
		{
			bufferLength -= sample.duration;
			return true;
		}
	}

	return false;
}

void HtspQueue::PutSample(HtspMediaSample& sample)
{
	m_queue.push(sample);
	cv.notify_all();
	bufferLength += sample.duration;
}

bool Wally::HtspClient::HtspQueue::ReadSample(MyIntPtr sample)
{
	return ReadSample(*(HtspMediaSample*)sample);
}

void Wally::HtspClient::HtspQueue::PutSample(MyIntPtr sample)
{
	PutSample(*(HtspMediaSample*)sample);
}

uint32_t HtspQueue::GetNumWaiting()
{
	return (uint32_t)m_queue.unsafe_size();
}

void Wally::HtspClient::HtspQueue::Clear()
{
	m_queue.clear();
}

inline Wally::HtspClient::HtspMediaSample::HtspMediaSample(const HtspMediaSample & other)
{
	buffer = other.buffer;
	bufferSize = other.bufferSize;
	isKey = other.isKey;
	duration = other.duration;
	presentTS = other.presentTS;
	decodeTS = other.decodeTS;

}
