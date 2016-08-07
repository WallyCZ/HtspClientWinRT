#pragma once

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfapi.h>
#include <mferror.h>

#include "critsec.h"

#include <wrl\client.h>
#include <wrl\implements.h>
#include <wrl\module.h>

#include <windows.media.h>




using namespace Platform;
using namespace Microsoft::WRL;

enum SourceState
{
	STATE_INVALID,      // Initial state. Have not started opening the stream.
	STATE_OPENING,      // BeginOpen is in progress.
	STATE_STOPPED,
	STATE_PAUSED,
	STATE_STARTED,
	STATE_SHUTDOWN
};







class CHtspSource WrlSealed :
	//public OpQueue<CHtspSource, SourceOp>,
	public IMFMediaSource,
	public IMFGetService
//	public IMFRateControl
{
	//InspectableClass(L"HTSPSource.MPEG1ByteStreamHandler", BaseTrust)

public:
	static ComPtr<CHtspSource> CreateInstance();

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void **ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	// IMFMediaEventGenerator
	STDMETHODIMP BeginGetEvent(IMFAsyncCallback *pCallback, IUnknown *punkState);
	STDMETHODIMP EndGetEvent(IMFAsyncResult *pResult, IMFMediaEvent **ppEvent);
	STDMETHODIMP GetEvent(DWORD dwFlags, IMFMediaEvent **ppEvent);
	STDMETHODIMP QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT *pvValue);

	// IMFMediaSource
	STDMETHODIMP CreatePresentationDescriptor(IMFPresentationDescriptor **ppPresentationDescriptor);
	STDMETHODIMP GetCharacteristics(DWORD *pdwCharacteristics);
	STDMETHODIMP Pause();
	STDMETHODIMP Shutdown();
	STDMETHODIMP Start(
		IMFPresentationDescriptor *pPresentationDescriptor,
		const GUID *pguidTimeFormat,
		const PROPVARIANT *pvarStartPosition
		);
	STDMETHODIMP Stop();

	// IMFGetService
	IFACEMETHOD(GetService) (_In_ REFGUID guidService, _In_ REFIID riid, _Out_opt_ LPVOID *ppvObject);

/*	// IMFRateControl
	IFACEMETHOD(SetRate) (BOOL fThin, float flRate);
	IFACEMETHOD(GetRate) (_Inout_opt_ BOOL *pfThin, _Inout_opt_ float *pflRate);

	// Called by the byte stream handler.
	concurrency::task<void> OpenAsync(IMFByteStream *pStream);

	// Queues an asynchronous operation, specify by op-type.
	// (This method is public because the streams call it.)
	HRESULT QueueAsyncOperation(SourceOp::Operation OpType);

	// Lock/Unlock:
	// Holds and releases the source's critical section. Called by the streams.
	_Acquires_lock_(m_critSec)
		void    Lock() { m_critSec.Lock(); }

	_Releases_lock_(m_critSec)
		void    Unlock() { m_critSec.Unlock(); }

	// Callbacks
	HRESULT OnByteStreamRead(IMFAsyncResult *pResult);  // Async callback for RequestData*/



private:
	CHtspSource();
	~CHtspSource();


	// CheckShutdown: Returns MF_E_SHUTDOWN if the source was shut down.
	HRESULT CheckShutdown() const
	{
		return (m_state == STATE_SHUTDOWN ? MF_E_SHUTDOWN : S_OK);
	}

	HRESULT     IsInitialized() const;



private:

	long                        m_cRef;                     // reference count

	CritSec                     m_critSec;                  // critical section for thread safety
	SourceState                 m_state;                    // Current state (running, stopped, paused)

	ComPtr<IMFMediaEventQueue>  m_spEventQueue;             // Event generator helper
	ComPtr<IMFPresentationDescriptor> m_spPresentationDescriptor; // Presentation descriptor.


};