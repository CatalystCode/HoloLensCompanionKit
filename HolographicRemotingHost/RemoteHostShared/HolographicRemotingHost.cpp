// RemotingHostLib.cpp : Defines the exported functions for the DLL application.
//

#include "pch.h"
#include "HolographicRemotingHost.h"
#include "Common\DbgLog.h"
#include <Collection.h>
#include <ppltasks.h>

using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Microsoft::Holographic;
using namespace Windows::Graphics::Holographic;
using namespace Windows::Perception::Spatial;

// This is the constructor of a class that has been exported.
// see RemotingHostLib.h for the class definition
HolographicRemotingHost::HolographicRemotingHost()
{
    return;
}

HolographicRemotingHost::~HolographicRemotingHost() {
	// Deregister device notification.
	m_deviceResources->RegisterDeviceNotify(nullptr);
	if (m_speechToken.Value != 0)
	{
		m_remoteSpeech->OnRecognizedSpeech -= m_speechToken;
	}
	UnregisterHolographicEventHandlers();
}

void HolographicRemotingHost::Initialize(ID3D11Device4 * spD3DDevice, const int viewportHeight, const int viewportWidth, ID3D11Texture2D &texture)
{
	m_viewportWidth = viewportWidth;
	m_viewportHeight = viewportHeight;

	assert(spD3DDevice);

	m_d3dDevice = spD3DDevice;

	//TODO: check device setup for compat
	//m_spDevice->

	m_streamerHelpers = ref new HolographicStreamerHelpers();
	m_holographicSpace = m_streamerHelpers->HolographicSpace;
	m_deviceResources->SetHolographicSpace(m_holographicSpace);

	m_remoteSpeech = m_streamerHelpers->RemoteSpeech;
}

void HolographicRemotingHost::ConnectToRemoteDevice(Platform::String ^ ipAddress, int port)
{
	// Connecting to the remote device can change the connection state.
	auto exclusiveLock = m_connectionStateLock.LockExclusive();

	if (m_streamerHelpers)
	{
		m_streamerHelpers->CreateStreamer(m_d3dDevice.Get());

		m_streamerHelpers->OnConnected += ref new ConnectedEvent(
			[this]()
		{
			// TODO: add a callback that the client app can hook to receive the connected event
		});

		Platform::WeakReference streamerHelpersWeakRef = Platform::WeakReference(m_streamerHelpers);
		m_streamerHelpers->OnDisconnected += ref new DisconnectedEvent(
			[this, streamerHelpersWeakRef](_In_ HolographicStreamerConnectionFailureReason failureReason)
		{
			DebugLog(L"Disconnected with reason %d", failureReason);
			// TODO: add a callback that the client app can hook to receive the disconnected event

			// Reconnect if this is a transient failure.
			if (failureReason == HolographicStreamerConnectionFailureReason::Unreachable ||
				failureReason == HolographicStreamerConnectionFailureReason::ConnectionLost)
			{
				DebugLog(L"Reconnecting...");

				try
				{
					auto helpersResolved = streamerHelpersWeakRef.Resolve<HolographicStreamerHelpers>();
					if (helpersResolved)
					{
						helpersResolved->Connect(m_ipAddress->Data(), 8001);
					}
					else
					{
						DebugLog(L"Failed to reconnect because a disconnect has already occurred.\n");
					}
				}
				catch (Platform::Exception^ ex)
				{
					DebugLog(L"Connect failed with hr = 0x%08X", ex->HResult);
				}
			}
			else
			{
				DebugLog(L"Disconnected with unrecoverable error, not attempting to reconnect.");
			}
		});

		m_streamerHelpers->OnSendFrame += ref new SendFrameEvent(
			[this](_In_ const ComPtr<ID3D11Texture2D>& spTexture, _In_ FrameMetadata metadata)
		{
			// NOTE: not sure what to do with this, if anything
		});

		// We currently need to stream at 720p because that's the resolution of our remote display.
		// There is a check in the holographic streamer that makes sure the remote and local 
		// resolutions match. The default streamer resolution is 1080p.
		m_streamerHelpers->SetVideoFrameSize(m_viewportWidth, m_viewportHeight);

		try
		{
			m_streamerHelpers->Connect(m_ipAddress->Data(), 8001);
		}
		catch (Platform::Exception^ ex)
		{
			DebugLog(L"Connect failed with hr = 0x%08X", ex->HResult);
		}
	}
}

Windows::Graphics::Holographic::HolographicFrame^ HolographicRemotingHost::Update()
{
	// Before doing the timer update, there is some work to do per-frame
	// to maintain holographic rendering. First, we will get information
	// about the current frame.

	// The HolographicFrame has information that the app needs in order
	// to update and render the current frame. The app begins each new
	// frame by calling CreateNextFrame.
	HolographicFrame^ holographicFrame = m_streamerHelpers->HolographicSpace->CreateNextFrame();

	// Get a prediction of where holographic cameras will be when this frame
	// is presented.
	HolographicFramePrediction^ prediction = holographicFrame->CurrentPrediction;

	// Back buffers can change from frame to frame. Validate each buffer, and recreate
	// resource views and depth buffers as needed.
	m_deviceResources->EnsureCameraResources(holographicFrame, prediction);

	// Next, we get a coordinate system from the attached frame of reference that is
	// associated with the current frame. Later, this coordinate system is used for
	// for creating the stereo view matrices when rendering the sample content.
	SpatialCoordinateSystem^ currentCoordinateSystem = m_referenceFrame->CoordinateSystem;

	return holographicFrame;
}

bool HolographicRemotingHost::Render(Windows::Graphics::Holographic::HolographicFrame^ holographicFrame)
{
	return false;
}

void HolographicRemotingHost::OnLocatabilityChanged(SpatialLocator^ sender, Object^ args)
{
	switch (sender->Locatability)
	{
	case SpatialLocatability::Unavailable:
		// Holograms cannot be rendered.
	{
		String^ message = L"Warning! Positional tracking is " +
			sender->Locatability.ToString() + L".\n";
		OutputDebugStringW(message->Data());
	}
	break;

	// In the following three cases, it is still possible to place holograms using a
	// SpatialLocatorAttachedFrameOfReference.
	case SpatialLocatability::PositionalTrackingActivating:
		// The system is preparing to use positional tracking.

	case SpatialLocatability::OrientationOnly:
		// Positional tracking has not been activated.

	case SpatialLocatability::PositionalTrackingInhibited:
		// Positional tracking is temporarily inhibited. User action may be required
		// in order to restore positional tracking.
		break;

	case SpatialLocatability::PositionalTrackingActive:
		// Positional tracking is active. World-locked content can be rendered.
		break;
	}
}

void HolographicRemotingHost::OnCameraAdded(
	HolographicSpace^ sender,
	HolographicSpaceCameraAddedEventArgs^ args
)
{
	Deferral^ deferral = args->GetDeferral();
	HolographicCamera^ holographicCamera = args->Camera;
	create_task([this, deferral, holographicCamera]()
	{
		// Create device-based resources for the holographic camera and add it to the list of
		// cameras used for updates and rendering. Notes:
		//   * Since this function may be called at any time, the AddHolographicCamera function
		//     waits until it can get a lock on the set of holographic camera resources before
		//     adding the new camera. At 60 frames per second this wait should not take long.
		//   * A subsequent Update will take the back buffer from the RenderingParameters of this
		//     camera's CameraPose and use it to create the ID3D11RenderTargetView for this camera.
		//     Content can then be rendered for the HolographicCamera.
		m_deviceResources->AddHolographicCamera(holographicCamera);

		// Holographic frame predictions will not include any information about this camera until
		// the deferral is completed.
		deferral->Complete();
	});
}

void HolographicRemotingHost::OnCameraRemoved(
	HolographicSpace^ sender,
	HolographicSpaceCameraRemovedEventArgs^ args
)
{
	// Before letting this callback return, ensure that all references to the back buffer 
	// are released.
	// Since this function may be called at any time, the RemoveHolographicCamera function
	// waits until it can get a lock on the set of holographic camera resources before
	// deallocating resources for this camera. At 60 frames per second this wait should
	// not take long.
	m_deviceResources->RemoveHolographicCamera(args->Camera);
}

void HolographicRemotingHost::OnRecognizedSpeech(Object^ sender, Microsoft::Holographic::RecognizedSpeech^ recognizedSpeech)
{

}

void HolographicRemotingHost::UnregisterHolographicEventHandlers()
{
	if (m_holographicSpace != nullptr)
	{
		// Clear previous event registrations.

		if (m_cameraAddedToken.Value != 0)
		{
			m_holographicSpace->CameraAdded -= m_cameraAddedToken;
			m_cameraAddedToken.Value = 0;
		}

		if (m_cameraRemovedToken.Value != 0)
		{
			m_holographicSpace->CameraRemoved -= m_cameraRemovedToken;
			m_cameraRemovedToken.Value = 0;
		}
	}

	if (m_locator != nullptr)
	{
		m_locator->LocatabilityChanged -= m_locatabilityChangedToken;
	}
}

bool HolographicRemotingHost::CheckDeviceCompat(ID3D11Device4 * d3ddevice)
{
	bool result = false;

	// check the hishest feature level
	result = d3ddevice->GetFeatureLevel() == FEATURE_LEVEL;

	return result;
}
