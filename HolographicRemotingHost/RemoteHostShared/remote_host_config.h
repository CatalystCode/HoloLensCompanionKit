#pragma once

struct REMOTE_HOST_CONFIG
{
public:
	bool EnableAudio = true;
	bool EnableVideo = true;
	bool HandleSpatialInput = true;

	int MaxBitRate = 5 * 1024;
	int Height = 720;
	int Width = 1280;
};