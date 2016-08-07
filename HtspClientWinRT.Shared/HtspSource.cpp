#include <new>
#include "HtspSource.h"


//ActivatableClass(CHtspSource);

/* Public class methods */

//-------------------------------------------------------------------
// Name: CreateInstance
// Static method to create an instance of the source.
//-------------------------------------------------------------------

ComPtr<CHtspSource> CHtspSource::CreateInstance()
{
	ComPtr<CHtspSource> spSource;

	spSource.Attach(new (std::nothrow) CHtspSource());
	if (spSource == nullptr)
	{
		throw ref new OutOfMemoryException();
	}

	return spSource;
}


//-------------------------------------------------------------------
// IUnknown methods
//-------------------------------------------------------------------

HRESULT CHtspSource::QueryInterface(REFIID riid, void **ppv)
{
	if (ppv == nullptr)
	{
		return E_POINTER;
	}

	HRESULT hr = E_NOINTERFACE;
	if (riid == IID_IUnknown ||
		riid == IID_IMFMediaEventGenerator ||
		riid == IID_IMFMediaSource)
	{
		(*ppv) = static_cast<IMFMediaSource *>(this);
		AddRef();
		hr = S_OK;
	}
	else if (riid == IID_IMFGetService)
	{
		(*ppv) = static_cast<IMFGetService*>(this);
		AddRef();
		hr = S_OK;
	}
	/*else if (riid == IID_IMFRateControl)
	{
		(*ppv) = static_cast<IMFRateControl*>(this);
		AddRef();
		hr = S_OK;
	}*/

	return hr;
}

ULONG CHtspSource::AddRef()
{
	return _InterlockedIncrement(&m_cRef);
}

ULONG CHtspSource::Release()
{
	LONG cRef = _InterlockedDecrement(&m_cRef);
	if (cRef == 0)
	{
		delete this;
	}
	return cRef;
}

//-------------------------------------------------------------------
// IMFMediaEventGenerator methods
//
// All of the IMFMediaEventGenerator methods do the following:
// 1. Check for shutdown status.
// 2. Call the event queue helper object.
//-------------------------------------------------------------------

HRESULT CHtspSource::BeginGetEvent(IMFAsyncCallback *pCallback, IUnknown *punkState)
{
	HRESULT hr = S_OK;

	AutoLock lock(m_critSec);

	hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{
		hr = m_spEventQueue->BeginGetEvent(pCallback, punkState);
	}

	return hr;
}

HRESULT CHtspSource::EndGetEvent(IMFAsyncResult *pResult, IMFMediaEvent **ppEvent)
{
	HRESULT hr = S_OK;

	AutoLock lock(m_critSec);

	hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{
		hr = m_spEventQueue->EndGetEvent(pResult, ppEvent);
	}

	return hr;
}

HRESULT CHtspSource::GetEvent(DWORD dwFlags, IMFMediaEvent **ppEvent)
{
	// NOTE:
	// GetEvent can block indefinitely, so we don't hold the lock.
	// This requires some juggling with the event queue pointer.

	HRESULT hr = S_OK;

	ComPtr<IMFMediaEventQueue> spQueue;

	{
		AutoLock lock(m_critSec);

		// Check shutdown
		hr = CheckShutdown();

		// Get the pointer to the event queue.
		if (SUCCEEDED(hr))
		{
			spQueue = m_spEventQueue;
		}
	}

	// Now get the event.
	if (SUCCEEDED(hr))
	{
		hr = spQueue->GetEvent(dwFlags, ppEvent);
	}

	return hr;
}

HRESULT CHtspSource::QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT *pvValue)
{
	HRESULT hr = S_OK;

	AutoLock lock(m_critSec);

	hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{
		hr = m_spEventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
	}

	return hr;
}


//-------------------------------------------------------------------
// IMFMediaSource methods
//-------------------------------------------------------------------


//-------------------------------------------------------------------
// CreatePresentationDescriptor
// Returns a shallow copy of the source's presentation descriptor.
//-------------------------------------------------------------------

HRESULT CHtspSource::CreatePresentationDescriptor(
	IMFPresentationDescriptor **ppPresentationDescriptor
	)
{
	if (ppPresentationDescriptor == nullptr)
	{
		return E_POINTER;
	}

	HRESULT hr = S_OK;

	AutoLock lock(m_critSec);

	// Fail if the source is shut down.
	hr = CheckShutdown();

	// Fail if the source was not initialized yet.
	if (SUCCEEDED(hr))
	{
		hr = IsInitialized();
	}

	// Do we have a valid presentation descriptor?
	if (SUCCEEDED(hr))
	{
		if (m_spPresentationDescriptor == nullptr)
		{
			hr = MF_E_NOT_INITIALIZED;
		}
	}

	// Clone our presentation descriptor.
	if (SUCCEEDED(hr))
	{
		hr = m_spPresentationDescriptor->Clone(ppPresentationDescriptor);
	}

	return hr;
}


//-------------------------------------------------------------------
// GetCharacteristics
// Returns capabilities flags.
//-------------------------------------------------------------------

HRESULT CHtspSource::GetCharacteristics(DWORD *pdwCharacteristics)
{
	if (pdwCharacteristics == nullptr)
	{
		return E_POINTER;
	}

	HRESULT hr = S_OK;

	AutoLock lock(m_critSec);

	hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{
		*pdwCharacteristics = MFMEDIASOURCE_CAN_PAUSE;
	}

	// NOTE: This sample does not implement seeking, so we do not
	// include the MFMEDIASOURCE_CAN_SEEK flag.
	return hr;
}


//-------------------------------------------------------------------
// Pause
// Pauses the source.
//-------------------------------------------------------------------

HRESULT CHtspSource::Pause()
{
	AutoLock lock(m_critSec);

	HRESULT hr = S_OK;

	// Fail if the source is shut down.
	hr = CheckShutdown();

	// Queue the operation.
	if (SUCCEEDED(hr))
	{
		//hr = QueueAsyncOperation(SourceOp::OP_PAUSE);
	}

	return hr;
}

//-------------------------------------------------------------------
// Shutdown
// Shuts down the source and releases all resources.
//-------------------------------------------------------------------

HRESULT CHtspSource::Shutdown()
{
	AutoLock lock(m_critSec);

	HRESULT hr = S_OK;

//	CMPEG1Stream *pStream = nullptr;

	hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{
/*		// Shut down the stream objects.
		for (DWORD i = 0; i < m_streams.GetCount(); i++)
		{
			(void)m_streams[i]->Shutdown();
		}
		// Break circular references with streams here.
		m_streams.Clear();*/

		// Shut down the event queue.
		if (m_spEventQueue)
		{
			(void)m_spEventQueue->Shutdown();
		}

		// Release objects.

		m_spEventQueue.Reset();
		m_spPresentationDescriptor.Reset();
/*		m_spByteStream.Reset();
		m_spCurrentOp.Reset();

		m_header = nullptr;

		m_parser = nullptr;*/

		// Set the state.
		m_state = STATE_SHUTDOWN;
	}

	return hr;
}


//-------------------------------------------------------------------
// Start
// Starts or seeks the media source.
//-------------------------------------------------------------------

HRESULT CHtspSource::Start(
	IMFPresentationDescriptor *pPresentationDescriptor,
	const GUID *pguidTimeFormat,
	const PROPVARIANT *pvarStartPos
	)
{

	HRESULT hr = S_OK;
	//ComPtr<SourceOp> spAsyncOp;

	// Check parameters.

	// Start position and presentation descriptor cannot be nullptr.
	if (pvarStartPos == nullptr || pPresentationDescriptor == nullptr)
	{
		return E_INVALIDARG;
	}

	// Check the time format.
	if ((pguidTimeFormat != nullptr) && (*pguidTimeFormat != GUID_NULL))
	{
		// Unrecognized time format GUID.
		return MF_E_UNSUPPORTED_TIME_FORMAT;
	}

	// Check the data type of the start position.
	if ((pvarStartPos->vt != VT_I8) && (pvarStartPos->vt != VT_EMPTY))
	{
		return MF_E_UNSUPPORTED_TIME_FORMAT;
	}

	AutoLock lock(m_critSec);

	// Check if this is a seek request. This sample does not support seeking.

	if (pvarStartPos->vt == VT_I8)
	{
		// If the current state is STOPPED, then position 0 is valid.
		// Otherwise, the start position must be VT_EMPTY (current position).

		if ((m_state != STATE_STOPPED) || (pvarStartPos->hVal.QuadPart != 0))
		{
			hr = MF_E_INVALIDREQUEST;
			goto done;
		}
	}

	// Fail if the source is shut down.
	hr = CheckShutdown();
	if (FAILED(hr))
	{
		goto done;
	}

	// Fail if the source was not initialized yet.
	hr = IsInitialized();
	if (FAILED(hr))
	{
		goto done;
	}

	// Perform a sanity check on the caller's presentation descriptor.
	/*hr = ValidatePresentationDescriptor(pPresentationDescriptor);
	if (FAILED(hr))
	{
		goto done;
	}*/

	// The operation looks OK. Complete the operation asynchronously.

	/*hr = SourceOp::CreateStartOp(pPresentationDescriptor, &spAsyncOp);
	if (FAILED(hr))
	{
		goto done;
	}

	hr = spAsyncOp->SetData(*pvarStartPos);
	if (FAILED(hr))
	{
		goto done;
	}

	hr = QueueOperation(spAsyncOp.Get());*/

done:

	return hr;
}


//-------------------------------------------------------------------
// Stop
// Stops the media source.
//-------------------------------------------------------------------

HRESULT CHtspSource::Stop()
{
	AutoLock lock(m_critSec);

	HRESULT hr = S_OK;

	// Fail if the source is shut down.
	hr = CheckShutdown();

	// Fail if the source was not initialized yet.
	if (SUCCEEDED(hr))
	{
		hr = IsInitialized();
	}

	// Queue the operation.
	if (SUCCEEDED(hr))
	{
		//hr = QueueAsyncOperation(SourceOp::OP_STOP);
	}

	return hr;
}


//-------------------------------------------------------------------
// IMFMediaSource methods
//-------------------------------------------------------------------

//-------------------------------------------------------------------
// GetService
// Returns a service
//-------------------------------------------------------------------

HRESULT CHtspSource::GetService(_In_ REFGUID guidService, _In_ REFIID riid, _Out_opt_ LPVOID *ppvObject)
{
	HRESULT hr = MF_E_UNSUPPORTED_SERVICE;

	if (ppvObject == nullptr)
	{
		return E_POINTER;
	}

/*	if (guidService == MF_RATE_CONTROL_SERVICE)
	{
		hr = QueryInterface(riid, ppvObject);
	}*/

	return hr;
}


/* Private methods */

CHtspSource::CHtspSource() :
//OpQueue(m_critSec.m_criticalSection),
m_cRef(1),
m_state(STATE_INVALID)
//m_cRestartCounter(0),
//m_OnByteStreamRead(this, &CMPEG1Source::OnByteStreamRead),
//m_flRate(1.0f)
{
	auto module = ::Microsoft::WRL::GetModuleBase();
	if (module != nullptr)
	{
		module->IncrementObjectCount();
	}
}

CHtspSource::~CHtspSource()
{
	if (m_state != STATE_SHUTDOWN)
	{
		Shutdown();
	}

	auto module = ::Microsoft::WRL::GetModuleBase();
	if (module != nullptr)
	{
		module->DecrementObjectCount();
	}
}




HRESULT CHtspSource::IsInitialized() const
{
	if (m_state == STATE_OPENING || m_state == STATE_INVALID)
	{
		return MF_E_NOT_INITIALIZED;
	}
	else
	{
		return S_OK;
	}
}