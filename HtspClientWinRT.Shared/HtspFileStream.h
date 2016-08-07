#pragma once
#include "windows.storage.streams.h"
#include "HtspClient.h"

namespace Wally
{
	namespace HtspClient
	{

		public ref class HtspFileStream sealed :
			public Windows::Storage::Streams::IRandomAccessStream
		{
		private:
			HtspClient^ m_htspClient;
			uint32_t m_fileId;
			uint64_t m_size;
			uint64_t m_pos;
			Windows::Foundation::DateTime m_mtime;

			friend ref class HtspClient;
		protected private:
			HtspFileStream(HtspClient^ htspClient, uint32_t fileId, uint64_t size, Windows::Foundation::DateTime mtime);
		public:
			virtual ~HtspFileStream();

			// Inherited via IRandomAccessStream
			virtual Windows::Foundation::IAsyncOperationWithProgress<Windows::Storage::Streams::IBuffer ^, unsigned int> ^ ReadAsync(Windows::Storage::Streams::IBuffer ^buffer, unsigned int count, Windows::Storage::Streams::InputStreamOptions options);
			virtual Windows::Foundation::IAsyncOperationWithProgress<unsigned int, unsigned int> ^ WriteAsync(Windows::Storage::Streams::IBuffer ^buffer);
			virtual Windows::Foundation::IAsyncOperation<bool> ^ FlushAsync();
			virtual property bool CanRead
			{
				bool get() { return true; }
			}
			virtual property bool CanWrite
			{
				bool get() { return false; }
			}
			virtual property unsigned long long Position
			{
				unsigned long long get() { return m_pos; }
			}
			virtual property unsigned long long Size
			{
				unsigned long long get() { return m_size; }
				void set(unsigned long long) {  }
			}
			virtual Windows::Storage::Streams::IInputStream ^ GetInputStreamAt(unsigned long long position);
			virtual Windows::Storage::Streams::IOutputStream ^ GetOutputStreamAt(unsigned long long position);
			virtual void Seek(unsigned long long position);
			virtual Windows::Storage::Streams::IRandomAccessStream ^ CloneStream();
		};

	}
}