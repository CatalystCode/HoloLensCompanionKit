#pragma once
#include "pch.h"
#include "Common\DeviceResources.h"
#include <HolographicStreamerHelpers.h>

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the REMOTINGHOSTLIB_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// REMOTINGHOSTLIB_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef REMOTINGHOSTLIB_EXPORTS
#define REMOTINGHOST_API __declspec(dllexport)
#else
#define REMOTINGHOST_API __declspec(dllimport)
#endif

// the acceptable feature level for the device
#define FEATURE_LEVEL D3D_FEATURE_LEVEL_11_1;

using namespace DX;
using namespace Microsoft::WRL;

// This class is exported from the RemotingHost.dll
class REMOTINGHOST_API HolographicRemotingHost {
public:
	HolographicRemotingHost(void);
	~HolographicRemotingHost();
	// TODO: add your methods here.

	// pass in the D3DDevice for resource initialization - should come from the rendering engine.
	void Initialize(ID3D11Device4 * d3ddevice, const int viewportHeight, const int viewportWidth, ID3D11Texture2D &texture);
	void ConnectToRemoteDevice(Platform::String^ ipAddress, int port);
	Windows::Graphics::Holographic::HolographicFrame^ Update();
	bool Render(Windows::Graphics::Holographic::HolographicFrame^ holographicFrame);

private:
	// Asynchronously creates resources for new holographic cameras.
	void OnCameraAdded(
		Windows::Graphics::Holographic::HolographicSpace^ sender,
		Windows::Graphics::Holographic::HolographicSpaceCameraAddedEventArgs^ args);

	// Synchronously releases resources for holographic cameras that are no longer
	// attached to the system.
	void OnCameraRemoved(
		Windows::Graphics::Holographic::HolographicSpace^ sender,
		Windows::Graphics::Holographic::HolographicSpaceCameraRemovedEventArgs^ args);

	// Used to notify the app when the positional tracking state changes.
	void OnLocatabilityChanged(
		Windows::Perception::Spatial::SpatialLocator^ sender,
		Platform::Object^ args);

	// Clears event registration state. Used when changing to a new HolographicSpace
	// and when tearing down AppMain.
	void UnregisterHolographicEventHandlers();

	// Responds to speech recognition results.
	void OnRecognizedSpeech(
		Platform::Object^ sender,
		Microsoft::Holographic::RecognizedSpeech ^recognizedSpeech);

	ComPtr<ID3D11Device4> m_d3dDevice = nullptr;
	
	// Cached pointer to device resources.
	std::shared_ptr<DX::DeviceResources>                m_deviceResources;

	//// The holographic space the app will use for rendering.
	//Windows::Graphics::Holographic::HolographicSpace^   m_holographicSpace = nullptr;
	//// Handles speech recognition from the remote endpoint.
	//Microsoft::Holographic::RemoteSpeech^               m_remoteSpeech;

	Microsoft::Holographic::HolographicStreamerHelpers^ m_streamerHelpers;

	Platform::String^                                   m_ipAddress;
	Microsoft::WRL::Wrappers::SRWLock                   m_connectionStateLock;
	Microsoft::WRL::Wrappers::CriticalSection           m_deviceLock;
	Microsoft::WRL::ComPtr<ID3D11Texture2D>             m_spTexture;

	// Handles speech recognition from the remote endpoint.
	Microsoft::Holographic::RemoteSpeech^                               m_remoteSpeech;

	// Represents the holographic space around the user.
	Windows::Graphics::Holographic::HolographicSpace^                   m_holographicSpace;

	// SpatialLocator that is attached to the primary camera.
	Windows::Perception::Spatial::SpatialLocator^                       m_locator;

	// A reference frame that is positioned in the world.
	Windows::Perception::Spatial::SpatialStationaryFrameOfReference^    m_referenceFrame;

	// Event registration tokens.
	Windows::Foundation::EventRegistrationToken                         m_cameraAddedToken;
	Windows::Foundation::EventRegistrationToken                         m_cameraRemovedToken;
	Windows::Foundation::EventRegistrationToken                         m_locatabilityChangedToken;
	Windows::Foundation::EventRegistrationToken                         m_speechToken = {};

	int m_viewportHeight = 720;
	int m_viewportWidth = 1280;

	// Checks the passed D3DDevice for compatibility with the HolographicRemoting framework.
	bool CheckDeviceCompat(ID3D11Device4 * d3ddevice);
};
