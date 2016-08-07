#pragma once


#include <wrl.h>
#include <wrl/client.h>
#include <agile.h>
#include <string>
#include <cstdint>
#include "HtspQueue.h"

namespace Wally
{
	namespace HtspClient
	{
		struct ServerInfo
		{
			std::string serverName;
			std::string serverVersion;
			uint32_t protoVersion;
		};


		public ref class ChannelInfo sealed
		{
		public:
			property uint32_t channelId;
			property Platform::String^ name;
			property uint32_t number;
			property Platform::String^ icon;
			property Platform::Array<uint32_t>^ tags;
			property uint32_t eventId;
			property uint32_t nextEventId;
		};


		public value struct AudioInfo sealed
		{
		public:
			Platform::String^ type;
			uint32_t channels;
			uint32_t rate;
		};

		public value struct VideoInfo sealed
		{
		public:
			Platform::String^ type;
			uint32_t width;
			uint32_t height;
			uint32_t aspectRatioNum;
			uint32_t aspectRatioDen;
		};


		public ref class SourceInfo sealed
		{
		public:
			property Platform::String^ adapter;
			property Platform::String^ mux;
			property Platform::String^ network;
			property Platform::String^ provider;
			property Platform::String^ service;
			property Platform::String^ satpos;
		};
		
		public ref class SignalStatusInfo sealed
		{
		public:
			property uint32_t subscriptionId;
			property Platform::String^ feStatus;
			property uint32_t feSNR;
			property uint32_t feSignal;
			property uint32_t feBER;
			property uint32_t feUNC;
		};

		public ref class ProfileInfo sealed
		{
		public:
			property Platform::String^ uuid;
			property Platform::String^ name;
			property Platform::String^ comment;
		};

		public enum class Operation
		{
			New,
			Update,
			Delete
		};


		public enum class FileSeekType
		{
			Set,
			Current,
			End
		};


		public ref class SubscriptionInfo sealed
		{
		private:
			bool audioPresent = false;
			bool videoPresent = false;

		public:
			property uint32_t subscriptionId;
			property uint32_t channelId;
			property uint32_t timeshiftPeriod;
			property SignalStatusInfo^ signalStatus;
			property HtspQueue^ audioQueue;
			property HtspQueue^ videoQueue;
			property AudioInfo audioInfo;
			property VideoInfo videoInfo;
			property SourceInfo^ sourceInfo;
			property bool IsAudioPresent { bool get() { return audioPresent; } };
			property bool IsVideoPresent { bool get() { return videoPresent; } };

			friend ref class HtspClient;
		};


		public ref class EventInfo sealed
		{
		public:
			property uint32_t eventId;
			property uint32_t channelId;
			property Windows::Foundation::DateTime start;
			property Windows::Foundation::DateTime stop;
			property Platform::String^ title;
			property Platform::String^ summary;
			property Platform::String^ description;
			property uint32_t dvrId;

		};


		public enum class DvrState
		{
			Scheduled,
			Completed,
			Recording,
			Missed,
			Invalid,
			Unknown
		};

		public ref class DVRInfo sealed
		{
		public:
			property Operation operation;
			property uint32_t dvrId;
			property uint32_t channelId;
			property Windows::Foundation::DateTime start;
			property Windows::Foundation::DateTime stop;
			property Windows::Foundation::DateTime startExtra;
			property Windows::Foundation::DateTime stopExtra;
			property int64_t retention;
			property uint32_t priority;
			property uint32_t eventId;
			property Platform::String^ autorecId;
			property Platform::String^ timerecId;
			property uint32_t contentType;
			property Platform::String^ title;
			property Platform::String^ subtitle;
			property Platform::String^ summary;
			property Platform::String^ description;
			property DvrState state;
			property Platform::String^ path;
			property int64_t dataSize;
			property uint32_t enabled;

		};



		public ref class TagInfo sealed
		{
		public:
			property Operation operation;
			property uint32_t tagId;
			property uint32_t tagIndex;
			property Platform::String^ tagName;
			property Platform::String^ tagIcon;
			property uint32_t tagTitledIcon;

		};

		public ref class QueueStatusInfo sealed
		{
		public:
			property uint32_t subscriptionId;
			property uint32_t packets;
			property uint32_t bytes;
			property uint32_t delay;
			property uint32_t Bdrops;
			property uint32_t Pdrops;
			property uint32_t Idrops;
			property int64_t delta;

		};
	}
}