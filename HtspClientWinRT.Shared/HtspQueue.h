#pragma once

#include <concurrent_queue.h>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "InteropUtils.h"
#include "HtspDataExt.h"




namespace Wally
{
	namespace HtspClient
	{

		//TODO: private ref class HtspQueueInternal as in Platform:Array

		public ref class HtspQueue sealed
		{
		internal:
			bool ReadSample(HtspMediaSample& sample);

			void PutSample(HtspMediaSample& sample);

		public:

			bool ReadSample(MyIntPtr sample);

			void PutSample(MyIntPtr sample);

			HtspQueue();
			virtual ~HtspQueue();

			uint32_t GetNumWaiting();

			void Clear();

			Windows::Foundation::TimeSpan GetQueueLength()
			{
				return InteropUtils::ToTimeSpan(bufferLength);
			}

		private:
			concurrency::concurrent_queue<HtspMediaSample> m_queue;
			std::condition_variable cv;
			std::mutex cv_m;
			Timestamp bufferLength;
		};



	}
}

