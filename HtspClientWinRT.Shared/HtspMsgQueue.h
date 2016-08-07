#pragma once

#include <deque>  
#include <mutex>  
#include <condition_variable>
#include <wrl.h>
#include <wrl/client.h>
#include "htsmessage.h"
#include "Logger.h"


namespace Wally
{
	namespace HtspClient
	{

		class HtspMsgQueue
		{
		private:
			typedef std::deque<HtsMessage> MsgQueueType;

			std::shared_ptr<Wally::Logger> m_logger;

			MsgQueueType m_queue;
			std::mutex m_mtxQueue;
			std::mutex m_mtxCV;
			std::condition_variable m_cvQueueChanged;

			

		public:
			HtspMsgQueue();
			~HtspMsgQueue();

			void EnqueueMsg(const HtsMessage& msg);

			bool FindFirst(HtsMessage& msg, std::function<bool(HtsMessage&)> cmp);
			bool FindFirstSequence(HtsMessage & msg, uint32_t timeoutMs, uint32_t sequence);
			bool FindFirstSequence(HtsMessage & msg, uint32_t sequence);
			bool FindFirst(HtsMessage& msg, uint32_t timeoutMs, std::function<bool(HtsMessage&)> cmp);

			bool GetMsg(HtsMessage& msg, uint32_t timeoutMs);


			uint32_t GetWaitingMsgCount();

			bool IsEmpty();
		};


	}
}