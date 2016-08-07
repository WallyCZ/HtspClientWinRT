#pragma once


#include <wrl.h>
#include <wrl/client.h>
#include <agile.h>
#include <atomic>
#include <condition_variable>
#include <collection.h>
#include <ppltasks.h>
#include <concurrent_queue.h>
#include "Exceptions.h"
#include "htsmessage.h"
#include "HtspQueue.h"
#include "HtspMsgQueue.h"
#include "HtspData.h"

#define HTSP_PROTO_VERSION 20
#define MAX_QUEUE_SIZE 1000

namespace Wally
{
	namespace HtspClient
	{
		struct SubscriptionInfoInternal
		{
		public:
			SubscriptionInfo^ publicInfo;
			uint32_t audioStreamIndex;
			uint32_t videoStreamIndex;
		};

		EXCEPTION_MACRO(CMessageReadTimeoutException);
		EXCEPTION_MACRO(CMessageReadFailedException);
		EXCEPTION_MACRO(CAccessDeniedException);
		EXCEPTION_MACRO(CDisconnectedException);

		ref class HtspClient;

		public delegate void QueueStatusEventHandler(HtspClient^ sender, QueueStatusInfo^ queueStatus);
		public delegate void SignalStatusEventHandler(HtspClient^ sender, SignalStatusInfo^ signalStatus);
		public delegate void ChannelUpdateEventHandler(HtspClient^ sender, ChannelInfo^ channelInfo);
		public delegate void TagUpdateEventHandler(HtspClient^ sender, TagInfo^ tagInfo);
		public delegate void DVREntryUpdateEventHandler(HtspClient^ sender, DVRInfo^ dvrInfo);
		
		public delegate void DisconnectedEventHandler(HtspClient^ sender);


		ref class HtspFileStream;


		public ref class HtspClient sealed
		{
		public:
			HtspClient();

			Windows::Foundation::IAsyncOperation<bool>^ OpenConnectionAsync(Platform::String^ server, int port);

			Windows::Foundation::IAsyncOperation<bool>^ CloseConnectionAsync();

			Windows::Foundation::IAsyncOperation<bool>^ AuthenticateAsync(Platform::String^ user, Platform::String^ password);

			Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IMap<uint32_t, ChannelInfo^>^ >^ GetChannelsAsync();

			Windows::Foundation::IAsyncOperation<SubscriptionInfo^>^ SubscribeChannelAsync(uint32_t channelId, uint32_t weight, Platform::String^ profile);

			Windows::Foundation::IAsyncOperation<bool>^ UnsubscribeAsync(uint32_t subscriptionId);
			
			Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVector <EventInfo^> ^> ^ GetEventsAsync(uint32_t eventId, uint32_t channelId, uint32_t numEvents);

			Windows::Foundation::IAsyncOperation<Windows::Foundation::DateTime>^ GetSysTime();

			Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVector <ProfileInfo^>^>^ GetProfiles();

			Windows::Foundation::IAsyncOperation<HtspFileStream^>^ FileOpen(Platform::String^ filePath);
			
			void StartWorker();

			Windows::Foundation::IAsyncOperation<bool>^ StopWorker();

			property Platform::String^ ServerName
			{
				Platform::String^ get()
				{
					return InteropUtils::MarshalStringUtf8(m_serverInfo.serverName);
				}
			};

			property Platform::String^ ServerVersion
			{
				Platform::String^ get()
				{
					return InteropUtils::MarshalStringUtf8(m_serverInfo.serverVersion);
				}
			};

			property uint32_t ServerProtocolVersion
			{
				uint32_t get()
				{
					return m_serverInfo.protoVersion;
				}
			};

			uint32_t GetWaitingMessageCount()
			{
				return m_queue.GetWaitingMsgCount();
			}

			bool IsConnected()
			{
				return m_workerRunning && m_reader!=nullptr && m_writer!=nullptr;
			}

			//Events
			event QueueStatusEventHandler^ QueueStatusEvent;
			event SignalStatusEventHandler^ SignalStatusEvent;
			event ChannelUpdateEventHandler^ ChannelUpdateEvent;
			event DisconnectedEventHandler^ DisconnectedEvent;
			event TagUpdateEventHandler^ TagUpdateEvent;
			event DVREntryUpdateEventHandler ^DVREntryUpdateEvent;
		
		protected:
			Windows::Foundation::IAsyncOperation<uint64_t>^ FileRead(uint32_t fileId, uint64_t size, int64_t offset, Windows::Storage::Streams::IBuffer ^buffer);
			Windows::Foundation::IAsyncOperation<uint64_t>^ FileSeek(uint32_t fileId, int64_t offset, FileSeekType whence);
			Windows::Foundation::IAsyncOperation<bool>^ FileClose(uint32_t fileId);
		private:

			uint32_t HTSPNextSeqNum();

			Concurrency::task<bool> TransmitMessage(HtsMessage m);

			Concurrency::task<HtsMessage> ReadMessageThen(bool bFromWorker);

			Concurrency::task<HtsMessage> ReadMessage(bool bFromWorker = false);

			Concurrency::task<HtsMessage> ReadResult_Finish(HtsMessage m);

			Concurrency::task<HtsMessage> ReadResult_Repeat(uint32_t iSequence, bool repeat);

			Concurrency::task<HtsMessage> ReadResult(HtsMessage m, bool sequence = true);

			Concurrency::task<HtsMessage> MakeTaskMessage(HtsMessage& m)
			{
				return Concurrency::create_task([m]() ->HtsMessage
				{
					return m;
				});
			}

			bool ProcessEventMsg(HtsMessage msg);
			void ParseMuxPacket(HtsMessage &msg);

			void DumpMessage(HtsData* m, std::ostringstream& msg);
			void DumpMessage(HtsData* m);


			ChannelInfo^ MakeChannelInfoFromMsg(const HtsMessage &msg);
			TagInfo ^ MakeTagInfoFromMsg(const HtsMessage & msg);
			DVRInfo ^ MakeDVREventInfoFromMsg(const HtsMessage & msg);

			std::vector<SubscriptionInfoInternal> m_subscriptions;
			std::mutex m_mtxSubscriptions;

			Windows::Networking::Sockets::StreamSocket^ m_tcpClient;

			Windows::Storage::Streams::DataReader^ m_reader;
			Windows::Storage::Streams::DataWriter^ m_writer;

			Wally::Utils::Logger^ m_logger;

			uint32_t m_nextSeqNum;

			uint32_t m_chall_len;
			unsigned int * m_chall;

			ServerInfo m_serverInfo;

			Concurrency::cancellation_token_source m_ctsWorker;
			Windows::Foundation::IAsyncAction^ m_workItem;
			Concurrency::event m_workItemCompleted;

			bool m_workerRunning;
			std::atomic_int m_numReadWriteOperations;
			std::condition_variable m_numReadWriteOperationsCV;
			std::mutex m_numReadWriteOperationsMtx;

			HtspMsgQueue m_queue;

			uint32_t m_subscriptionIdNext;


			friend ref class HtspFileStream;

		};


	}
}