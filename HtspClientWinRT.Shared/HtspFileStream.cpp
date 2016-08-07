#include "HtspFileStream.h"

#include <ppltasks.h>

using namespace Concurrency;
using namespace Wally::HtspClient;


HtspFileStream::HtspFileStream(HtspClient^ htspClient, uint32_t fileId, uint64_t size, Windows::Foundation::DateTime mtime) :
m_htspClient(htspClient),
m_fileId(fileId),
m_size(size),
m_mtime(mtime)
{
	m_pos = 0;
}


HtspFileStream::~HtspFileStream()
{
	auto task1 = create_task(m_htspClient->FileClose(m_fileId));

	task1.wait();

	task1.get();
}

Windows::Foundation::IAsyncOperationWithProgress<Windows::Storage::Streams::IBuffer ^, unsigned int> ^ HtspFileStream::ReadAsync(Windows::Storage::Streams::IBuffer ^buffer, unsigned int count, Windows::Storage::Streams::InputStreamOptions options)
{
	return create_async([this, count, buffer] (progress_reporter<unsigned int> reporter)
	{
		return create_task(m_htspClient->FileRead(m_fileId, count, 0, buffer))
			.then([buffer](task<uint64_t> prevTask) -> Windows::Storage::Streams::IBuffer^
		{
			prevTask.get();
			return buffer;
		});
	});
}

Windows::Foundation::IAsyncOperationWithProgress<unsigned int, unsigned int> ^ HtspFileStream::WriteAsync(Windows::Storage::Streams::IBuffer ^buffer)
{
	throw ref new Platform::NotImplementedException();
}

Windows::Foundation::IAsyncOperation<bool> ^ HtspFileStream::FlushAsync()
{
	throw ref new Platform::NotImplementedException();
}

Windows::Storage::Streams::IInputStream ^ HtspFileStream::GetInputStreamAt(unsigned long long position)
{
	throw ref new Platform::NotImplementedException();
}

Windows::Storage::Streams::IOutputStream ^ HtspFileStream::GetOutputStreamAt(unsigned long long position)
{
	throw ref new Platform::NotImplementedException();
}

void HtspFileStream::Seek(unsigned long long position)
{
	if (position > m_size)
		throw ref new Platform::InvalidArgumentException("position beyond end");

	auto task1 = create_task(m_htspClient->FileSeek(m_fileId, position, FileSeekType::Set));

	task1.wait();

	m_pos = task1.get();
}

Windows::Storage::Streams::IRandomAccessStream ^ HtspFileStream::CloneStream()
{
	throw ref new Platform::NotImplementedException();
}
