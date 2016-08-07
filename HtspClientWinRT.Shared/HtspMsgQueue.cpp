#include "HtspMsgQueue.h"
#include <chrono>

using namespace Wally::HtspClient;

HtspMsgQueue::HtspMsgQueue() :
	m_logger(Wally::Logger::get("HtspMsgQueue"))
{
}


HtspMsgQueue::~HtspMsgQueue()
{
}


void HtspMsgQueue::EnqueueMsg(const HtsMessage& msg)
{
	m_mtxQueue.lock();
	m_queue.push_back(msg);
	m_mtxQueue.unlock();
	m_cvQueueChanged.notify_all();
}

uint32_t Wally::HtspClient::HtspMsgQueue::GetWaitingMsgCount()
{
	return uint32_t(m_queue.size());
}

bool HtspMsgQueue::IsEmpty()
{
	bool res = false;
	m_logger->Trace("Queue lock: IsEmpty");
	m_mtxQueue.lock();
	res = m_queue.empty();
	m_mtxQueue.unlock();
	m_logger->Trace("Queue unlock: IsEmpty");

	return res;
	
}

bool HtspMsgQueue::FindFirst(HtsMessage& msg, uint32_t timeoutMs, std::function<bool(HtsMessage&)> cmp)
{
	if (!FindFirst(msg, cmp))
	{
		try {
			typedef std::chrono::high_resolution_clock Clock;
			Clock::time_point start = Clock::now();

			while (Clock::now() < start + std::chrono::milliseconds(timeoutMs))
			{	
				Clock::duration toWait = start + std::chrono::milliseconds(timeoutMs) - Clock::now();
				std::unique_lock<std::mutex> lk(m_mtxCV);
				if (m_cvQueueChanged.wait_for(lk, toWait) == std::cv_status::no_timeout)
				{
					if (FindFirst(msg, cmp))
						return true;
				}
			}
		}
		catch (std::system_error err)
		{
			m_logger->Error(err.what());
			return false;
		}

		return false;
	}


	return true;
}

bool HtspMsgQueue::FindFirst(HtsMessage& msg, std::function<bool(HtsMessage&)> cmp)
{
	//m_logger->Trace("Queue lock: FindFirst");
	m_mtxQueue.lock();
	if (!m_queue.empty())
	{
		auto it = m_queue.begin();

		while (it != m_queue.end())
		{
			if (cmp(*it))
			{
				msg = *it;
				m_queue.erase(it);
				m_mtxQueue.unlock();
				//m_logger->Trace("Queue unlock: FindFirst");
				return true;
			}
			it++;
		}
	}
	m_mtxQueue.unlock();
	//m_logger->Trace("Queue unlock: FindFirst");

	return false;
}


bool HtspMsgQueue::FindFirstSequence(HtsMessage& msg, uint32_t timeoutMs, uint32_t sequence)
{
	if (!FindFirstSequence(msg, sequence))
	{
		try {
			typedef std::chrono::high_resolution_clock Clock;
			Clock::time_point start = Clock::now();

			while (Clock::now() < start + std::chrono::milliseconds(timeoutMs))
			{
				Clock::duration toWait = start + std::chrono::milliseconds(timeoutMs) - Clock::now();
				std::unique_lock<std::mutex> lk(m_mtxCV);
				if (m_cvQueueChanged.wait_for(lk, toWait) == std::cv_status::no_timeout)
				{
					if (FindFirstSequence(msg, sequence))
						return true;
				}
			}
		}
		catch (std::system_error err)
		{
			m_logger->Error(err.what());
			return false;
		}

		return false;
	}


	return true;
}

bool HtspMsgQueue::FindFirstSequence(HtsMessage& msg, uint32_t sequence)
{
	//m_logger->Trace("Queue lock: FindFirstSequence");
	m_mtxQueue.lock();
	if (!m_queue.empty())
	{
		auto it = m_queue.begin();

		while (it != m_queue.end())
		{
			if (it->getRoot()->contains("seq") && it->getRoot()->getU32("seq") == sequence)
			{
				msg = *it;
				m_queue.erase(it);
				m_mtxQueue.unlock();
				//m_logger->Trace("Queue unlock: FindFirstSequence");
				return true;
			}
			it++;
		}
	}
	m_mtxQueue.unlock();
	//m_logger->Trace("Queue unlock: FindFirst");

	return false;
}

bool HtspMsgQueue::GetMsg(HtsMessage& msg, uint32_t timeoutMs)
{
	//m_logger->Trace("Queue lock: GetMsg");
	m_mtxQueue.lock();
	if (!m_queue.empty())
	{
		msg = m_queue.front();
		m_queue.pop_front();
		m_mtxQueue.unlock();
		//m_logger->Trace("Queue unlock: GetMsg");
		return true;
	}
	m_mtxQueue.unlock();
	//m_logger->Trace("Queue unlock: GetMsg, waiting");

	if (timeoutMs > 0)
	{

		std::unique_lock<std::mutex> lk(m_mtxCV);
		if (m_cvQueueChanged.wait_for(lk, std::chrono::milliseconds(timeoutMs)) == std::cv_status::no_timeout)
		{
			//m_logger->Trace("Queue lock: GetMsg2");
			m_mtxQueue.lock();
			if (!m_queue.empty())
			{
				msg = m_queue.front();
				m_queue.pop_front();
				m_mtxQueue.unlock();
				//m_logger->Trace("Queue unlock: GetMsg2");
				return true;
			}
			m_mtxQueue.unlock();
			//m_logger->Trace("Queue unlock: GetMsg2 loop");
		}
	}

	return false;
}

