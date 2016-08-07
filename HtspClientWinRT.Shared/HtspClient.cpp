#include "pch.h"
#include <collection.h>
#include <sstream>
#include <chrono>
#include <thread>
#include <ppl.h>
#include "scope_guard.h"
#include "common.h"
#include "InteropUtils.h"
#include "HtspClient.h"
#include "sha1.h"
#include "HtspSource.h"
#include "HtspFileStream.h"

using namespace std;
using namespace Platform;
using namespace Platform::Collections;
using namespace Concurrency;
using namespace Windows::Foundation;
using namespace Windows::Networking;
using namespace Windows::Networking::Sockets;
using namespace Windows::System::Threading;
using namespace Windows::Storage::Streams;
using namespace Windows::Security::Cryptography;
using namespace Windows::Security::Cryptography::Core;
using namespace Wally::HtspClient;
using namespace Wally::Utils;


HtspClient::HtspClient()
{
	m_logger = Wally::Utils::Logger::GetLogger("HtspClient");
	m_nextSeqNum = 1;
	m_chall = nullptr;
	m_reader = nullptr;
	m_writer = nullptr;
	m_tcpClient = nullptr;
	m_chall_len = 0;
	m_workerRunning = false;
	m_subscriptionIdNext = 1;
	m_numReadWriteOperations = 0;
}


IAsyncOperation<bool>^ HtspClient::OpenConnectionAsync(String^ server, int port)
{

	return create_async([this, server, port]()
	{
		return create_task([this, server, port]() -> IAsyncAction^
		{
			HostName^ hostName = ref new HostName(server);

			m_tcpClient = ref new StreamSocket();
			m_tcpClient->Control->KeepAlive = true;

			return m_tcpClient->ConnectAsync(
				hostName,
				port.ToString(),
				SocketProtectionLevel::PlainSocket);

		}).then([this](task<void> prevTask) -> HtsMap
		{
			prevTask.get();

			// add after calling ConnectAsync on the StreamSocket Object

			m_reader = ref new DataReader(m_tcpClient->InputStream);
			//m_reader->InputStreamOptions = InputStreamOptions::Partial;

			m_writer = ref new DataWriter(m_tcpClient->OutputStream);

			HtsMap map;
			map.setData("method", "hello");
			map.setData("clientname", "TVHPlayer WinRT");
			map.setData("htspversion", HTSP_PROTO_VERSION);

			return map;

		}).then([this](task<HtsMap> prevTask) -> task<HtsMessage>
		{
			return ReadResult(prevTask.get().makeMsg());

		}).then([this](task<HtsMessage> prevTask) -> bool
		{
			HtsMessage& m = prevTask.get();

			if (!m.isValid())
			{
				return false;
			}

			m_serverInfo.serverName = m.getRoot()->getStr("servername");
			m_serverInfo.serverVersion = m.getRoot()->getStr("serverversion");
			m_serverInfo.protoVersion = m.getRoot()->getU32("htspversion");
			m.getRoot()->getBin("challenge", &m_chall_len, (void**)&m_chall);

			m_logger->Info("Connected to HTSP Server {0}, version {1}, protocol {2}", InteropUtils::MarshalStringUtf8(m_serverInfo.serverName), InteropUtils::MarshalStringUtf8(m_serverInfo.serverVersion), ref new Platform::Box<int>(m_serverInfo.protoVersion));
			if (m_serverInfo.protoVersion < HTSP_PROTO_VERSION)
			{
				m_logger->Warn("TVHeadend is running an older version of HTSP(v{0}) than we are(v{1}). No effort was made to keep compatible with older versions, update tvh before reporting problems!", ref new Platform::Box<int>(m_serverInfo.protoVersion), ref new Platform::Box<int>(HTSP_PROTO_VERSION));
			}
			else if (m_serverInfo.protoVersion > HTSP_PROTO_VERSION)
			{
				m_logger->Info("TVHeadend is running a more recent version of HTSP(v{0}) than we are(v{1}). Check if there is an update available!", ref new Platform::Box<int>(m_serverInfo.protoVersion), ref new Platform::Box<int>(HTSP_PROTO_VERSION));
			}

			return true;
		});
	});

}

IAsyncOperation<bool>^ HtspClient::CloseConnectionAsync()
{
	return create_async(
		[this]() -> bool
	{
		m_logger->Info("Disconnecting from htsp server");

		//TODO: unsubscribe all?
		try {
			if (m_workerRunning)
			{
				m_mtxSubscriptions.lock();

				for (auto it = m_subscriptions.begin(); it != m_subscriptions.end(); ++it)
				{
					create_task(UnsubscribeAsync(it->publicInfo->subscriptionId)).wait(); 
				}
				m_mtxSubscriptions.unlock();
			}
		}
		catch (Platform::Exception^ exc)
		{
			m_logger->Warn("Exception in CloseConnectionAsync {0}", exc->Message);
			m_logger->Warn("Exception in CloseConnectionAsync - toString: {0}", exc->ToString());
		}
		catch (...)
		{
			m_logger->Warn("Exception in CloseConnectionAsync");
		}

		StopWorker();

		std::unique_lock<std::mutex> lk(m_numReadWriteOperationsMtx);

		if (!m_numReadWriteOperationsCV.wait_for(lk, std::chrono::milliseconds(3000), [this]() {return m_numReadWriteOperations == 0; }))
			m_logger->Warn("CloseConnectionAsync: Waiting for closing datareader/writer timeouted");
		
		if (m_reader != nullptr)
		{
			delete m_reader;
			m_reader = nullptr;
		}

		if (m_writer != nullptr)
		{
			delete m_writer;
			m_writer = nullptr;
		}

		if (m_tcpClient != nullptr)
		{
			delete m_tcpClient;
			m_tcpClient = nullptr;
		}

		return true;
	});
}

IAsyncOperation<bool>^ HtspClient::AuthenticateAsync(String^ user, String^ pass)
{
	return create_async(
		[this, user, pass]() -> bool
	{
		HtsMap map = HtsMap();

		std::string username = InteropUtils::MarshalString(user);

		map.setData("method", "authenticate");
		map.setData("username", username);

		if (!pass->IsEmpty() && m_chall)
		{
			/*auto SHA1 = HashAlgorithmProvider::OpenAlgorithm(HashAlgorithmNames::Sha1);

			auto buffer = CryptographicBuffer::ConvertStringToBinary(pass, BinaryStringEncoding::Utf8);
			Array<unsigned char>^ arrPass;

			CryptographicBuffer::CopyToByteArray(buffer, &arrPass);
			auto arr = ref new Array<unsigned char>(arrPass->Length + m_chall_len);

			InteropUtils::CopyArray(arrPass, 0, arr, 0, arrPass->Length);

			for (size_t i = 0, index = buffer->Length; i < m_chall_len; i++, index++)
			{
				arr[index] = m_chall[i];
			}

			buffer = CryptographicBuffer::CreateFromByteArray(arr);

			auto hash = SHA1->HashData(buffer);

			CryptographicBuffer::CopyToByteArray(hash, &arr);


			auto d = InteropUtils::ToArray(arr);*/


			std::string password = InteropUtils::MarshalString(pass);
			HTSSHA1 *shactx = (HTSSHA1*)malloc(hts_sha1_size);
			uint8_t e[20];
			hts_sha1_init(shactx);
			hts_sha1_update(shactx, (const uint8_t *)(password.c_str()), (unsigned int)password.length());
			hts_sha1_update(shactx, (const uint8_t *)m_chall, m_chall_len);
			hts_sha1_final(shactx, e);

			std::shared_ptr<HtsBin> bin = std::make_shared<HtsBin>();
			bin->setBin(20, /*d.get()*/e);
			map.setData("digest", bin);

		}
		else
		{
			m_logger->Info("Authenticating as '{0}' without a password", InteropUtils::MarshalStringUtf8(username));
		}

		if (m_chall)
		{
			free(m_chall);
			m_chall = nullptr;
		}

		try {
			ReadResult(map.makeMsg()).get();
		}
		catch (CAccessDeniedException)
		{
			return false;
		}

		return true;

	});
}
struct tmp_channel
{
	std::string name;
	uint32_t cid;
	uint32_t cnum;
	std::string url;
	std::string cicon;
	std::list<std::string> tags;
};

IAsyncOperation<Windows::Foundation::Collections::IMap<uint32_t, ChannelInfo^>^ >^ HtspClient::GetChannelsAsync()
{
	return create_async(
		[this]() -> Windows::Foundation::Collections::IMap<uint32_t, ChannelInfo^>^
	{
		m_logger->Debug("GetChannelsAsync start");
		HtsMap map;
		map.setData("method", "enableAsyncMetadata");
		if (!(ReadResult(map.makeMsg()).get()).isValid())
			return nullptr;

		auto channels = ref new Map<uint32_t, ChannelInfo^>();

		HtsMessage m;
		while ((m = ReadMessage().get()).isValid())
		{
			std::string method = m.getRoot()->getStr("method");
			if (method.empty() || method == "initialSyncCompleted")
			{
				//msg_Info(sd, "Finished getting initial metadata sync");
				break;
			}

			if (method == "channelAdd")
			{
				if (!m.getRoot()->contains("channelId"))
					continue;

				ChannelInfo^ chl = MakeChannelInfoFromMsg(m);

				if (channels->HasKey(chl->channelId))
				{
					ChannelInfo^ chl2 = channels->Lookup(chl->channelId);
					chl2 = chl;//???
				}
				else
				{ 
					channels->Insert(chl->channelId, chl);
				}
			}
			else 
			{
				ProcessEventMsg(m);
			}
		}

		m_logger->Debug("GetChannelsAsync stop");
		return channels;
	});
}


IAsyncOperation<SubscriptionInfo^>^ HtspClient::SubscribeChannelAsync(uint32_t channelId, uint32_t weight, Platform::String^ profile)
{
	return create_async(
		[this, channelId, weight, profile]() -> SubscriptionInfo^
	{
		if (!m_workerRunning)
		{
			throw ref new Platform::FailureException(ref new Platform::String(L"Worker is not running"));
		}

		uint32_t subscriptionId = m_subscriptionIdNext++;

		HtsMap map;
		map.setData("method", "subscribe");
		map.setData("channelId", channelId);
		map.setData("subscriptionId", subscriptionId);
		map.setData("queueDepth", 5 * 1024 * 1024);
		map.setData("timeshiftPeriod", (uint32_t)~0);
		map.setData("normts", 1);
		if(weight > 0)
			map.setData("weight", weight);

		if(!profile->IsEmpty())
			map.setData("profile", InteropUtils::MarshalString(profile));

		auto readTask = ReadResult(map.makeMsg());
		HtsMessage res = readTask.get();
		if (!res.isValid())
			return nullptr;


		SubscriptionInfoInternal sii;

		SubscriptionInfo^ si = ref new SubscriptionInfo();

		sii.publicInfo = si;
		sii.audioStreamIndex = 0;
		sii.videoStreamIndex = 0;

		si->audioPresent = false;
		si->videoPresent = false;
		si->subscriptionId = subscriptionId;
		si->channelId = channelId;
		si->timeshiftPeriod = res.getRoot()->getU32("timeshiftPeriod");

		int msgCount = 3;

		while (msgCount > 0)
		{
			if (!m_queue.FindFirst(res, 30000, [&](HtsMessage& msg) {
				std::string method = msg.getRoot()->getStr("method");

				return (
						method == "subscriptionGrace"
						|| method == "subscriptionStatus"
						|| method == "subscriptionStart"
					)
					&& subscriptionId == msg.getRoot()->getU32("subscriptionId");
			}))
			{
				THROW_SYSEXC2(CMessageReadTimeoutException, "Subscribe failed - timeout");
			}


			msgCount--;


			std::string method = res.getRoot()->getStr("method");

			m_logger->Debug("subscribe message {0}",  InteropUtils::MarshalStringUtf8(method.c_str()));

			if (method == "subscriptionGrace")
			{
				DumpMessage(res.getRoot().get());
			}
			else if (method == "subscriptionStatus")
			{
				DumpMessage(res.getRoot().get());
			}
			else if (method == "subscriptionStart")
			{
				DumpMessage(res.getRoot().get());

				static const int sample_rates[16] = {
					96000, 88200, 64000, 48000,
					44100, 32000, 24000, 22050,
					16000, 12000, 11025, 8000,
					7350, 0, 0, 0 };

				std::shared_ptr<HtsList> streams = res.getRoot()->getList("streams");
				if (res.getRoot()->contains("sourceinfo"))
				{
					std::shared_ptr<HtsMap> map = std::static_pointer_cast<HtsMap>(res.getRoot()->getMap("sourceinfo"));
					SourceInfo^ srcInfo = ref new SourceInfo();
					if (map->contains("adapter"))
						srcInfo->adapter = InteropUtils::MarshalStringUtf8(map->getStr("adapter"));
					if (map->contains("mux"))
						srcInfo->mux = InteropUtils::MarshalStringUtf8(map->getStr("mux"));
					if (map->contains("network"))
						srcInfo->network = InteropUtils::MarshalStringUtf8(map->getStr("network"));
					if (map->contains("provider"))
						srcInfo->provider = InteropUtils::MarshalStringUtf8(map->getStr("provider"));
					if (map->contains("service"))
						srcInfo->service = InteropUtils::MarshalStringUtf8(map->getStr("service"));
					if (map->contains("satpos"))
						srcInfo->satpos = InteropUtils::MarshalStringUtf8(map->getStr("satpos"));

					si->sourceInfo = srcInfo;
				}


				for (uint32_t i = 0; i < streams->count(); i++)
				{
					std::shared_ptr<HtsData> sub = streams->getData(i);
					if (!sub->isMap())
						continue;
					std::shared_ptr<HtsMap> map = std::static_pointer_cast<HtsMap>(sub);

					std::string type = map->getStr("type");
					if (type.empty())
						continue;

					if (!map->contains("index"))
						continue;

					uint32_t index = map->getU32("index");

					if ((type == "AC3" || type == "MPEG2AUDIO" || type == "AAC")
						&& sii.audioStreamIndex == 0)
					{
						AudioInfo a;
						a.type = InteropUtils::MarshalStringUtf8(type);
						a.channels = map->getU32("channels");
						if (map->contains("rate"))
							a.rate = sample_rates[map->getU32("rate")];
						else
							a.rate = 0;
						si->audioInfo = a;
						si->audioPresent = true;
						si->audioQueue = ref new HtspQueue();
						sii.audioStreamIndex = index;
					}
					else if (type == "MPEG2VIDEO" || type == "H264")
					{
						VideoInfo v;
						v.type = InteropUtils::MarshalStringUtf8(type);
						v.width = map->getU32("width");
						v.height = map->getU32("height");
						v.aspectRatioNum = map->getU32("aspect_num");
						v.aspectRatioDen = map->getU32("aspect_den");
						si->videoInfo = v;
						si->videoPresent = true;
						si->videoQueue = ref new HtspQueue();
						sii.videoStreamIndex = index;
					}
				}
			}
		}

		m_mtxSubscriptions.lock();
		m_subscriptions.push_back(sii);
		m_mtxSubscriptions.unlock();

		return si;
	});
}

Windows::Foundation::IAsyncOperation<bool>^ HtspClient::UnsubscribeAsync(uint32_t subscriptionId)
{

	/*if (!m_workerRunning)
	{
		return create_async(
			[this]() -> bool
		{
			m_logger->Warn("Can't unsubscribe channel while worker is not running");
			return false;
		});
	}*/

	return create_async(
		[this, subscriptionId]() -> bool
	{

		m_mtxSubscriptions.lock();

		for (auto it = m_subscriptions.begin(); it != m_subscriptions.end(); ++it)
		{
			if (it->publicInfo->subscriptionId == subscriptionId)
			{
				m_subscriptions.erase(it);
				m_mtxSubscriptions.unlock();

				HtsMap map;
				map.setData("method", "unsubscribe");
				map.setData("subscriptionId", subscriptionId);

				m_logger->Info("Unsubscribing subscription {0}", ref new Platform::Box<int>(subscriptionId));

				return TransmitMessage(map.makeMsg()).get();
			}
		}
		m_mtxSubscriptions.unlock();


		m_logger->Info("Unsubscribing failed, subscription not subscribed {0}", ref new Platform::Box<int>(subscriptionId));
		return false;

	});
}


Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVector <ProfileInfo^>^>^ HtspClient::GetProfiles()
{
	return create_async(
		[this]()
	{
		return create_task([this]() -> task<HtsMessage>
		{
			HtsMap map;
			map.setData("method", "getProfiles");

			return ReadResult(map.makeMsg());
		})
			.then([this](task<HtsMessage> prevTask) -> Windows::Foundation::Collections::IVector <ProfileInfo^>^
		{
			HtsMessage res = prevTask.get();

			if (!res.isValid())
				THROW_SYSEXC2(CMessageReadFailedException, "GetSysTime: Error getting profiles - invalid message");

			std::shared_ptr<HtsList> profiles = res.getRoot()->getList("profiles");

			auto ret = ref new Vector<ProfileInfo^>();

			for (uint32_t i = 0; i < profiles->count(); i++)
			{
				std::shared_ptr<HtsData> sub = profiles->getData(i);
				if (!sub->isMap())
					continue;

				std::shared_ptr<HtsMap> map = std::static_pointer_cast<HtsMap>(sub);

				ProfileInfo^ profile = ref new ProfileInfo();

				profile->uuid = InteropUtils::MarshalStringUtf8(map->getStr("uuid"));
				profile->name = InteropUtils::MarshalStringUtf8(map->getStr("name"));
				profile->comment = InteropUtils::MarshalStringUtf8(map->getStr("comment"));

				ret->Append(profile);
			}

			return ret;
		});
	});
}

Windows::Foundation::IAsyncOperation<DateTime>^ HtspClient::GetSysTime()
{
	return create_async(
		[this]()
	{
		return create_task([this]() -> task<HtsMessage>
		{
			HtsMap map;
			map.setData("method", "getSysTime");

			return ReadResult(map.makeMsg());
		})
		.then([this](task<HtsMessage> prevTask) -> DateTime
		{
			HtsMessage res = prevTask.get();

			if (!res.isValid())
				THROW_SYSEXC2(CMessageReadFailedException, "GetSysTime: Error getting time - invalid message");

			int64_t datetime = res.getRoot()->getS64("time");
			uint32_t time_zone_offset = res.getRoot()->getU32("timezone");

			return InteropUtils::UnixTimeToDateTime(datetime);

			/*Windows::Globalization::Calendar^ calendar = ref new Windows::Globalization::Calendar();
			calendar->SetDateTime(dt);
			//calendar->

			return calendar;*/
		});
	});
}


Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVector <EventInfo^> ^> ^  HtspClient::GetEventsAsync(uint32_t eventId, uint32_t channelId, uint32_t numFollowing)
{
	return create_async(
		[this, eventId, channelId, numFollowing]()
	{
		return create_task([this, eventId, channelId, numFollowing]() -> task<HtsMessage>
		{

			//m_logger->Debug("GetEventsAsync start, eventId: {0}", ref new Platform::Box<int>(eventId));
			HtsMap map;
			map.setData("method", "getEvents");
			map.setData("eventId", eventId);
			map.setData("channelId", channelId);
			map.setData("numFollowing", numFollowing);

			return ReadResult(map.makeMsg());
		})
		.then([this, eventId, channelId, numFollowing](task<HtsMessage> prevTask) -> Windows::Foundation::Collections::IVector <EventInfo^> ^
		{
			HtsMessage res = prevTask.get();

			if (!res.isValid())
				THROW_SYSEXC2(CMessageReadFailedException, "GetEventsAsync: invalid message");

			std::shared_ptr<HtsList> events = res.getRoot()->getList("events");

			//std::unordered_map<uint32_t, tmp_channel> channels;

			auto eventsOut = ref new Vector<EventInfo^>();

			for (uint32_t i = 0; i < events->count(); i++)
			{
				std::shared_ptr<HtsData> sub = events->getData(i);
				if (!sub->isMap())
					continue;

				std::shared_ptr<HtsMap> map = std::static_pointer_cast<HtsMap>(sub);

				EventInfo^ event = ref new EventInfo();


				event->eventId = map->getU32("eventId");
				event->title = InteropUtils::MarshalStringUtf8(map->getStr("title"));
				event->summary = InteropUtils::MarshalStringUtf8(map->getStr("summary"));
				event->description = InteropUtils::MarshalStringUtf8(map->getStr("description"));
				event->start = InteropUtils::UnixTimeToDateTime(map->getS64("start"));
				event->stop = InteropUtils::UnixTimeToDateTime(map->getS64("stop"));

				eventsOut->Append(event);
			}


			//m_logger->Debug("GetEventsAsync stop, eventId: {0}", ref new Platform::Box<int>(eventId));

			return eventsOut;
		});
	});
}

Windows::Foundation::IAsyncOperation<HtspFileStream^>^ Wally::HtspClient::HtspClient::FileOpen(Platform::String ^ filePath)
{
	return create_async(
		[this, filePath]()
	{
		return create_task([this, filePath]() -> task<HtsMessage>
		{
			std::string strFilePath = InteropUtils::MarshalString(filePath);

			HtsMap map;
			map.setData("method", "fileOpen");
			map.setData("file", strFilePath);

			return ReadResult(map.makeMsg());
		})
		.then([this](task<HtsMessage> prevTask)->HtspFileStream^
		{
			try
			{
				HtsMessage res = prevTask.get();
				if (!res.isValid())
					return nullptr;

				uint32_t fileId = res.getRoot()->getU32("id");
				uint64_t size = res.getRoot()->getS64("size");
				DateTime mtime = InteropUtils::UnixTimeToDateTime(res.getRoot()->getS64("mtime"));


				HtspFileStream^ stream = ref new HtspFileStream(this, fileId, size, mtime);

				return stream;
			}
			catch (Platform::COMException^ exc)
			{
				m_logger->Warn("Exception in FileOpen {0}", exc->Message);
			}
			catch (CExceptionBase& exc)
			{
				m_logger->Warn("Exception in FileOpen {0}", exc.GetMessage());
			}
			catch (...)
			{
				m_logger->Warn("Exception in FileOpen");
			}
			return nullptr;
		});
	});

}

Windows::Foundation::IAsyncOperation<uint64_t>^ Wally::HtspClient::HtspClient::FileRead(uint32_t fileId, uint64_t size, int64_t offset, Windows::Storage::Streams::IBuffer ^buffer)
{
	return create_async(
		[this, fileId, size, offset, buffer]()
	{
		return create_task([this, fileId, size, offset, buffer]()->task<HtsMessage>
		{
			HtsMap map;
			map.setData("method", "fileRead");
			map.setData("id", fileId);
			map.setData("size", size);
			if(offset != 0)
				map.setData("offset", offset);

			return ReadResult(map.makeMsg());
		})
		.then([this, size, buffer](task<HtsMessage> prevTask)->uint64_t
		{
			try
			{
				HtsMessage res = prevTask.get();
				if (!res.isValid())
					THROW_SYSEXC2(CMessageReadFailedException, "FileRead: Error reading file - invalid message");

				auto buf = InteropUtils::GetPointerToBufferData(buffer);
				uint32_t len = size;
				unsigned char* bin;
				res.getRoot()->getBin("data", &len, (void**)&bin);
				if (len <= buffer->Capacity)
				{
					memcpy(buf, bin, len);
					free(bin);
					buffer->Length = len;
					return len;
				}
				free(bin);
				return 0;

			}
			catch (Platform::COMException^ exc)
			{
				m_logger->Warn("Exception in FileRead {0}", exc->Message);
			}
			catch (...)
			{
				m_logger->Warn("Exception in FileRead");
			}
			return 0;
		});
	});


}
Windows::Foundation::IAsyncOperation<uint64_t>^ Wally::HtspClient::HtspClient::FileSeek(uint32_t fileId, int64_t offset, FileSeekType whence)
{
	return create_async(
		[this, fileId, offset, whence]() -> uint64_t
	{
		HtsMap map;
		map.setData("method", "fileSeek");
		map.setData("id", fileId);
		map.setData("offset", offset);

		if (whence == FileSeekType::Set)
			map.setData("whence", "SEEK_SET");
		else if(whence == FileSeekType::End)
			map.setData("whence", "SEEK_END");
		else
			map.setData("whence", "SEEK_CUR");

		try
		{
			HtsMessage res = ReadResult(map.makeMsg()).get();
			if (!res.isValid())
				THROW_SYSEXC2(CMessageReadFailedException, "FileSeek: Error seeking file - invalid message");

			uint64_t absOffset = res.getRoot()->getS64("offset");
			
			return absOffset;

		}
		catch (Platform::COMException^ exc)
		{
			m_logger->Warn("Exception in FileSeek {0}", exc->Message);
		}
		catch (...)
		{
			m_logger->Warn("Exception in FileSeek");
		}
		return 0;
	}); 
}
Windows::Foundation::IAsyncOperation<bool>^ Wally::HtspClient::HtspClient::FileClose(uint32_t fileId)
{
	return create_async(
		[this, fileId]() -> bool
	{
		HtsMap map;
		map.setData("method", "fileClose");
		map.setData("id", fileId);

		try
		{
			HtsMessage res = ReadResult(map.makeMsg()).get();
			if (!res.isValid())
				THROW_SYSEXC2(CMessageReadFailedException, "FileClose: Error closing file - invalid message");

			return true;

		}
		catch (Platform::COMException^ exc)
		{
			m_logger->Warn("Exception in FileClose {0}", exc->Message);
		}
		catch (...)
		{
			m_logger->Warn("Exception in FileClose");
		}
		return false;
	});
}
uint32_t HtspClient::HTSPNextSeqNum()
{
	uint32_t res = m_nextSeqNum++;
	if (m_nextSeqNum > 2147483647)
		m_nextSeqNum = 1;
	return res;
}
void HtspClient::ParseMuxPacket(HtsMessage &msg)
{
	//DumpMessage(msg.getRoot().get());

	uint32_t index = msg.getRoot()->getU32("stream");
	uint32_t subs = msg.getRoot()->getU32("subscriptionId");


	m_mtxSubscriptions.lock();

	for (auto it = m_subscriptions.begin(); it != m_subscriptions.end(); ++it)
	{
		if (it->publicInfo->subscriptionId == subs)
		{
			if (index == it->audioStreamIndex || index == it->videoStreamIndex)
			{
				HtspMediaSample sample;

				msg.getRoot()->getBin("payload", &sample.bufferSize, (void**)&sample.buffer);
				uint32_t duration = msg.getRoot()->getU32("duration");
				int64_t pts = msg.getRoot()->getS64("pts");
				int64_t dts = msg.getRoot()->getS64("dts");

				sample.presentTS = pts * 10;

				sample.decodeTS = dts * 10;

				sample.duration = duration * 10;

				/*sample.bufferSize = binlen;
				sample.buffer = new unsigned char[binlen];
				memcpy(sample.buffer, bin, binlen);
				free(bin);*/

				if (index == it->videoStreamIndex)
				{
					uint32_t frametype = msg.getRoot()->getU32("frametype");
					char ft = (char)frametype;

					sample.isKey = (ft == 'I');

					it->publicInfo->videoQueue->PutSample(sample);
				}
				else if (index == it->audioStreamIndex)
				{
					sample.isKey = true;
					it->publicInfo->audioQueue->PutSample(sample);
				}
			}
			break;
		}
	}
	m_mtxSubscriptions.unlock();

}

TagInfo^ HtspClient::MakeTagInfoFromMsg(const HtsMessage & msg)
{
	uint32_t tagId = msg.getRoot()->getU32("tagId");
	std::string tagName = msg.getRoot()->getStr("tagName");

	/*std::shared_ptr<HtsList> chList = m.getRoot()->getList("members");
	for (uint32_t i = 0; i < chList->count(); ++i)
	channels[chList->getData(i)->getU32()].tags.push_back(tagName);*/
	TagInfo^ tag = ref new TagInfo();

	tag->tagId = tagId;
	tag->tagName = InteropUtils::MarshalStringUtf8(tagName);

	return tag;

}

DVRInfo ^ Wally::HtspClient::HtspClient::MakeDVREventInfoFromMsg(const HtsMessage & msg)
{
	DVRInfo^ dvr = ref new DVRInfo();

	uint32_t dvrId = msg.getRoot()->getU32("id");
	std::string path = msg.getRoot()->getStr("path");
	std::string title = msg.getRoot()->getStr("title");
	std::string state = msg.getRoot()->getStr("state");

	dvr->dvrId = dvrId;
	dvr->channelId = msg.getRoot()->getU32("channel");

	dvr->path = InteropUtils::MarshalStringUtf8(path);
	dvr->title = InteropUtils::MarshalStringUtf8(title);

	if (state == "scheduled")
	{
		dvr->state = DvrState::Scheduled;
	}
	else if(state == "completed")
	{
		dvr->state = DvrState::Completed;
	}
	else if (state == "recording")
	{
		dvr->state = DvrState::Recording;
	}
	else if (state == "missed")
	{
		dvr->state = DvrState::Missed;
	}
	else if (state == "invalid")
	{
		dvr->state = DvrState::Invalid;
	}
	else
	{
		dvr->state = DvrState::Unknown;
	}


	dvr->start = InteropUtils::UnixTimeToDateTime(msg.getRoot()->getS64("start"));
	dvr->stop = InteropUtils::UnixTimeToDateTime(msg.getRoot()->getS64("stop"));
	dvr->startExtra = InteropUtils::UnixTimeToDateTime(msg.getRoot()->getS64("startExtra"));
	dvr->stopExtra = InteropUtils::UnixTimeToDateTime(msg.getRoot()->getS64("stopExtra"));

	return dvr;
}

ChannelInfo^ HtspClient::MakeChannelInfoFromMsg(const HtsMessage & msg)
{
	uint32_t cid = msg.getRoot()->getU32("channelId");

	std::string cname = msg.getRoot()->getStr("channelName");

	uint32_t cnum = msg.getRoot()->getU32("channelNumber");

	std::string cicon = msg.getRoot()->getStr("channelIcon");
	uint32_t eventId = msg.getRoot()->getU32("eventId");
	uint32_t nextEventId = msg.getRoot()->getU32("nextEventId");

	ChannelInfo^ chl = ref new ChannelInfo();
	chl->name = InteropUtils::MarshalStringUtf8(cname);
	chl->channelId = cid;
	chl->number = cnum;
	chl->icon = InteropUtils::MarshalStringUtf8(cicon);
	chl->eventId = eventId;
	chl->nextEventId = nextEventId;
	

	std::shared_ptr<HtsList> tagList = msg.getRoot()->getList("tags");
	chl->tags = ref new Platform::Array<uint32_t>(tagList->count());

	for (uint32_t i = 0; i < tagList->count(); ++i)
		chl->tags[i] = tagList->getData(i)->getU32();

	return chl;
}

void HtspClient::DumpMessage(HtsData* m)
{
	std::ostringstream msg;
	DumpMessage(m, msg);
	m_logger->Trace("Message dump: {0}", InteropUtils::MarshalStringUtf8(msg.str()));
}

void HtspClient::DumpMessage(HtsData* m, std::ostringstream& msg)
{
	if (m->isMap())
	{
		auto map = ((HtsMap*)m)->getRawData();
		msg << " (M) {\n";
		for (auto it = map.begin(); it != map.end(); it++)
		{
			msg << it->first;
			DumpMessage(it->second.get(), msg);
		}
		msg << "}";
	}
	else if (m->isList())
	{
		HtsList* list = (HtsList*)m;
		msg << " (L " << list->count() << ") {\n" ;
		for (uint32_t i = 0; i < list->count(); i++)
		{
			DumpMessage(list->getData(i).get(), msg);
		}
		msg << "}";
	}
	else if (m->isInt())
	{
		msg << "(I): " << m->getU32();
	}
	else if (m->isStr())
	{
		msg << "(S): " << m->getStr();
	}
	else if (m->isBin())
	{
		uint32_t len;
		void* buf;
		m->getBin(&len, &buf);
		msg << "(B): " << len;
	}
	msg << "\n";
}


/*
	Return true if message was processed
*/

bool HtspClient::ProcessEventMsg(HtsMessage msg)
{
	std::string method = msg.getRoot()->getStr("method");

	std::function<void()> f_process;

	if (method == "queueStatus")
	{
		f_process = [this, msg]()
		{
			QueueStatusInfo^ qinfo = ref new QueueStatusInfo();

			std::shared_ptr<HtsMap> map = msg.getRoot();

			qinfo->subscriptionId = map->getU32("subscriptionId");
			qinfo->bytes = map->getU32("bytes");
			qinfo->packets = map->getU32("packets");
			qinfo->Idrops = map->getU32("Idrops");
			qinfo->Pdrops = map->getU32("Pdrops");
			qinfo->Bdrops = map->getU32("Bdrops");
			qinfo->delay = map->getU32("delay");
			qinfo->delta = map->getS64("delta");
			QueueStatusEvent(this, qinfo);
		};
	}
	else if (method == "signalStatus")
	{
		f_process = [this, msg]()
		{
			SignalStatusInfo^ qinfo = ref new SignalStatusInfo();

			std::shared_ptr<HtsMap> map = msg.getRoot();

			//DumpMessage(msg.getRoot().get());

			qinfo->subscriptionId = map->getU32("subscriptionId");
			qinfo->feStatus = InteropUtils::MarshalStringUtf8(map->getStr("feStatus"));
			qinfo->feSNR = map->getU32("feSNR");
			qinfo->feSignal = map->getU32("feSignal");
			qinfo->feBER = map->getU32("feBER");
			qinfo->feUNC = map->getU32("feUNC");
			SignalStatusEvent(this, qinfo);

			m_mtxSubscriptions.lock();

			for (auto it = m_subscriptions.begin(); it != m_subscriptions.end(); ++it)
			{
				if (it->publicInfo->subscriptionId == qinfo->subscriptionId)
				{
					it->publicInfo->signalStatus = qinfo;
					break;
				}
			}
			m_mtxSubscriptions.unlock();
		};

	}
	else if (method == "channelUpdate")
	{
		f_process = [this, msg]()
		{
			if (msg.getRoot()->contains("channelId"))
			{

				ChannelInfo^ chl = MakeChannelInfoFromMsg(msg);

				ChannelUpdateEvent(this, chl);
			}
		};
	}
	else if (method == "timeshiftStatus")
	{
		//TODO
		f_process = []()
		{
		};
	}
	else if (method == "tagAdd" || method == "tagUpdate" || method == "tagDelete")
	{
		f_process = [method, this, msg]()
		{
			if (msg.getRoot()->contains("tagId"))
			{
				auto tag = MakeTagInfoFromMsg(msg);

				if (method == "tagAdd")
					tag->operation = Operation::New;
				else if (method == "tagDelete")
					tag->operation = Operation::Delete;
				else
					tag->operation = Operation::Update;

				TagUpdateEvent(this, tag);
			}
		};
	}
	else if (method == "dvrEntryAdd" || method == "dvrEntryUpdate" || method == "dvrEntryDelete")
	{
		f_process = [method, this, msg]()
		{
			if (msg.getRoot()->contains("id"))
			{
				auto dvr = MakeDVREventInfoFromMsg(msg);

				if (method == "dvrEntryAdd")
					dvr->operation = Operation::New;
				else if (method == "dvrEntryDelete")
					dvr->operation = Operation::Delete;
				else
					dvr->operation = Operation::Update;

				DVREntryUpdateEvent(this, dvr);
			}
		};
	}
	else
	{
		return false;
	}

	task<void> task1(f_process);


	return true;
}

void HtspClient::StartWorker()
{
	m_workItemCompleted.reset();

	auto workItem = ref new WorkItemHandler(
		[this](IAsyncAction^ workItem)
	{

		m_workerRunning = true;
		while (m_workerRunning)
		{
			try
			{
				
				// Check for cancellation. 
				if (workItem->Status == Windows::Foundation::AsyncStatus::Canceled)
				{
					this->m_logger->Debug("Exiting StartWorker procedure");

					m_workItemCompleted.set();

					break;
				}
				else
				{
					//this->m_logger->Trace("worker - waiting for new msg");
					HtsMessage msg = ReadMessage(true).get();

					if (!msg.isValid())
					{
						this->m_logger->Warn("Got invalid message");
						continue;
					}
					
					std::string method = msg.getRoot()->getStr("method");

					if (method == "muxpkt")
					{
						//this->m_logger->Trace("worker - got muxpkt stream {0}", ref new Platform::Box<int>(msg.getRoot()->getU32("stream")));
						ParseMuxPacket(msg);
					}
					else if(!ProcessEventMsg(msg))
					{
						/*std::string str("worker message ");
						str.append(method.c_str());
						if (msg.getRoot()->contains("seq"))
						{
							str.append(" seq ");
							str.append(std::to_string(msg.getRoot()->getU32("seq")));
						}
						m_logger->Debug(InteropUtils::MarshalStringUtf8(str));*/

						m_queue.EnqueueMsg(msg);
					}
				}
			}
			catch (CDisconnectedException& exc)
			{
				m_logger->Warn("Disconnection in worker in Worker {0}", exc.GetMessage());
				m_workerRunning = false;
			}
			catch (Wally::CExceptionBase& exc)
			{
				m_logger->Warn("Exception in Worker {0}", exc.GetMessage());
			}
			catch (Platform::COMException^ exc)
			{
				if (exc->HResult == ERROR_OPERATION_ABORTED 
					|| exc->HResult == ERROR_INVALID_OPERATION
					|| exc->HResult == WSAECONNRESET
					|| exc->HResult == RO_E_CLOSED)
				{
					m_workerRunning = false;
				}
				m_logger->Warn("Exception in Worker {0}", exc->Message);
				m_logger->Warn("Exception in Worker - toString: {0}", exc->ToString());
			}
			catch (Concurrency::task_canceled& exc)
			{
				m_workerRunning = false;
				m_logger->Warn("Task cancelled {0}",  InteropUtils::MarshalStringUtf8(exc.what()));
			}
			catch (...)
			{
				m_logger->Warn("Exception in Worker");
			}
		}

		m_workerRunning = false;

		DisconnectedEvent(this);

	}, Platform::CallbackContext::Any);

	m_workItem = ThreadPool::RunAsync(workItem, WorkItemPriority::High, WorkItemOptions::TimeSliced);


}

Windows::Foundation::IAsyncOperation<bool>^ HtspClient::StopWorker()
{
	return create_async(
		[this]() -> bool
	{
		if (!m_workerRunning)
			return false;
		
		//stop all
		m_ctsWorker.cancel();

		//set workitem as canceled
		if (m_workItem != nullptr)
		{
			m_workItem->Cancel();

			m_workItemCompleted.wait();

			m_workItem = nullptr;
		}

		return !m_workerRunning;
	});
}

task<bool> HtspClient::TransmitMessage(HtsMessage m)
{

	try {
		void *buf;
		uint32_t len;


		if (!m.Serialize(&len, &buf))
		{
			THROW_SYSEXC2(CMessageReadFailedException, "TransmitMessage: serialize failed");
		}

		// unmanaged buffer
		auto _buffer = InteropUtils::ToArray((unsigned char*)buf, len);

		free(buf);


		m_numReadWriteOperations++;
		std_ext::scope_guard rwOpGuard = [this]()
		{
			m_numReadWriteOperationsCV.notify_all();
			m_numReadWriteOperations--;
		};

		if (m_writer == nullptr)
		{
			THROW_SYSEXC2(CMessageReadFailedException, "TransmitMessage: not connected");
		}

		m_writer->WriteBytes(_buffer);

		task<unsigned int> storeTask = create_task(m_writer->StoreAsync(), m_ctsWorker.get_token());

		return storeTask.then([](unsigned int result)
		{
			return result > 0;
		});
	}
	catch (Platform::COMException^ exc)
	{
		m_logger->Warn("Exception in TransmitMessage {0}", exc->Message);
	}
	catch (Concurrency::task_canceled& exc)
	{
		m_logger->Warn("TransmitMessage: Task cancelled {0}", InteropUtils::MarshalStringUtf8(exc.what()));
	}
	catch (...)
	{
		m_logger->Warn("Exception in TransmitMessage");
	}

	m_logger->Warn("TransmitMessage: failed");
	return create_task([]() -> bool
	{
		return false;
	});


}

task<HtsMessage> HtspClient::ReadMessageThen(bool bFromWorker)
{
	return create_task([this, bFromWorker]() -> task<size_t>
	{
		task<size_t> t1;
		if (bFromWorker)
			t1 = create_task(m_reader->LoadAsync(sizeof(uint32_t)), m_ctsWorker.get_token());
		else
			t1 = create_task(m_reader->LoadAsync(sizeof(uint32_t)));

		return t1;
	}, bFromWorker ? task_continuation_context::use_current() : task_continuation_context::use_default())
	.then([this, bFromWorker](task<size_t> prevTask) -> task<size_t>
	{
		int count = 0;
		
		try {
			count = prevTask.get();
		}
		catch (Concurrency::task_canceled exc)
		{
			m_logger->Warn("ReadMessage: LoadAsync canceled");
			return task<size_t>([]() {return 0; });
		}

		uint32_t len = 0;

		if (count == sizeof(uint32_t))
			len = m_reader->ReadUInt32();
		else
			THROW_SYSEXC2(CDisconnectedException, "ReadMessage: disconnected");


		//len = ntohl(len);
		if (len == 0)
		{
			m_logger->Warn("ReadMessage: zero size");
			return task<size_t>([]() {return 0; });
		}

		unsigned char *buf;
		buf = nullptr;

		task<size_t> t2;
		if (bFromWorker)
			t2 = create_task(m_reader->LoadAsync(len), m_ctsWorker.get_token());
		else
			t2 = create_task(m_reader->LoadAsync(len));

		return t2;

	}, bFromWorker ? task_continuation_context::use_current() : task_continuation_context::use_default())
	.then([this, bFromWorker](task<size_t> prevTask) -> HtsMessage
	{

		int count = 0;

		try {
			count = prevTask.get();
		}
		catch (Concurrency::task_canceled exc)
		{
			m_logger->Warn("ReadMessage: LoadAsync2 canceled");
		}			
			
		if (count == 0)
		{
			return HtsMessage();
		}

		std::shared_ptr<unsigned char> buf2;

		auto _buffer = ref new Array<unsigned char>(count);

		/*
		if(len == count)
		{
		*/
		m_reader->ReadBytes(_buffer);

		HtsMessage result = HtsMessage::Deserialize(count, _buffer->Data);
		delete _buffer;
		return result;

/*		}
		else
		{
			m_logger->Warn("ReadMessage: message wrong size");
			return HtsMessage();
		}*/
	}, bFromWorker ? task_continuation_context::use_current() : task_continuation_context::use_default());

}

task<HtsMessage> HtspClient::ReadMessage(bool bFromWorker)
{
	try
	{

		if (m_reader == nullptr)
		{
			THROW_SYSEXC2(CMessageReadFailedException, "TransmitMessage: not connected");
		}

		if (!bFromWorker)
		{
			return create_task([this, bFromWorker]() -> task<HtsMessage>
			{
				HtsMessage res;

				if (m_workerRunning)
				{
					if (m_queue.GetMsg(res, 3000))
						return MakeTaskMessage(res);
					else
						THROW_SYSEXC2(CMessageReadTimeoutException, "ReadMessage: worker message read timeout");
				}
				else
				{
					if (m_queue.GetMsg(res, 0))
					{
						return MakeTaskMessage(res);
					}
				}

				return ReadMessageThen(bFromWorker);
			});
		}

		return ReadMessageThen(bFromWorker);
	}
	catch (Platform::COMException^ exc)
	{
		m_logger->Warn("Exception in ReadMessage {0}", exc->Message);
	}
	catch (Concurrency::task_canceled& exc)
	{
		m_logger->Warn("ReadMessage: Task cancelled {0}", InteropUtils::MarshalStringUtf8(exc.what()));
	}
	catch (...)
	{
		m_logger->Warn("Exception in ReadMessage");
	}


	return MakeTaskMessage(HtsMessage());

}

task<HtsMessage> HtspClient::ReadResult_Finish(HtsMessage m)
{
	return create_task([this, m]() -> HtsMessage
	{
		if (!m.isValid())
		{
			THROW_SYSEXC2(CMessageReadFailedException, "ReadResult: message not valid");
		}

		if (m.getRoot()->contains("error"))
		{
			DumpMessage(m.getRoot().get());
			//THROW_SYSEXC2(CMessageReadFailedException, InteropUtils::MarshalStringUtf8(m.getRoot()->getStr("error")));
			return HtsMessage();
		}
		if (m.getRoot()->getU32("noaccess") != 0)
		{
			THROW_SYSEXC1(CAccessDeniedException);
		}

		return m;
	});
}

task<HtsMessage> HtspClient::ReadResult_Repeat(uint32_t iSequence, bool repeat)
{
	return create_task( ReadMessage())
	.then([this, iSequence, repeat](task<HtsMessage> prevTask) -> task<HtsMessage>
	{
		HtsMessage m = prevTask.get();

		if (!m.isValid())
		{
			return MakeTaskMessage(m);
		}

		if (!repeat || m.getRoot()->contains("seq") && m.getRoot()->getU32("seq") == iSequence)
		{
			return MakeTaskMessage(m);
		}

		m_queue.EnqueueMsg(m);

		return ReadResult_Repeat(iSequence, repeat);
	});
}
task<HtsMessage> HtspClient::ReadResult(HtsMessage m, bool sequence)
{
	std::shared_ptr<uint32_t> iSequence = std::make_shared<uint32_t>(0);

	if (sequence)
	{
		*iSequence = HTSPNextSeqNum();
		m.getRoot()->setData("seq", *iSequence);
	}

	//m_logger->Trace("ReadResult: Transmit msg {}", ref new Platform::Box<int>(*iSequence));


	task<bool> transmitTask = TransmitMessage(m);

	return transmitTask.then([this, sequence, iSequence, m] (task<bool> prevtask) -> task<HtsMessage>
	{
		bool result = false;
		HtsMessage msg = m;

		try
		{
			bool result = prevtask.get();

			if (!result)
			{
				THROW_SYSEXC2(CMessageReadFailedException, "ReadResult: TransmitMessage failed, result false");
			}
		}
		catch (Platform::COMException^ exc)
		{
			m_logger->Warn("Exception in ReadResult {0}", exc->Message);
		}
		catch (std::exception& ex)
		{
			THROW_SYSEXC2(CMessageReadFailedException, "ReadResult: TransmitMessage failed");
		}
		catch (...)
		{
			m_logger->Warn("Exception in ReadResult");
		}
		//m_logger->Trace("ReadResult: Transmited {}", ref new Platform::Box<int>(*iSequence));

		if (m_workerRunning)
		{
			if (!m_queue.FindFirstSequence(msg, 10000, *iSequence))
			{
				THROW_SYSEXC2(CMessageReadTimeoutException, "ReadResult: seq message read timeout");
			}

		}
		else
		{
			return ReadResult_Repeat(*iSequence, sequence).then([this](task<HtsMessage> prevTask) -> task<HtsMessage>
			{
				return ReadResult_Finish(prevTask.get());
			});
		}


		return ReadResult_Finish(msg);


	});


}





