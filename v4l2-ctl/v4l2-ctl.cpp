#include <vector>
#include <string>
#include <iostream>
#include <dshow.h>

#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "quartz.lib")

int GetUSBCameraDevicesList(std::vector<std::string>& list, std::vector<std::string>& devicePaths)
{
	//COM Library Initialization
	//comInit();

	ICreateDevEnum* pDevEnum = NULL;
	IEnumMoniker* pEnum = NULL;
	int deviceCounter = 0;
	CoInitialize(NULL);

	HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
		CLSCTX_INPROC_SERVER, IID_ICreateDevEnum,
		reinterpret_cast<void**>(&pDevEnum));

	if (SUCCEEDED(hr))
	{
		// Create an enumerator for the video capture category.
		hr = pDevEnum->CreateClassEnumerator(
			CLSID_VideoInputDeviceCategory,
			&pEnum, 0);

		if (hr == S_OK) {

			//printf("SETUP: Looking For Capture Devices\n");
			IMoniker* pMoniker = NULL;

			while (pEnum->Next(1, &pMoniker, NULL) == S_OK) {

				IPropertyBag* pPropBag;
				hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)(&pPropBag));

				if (FAILED(hr)) {
					pMoniker->Release();
					continue;  // Skip this one, maybe the next one will work.
				}

				// Find the description or friendly name.
				VARIANT varName;
				VariantInit(&varName);
				hr = pPropBag->Read(L"Description", &varName, 0);

				if (FAILED(hr)) hr = pPropBag->Read(L"FriendlyName", &varName, 0);

				if (SUCCEEDED(hr))
				{

					hr = pPropBag->Read(L"FriendlyName", &varName, 0);

					int count = 0;
					char tmp[255] = { 0 };
					//int maxLen = sizeof(deviceNames[0]) / sizeof(deviceNames[0][0]) - 2;
					while (varName.bstrVal[count] != 0x00 && count < 255)
					{
						tmp[count] = (char)varName.bstrVal[count];
						count++;
					}

					// then read Device Path
					{
						VARIANT DP_Path;
						VariantInit(&DP_Path);

						hr = pPropBag->Read(L"DevicePath", &DP_Path, 0);

						if (SUCCEEDED(hr))
						{
							int __count = 0;
							char __tmp[255] = { 0 };
							while (DP_Path.bstrVal[__count] != 0x00 && __count < 255)
							{
								__tmp[__count] = (char)DP_Path.bstrVal[__count];
								__count++;
							}

							list.emplace_back(tmp);
							devicePaths.emplace_back(__tmp);
							deviceCounter++;
						}
					}
				}

				pPropBag->Release();
				pPropBag = NULL;

				pMoniker->Release();
				pMoniker = NULL;

			}

			pDevEnum->Release();
			pDevEnum = NULL;

			pEnum->Release();
			pEnum = NULL;
		}

	}

	//comUnInit();

	return deviceCounter;
}

void list_devices()
{
	std::vector<std::string> list;
	std::vector<std::string> devicePaths;
	int count = GetUSBCameraDevicesList(list, devicePaths);
	for (int i = 0; i < count; i++) {
		std::cout << list[i] << std::endl;
	}
}

// zoom 100-800
// tilt -180-180
// pan -180-180

int zoom_tilt_pan (
	int camera_number, 
	bool getzoom, bool gettilt, bool getpan, long *_zoom, long *_tilt, long *_pan,
	bool setzoom, bool settilt, bool setpan, long zoom, long tilt, long pan
)
{
	// Initialize COM
	CoInitialize(NULL);

	// Create a filter graph manager
	IGraphBuilder* graphBuilder = NULL;
	CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
		IID_IGraphBuilder, (void**)&graphBuilder);

	// Create a capture graph builder
	ICaptureGraphBuilder2* captureGraphBuilder = NULL;
	CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL,
		CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2,
		(void**)&captureGraphBuilder);

	// Set the filter graph manager for the capture graph builder
	captureGraphBuilder->SetFiltergraph(graphBuilder);

	// Get the system device enumerator
	ICreateDevEnum* devEnum = NULL;
	CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
		CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&devEnum);

	// Create a video capture category enumerator
	IEnumMoniker* enumMoniker = NULL;
	devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
		&enumMoniker, 0);

	// Get the video capture device (camera)
	IMoniker* moniker = NULL;
	enumMoniker->Next(1, &moniker, NULL);
	while (camera_number-- > 0) {
		enumMoniker->Next(1, &moniker, NULL);
	}

	// Bind the selected video capture device (camera) to a filter object
	IBaseFilter* captureFilter = NULL;
	moniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void**)&captureFilter);

	// Add the capture filter to the filter graph manager
	graphBuilder->AddFilter(captureFilter, L"Capture Filter");

	// Connect the capture filter to the video renderer filter
	captureGraphBuilder->RenderStream(&PIN_CATEGORY_PREVIEW,
		&MEDIATYPE_Video, captureFilter, NULL, NULL);

	// Get the video renderer filter
	IBaseFilter* videoRenderer = NULL;
	captureGraphBuilder->FindInterface(&PIN_CATEGORY_PREVIEW,
		&MEDIATYPE_Video, captureFilter, IID_IBaseFilter,
		(void**)&videoRenderer);

	// Get the IAMCameraControl interface for camera control
	IAMCameraControl* cameraControl = NULL;
	captureFilter->QueryInterface(IID_IAMCameraControl, (void**)&cameraControl);

	// Set the camera properties
	if (cameraControl)
	{
		long flags;
		if (getzoom) cameraControl->Get(CameraControl_Zoom, _zoom, &flags);
		if (gettilt) cameraControl->Get(CameraControl_Tilt, _tilt, &flags);
		if (getpan) cameraControl->Get(CameraControl_Pan, _pan, &flags);
		if (setzoom) cameraControl->Set(CameraControl_Zoom, zoom, CameraControl_Flags_Manual);
		if (settilt) cameraControl->Set(CameraControl_Tilt, tilt, CameraControl_Flags_Manual);
		if (setpan) cameraControl->Set(CameraControl_Pan, pan, CameraControl_Flags_Manual);
		// Release the camera control interface
		cameraControl->Release();
	}

	videoRenderer->Release();
	captureFilter->Release();
	moniker->Release();
	enumMoniker->Release();
	devEnum->Release();
	captureGraphBuilder->Release();
	graphBuilder->Release();

	// Uninitialize COM
	CoUninitialize();
	return 0;
}


#include "getopt.h"

enum Option {
	OptGetSlicedVbiFormat = 'B',
	OptSetSlicedVbiFormat = 'b',
	OptGetCtrl = 'C',
	OptSetCtrl = 'c',
	OptSetDevice = 'd',
	OptGetDriverInfo = 'D',
	OptSetOutDevice = 'e',
	OptGetFreq = 'F',
	OptSetFreq = 'f',
	OptHelp = 'h',
	OptGetInput = 'I',
	OptSetInput = 'i',
	OptConcise = 'k',
	OptListCtrls = 'l',
	OptListCtrlsMenus = 'L',
	OptListOutputs = 'N',
	OptListInputs = 'n',
	OptGetOutput = 'O',
	OptSetOutput = 'o',
	OptGetParm = 'P',
	OptSetParm = 'p',
	OptSubset = 'r',
	OptGetStandard = 'S',
	OptSetStandard = 's',
	OptGetTuner = 'T',
	OptSetTuner = 't',
	OptGetVideoFormat = 'V',
	OptSetVideoFormat = 'v',
	OptUseWrapper = 'w',

	OptGetSlicedVbiOutFormat = 128,
	OptGetOverlayFormat,
	OptGetVbiFormat,
	OptGetVbiOutFormat,
	OptGetSdrFormat,
	OptGetSdrOutFormat,
	OptGetVideoOutFormat,
	OptGetMetaFormat,
	OptSetSlicedVbiOutFormat,
	OptSetOverlayFormat,
	OptSetVbiFormat,
	OptSetVbiOutFormat,
	OptSetSdrFormat,
	OptSetSdrOutFormat,
	OptSetVideoOutFormat,
	OptSetMetaFormat,
	OptTryVideoOutFormat,
	OptTrySlicedVbiOutFormat,
	OptTrySlicedVbiFormat,
	OptTryVideoFormat,
	OptTryOverlayFormat,
	OptTryVbiFormat,
	OptTryVbiOutFormat,
	OptTrySdrFormat,
	OptTrySdrOutFormat,
	OptTryMetaFormat,
	OptAll,
	OptListStandards,
	OptListFormats,
	OptListFormatsExt,
	OptListFields,
	OptListFrameSizes,
	OptListFrameIntervals,
	OptListOverlayFormats,
	OptListSdrFormats,
	OptListSdrOutFormats,
	OptListOutFormats,
	OptListMetaFormats,
	OptListOutFields,
	OptClearClips,
	OptClearBitmap,
	OptAddClip,
	OptAddBitmap,
	OptFindFb,
	OptLogStatus,
	OptVerbose,
	OptSilent,
	OptGetSlicedVbiCap,
	OptGetSlicedVbiOutCap,
	OptGetFBuf,
	OptSetFBuf,
	OptGetCrop,
	OptSetCrop,
	OptGetOutputCrop,
	OptSetOutputCrop,
	OptGetOverlayCrop,
	OptSetOverlayCrop,
	OptGetOutputOverlayCrop,
	OptSetOutputOverlayCrop,
	OptGetSelection,
	OptSetSelection,
	OptGetOutputSelection,
	OptSetOutputSelection,
	OptGetAudioInput,
	OptSetAudioInput,
	OptGetAudioOutput,
	OptSetAudioOutput,
	OptListAudioOutputs,
	OptListAudioInputs,
	OptGetCropCap,
	OptGetOutputCropCap,
	OptGetOverlayCropCap,
	OptGetOutputOverlayCropCap,
	OptOverlay,
	OptSleep,
	OptGetJpegComp,
	OptSetJpegComp,
	OptGetModulator,
	OptSetModulator,
	OptListFreqBands,
	OptListDevices,
	OptGetOutputParm,
	OptSetOutputParm,
	OptQueryStandard,
	OptPollForEvent,
	OptWaitForEvent,
	OptGetPriority,
	OptSetPriority,
	OptListDvTimings,
	OptQueryDvTimings,
	OptGetDvTimings,
	OptSetDvBtTimings,
	OptGetDvTimingsCap,
	OptSetEdid,
	OptClearEdid,
	OptGetEdid,
	OptInfoEdid,
	OptFixEdidChecksums,
	OptFreqSeek,
	OptEncoderCmd,
	OptTryEncoderCmd,
	OptDecoderCmd,
	OptTryDecoderCmd,
	OptTunerIndex,
	OptListBuffers,
	OptListBuffersOut,
	OptListBuffersVbi,
	OptListBuffersSlicedVbi,
	OptListBuffersVbiOut,
	OptListBuffersSlicedVbiOut,
	OptListBuffersSdr,
	OptListBuffersSdrOut,
	OptListBuffersMeta,
	OptStreamCount,
	OptStreamSkip,
	OptStreamLoop,
	OptStreamSleep,
	OptStreamPoll,
	OptStreamNoQuery,
	OptStreamTo,
	OptStreamToHost,
	OptStreamMmap,
	OptStreamUser,
	OptStreamDmaBuf,
	OptStreamFrom,
	OptStreamFromHost,
	OptStreamOutPattern,
	OptStreamOutSquare,
	OptStreamOutBorder,
	OptStreamOutInsertSAV,
	OptStreamOutInsertEAV,
	OptStreamOutHorSpeed,
	OptStreamOutVertSpeed,
	OptStreamOutPercFill,
	OptStreamOutAlphaComponent,
	OptStreamOutAlphaRedOnly,
	OptStreamOutRGBLimitedRange,
	OptStreamOutPixelAspect,
	OptStreamOutVideoAspect,
	OptStreamOutMmap,
	OptStreamOutUser,
	OptStreamOutDmaBuf,
	OptListPatterns,
	OptHelpTuner,
	OptHelpIO,
	OptHelpStds,
	OptHelpVidCap,
	OptHelpVidOut,
	OptHelpOverlay,
	OptHelpVbi,
	OptHelpSdr,
	OptHelpMeta,
	OptHelpSelection,
	OptHelpMisc,
	OptHelpStreaming,
	OptHelpEdid,
	OptHelpAll,
	OptLast = 512
};

extern char options[OptLast];

static struct option long_options[] = {
//	{"list-audio-inputs", no_argument, 0, OptListAudioInputs},
//	{"list-audio-outputs", no_argument, 0, OptListAudioOutputs},
//	{"all", no_argument, 0, OptAll},
	{"device", required_argument, 0, OptSetDevice},
//	{"out-device", required_argument, 0, OptSetOutDevice},
//	{"get-fmt-video", no_argument, 0, OptGetVideoFormat},
//	{"set-fmt-video", required_argument, 0, OptSetVideoFormat},
//	{"try-fmt-video", required_argument, 0, OptTryVideoFormat},
//	{"get-fmt-video-out", no_argument, 0, OptGetVideoOutFormat},
//	{"set-fmt-video-out", required_argument, 0, OptSetVideoOutFormat},
//	{"try-fmt-video-out", required_argument, 0, OptTryVideoOutFormat},
//	{"help", no_argument, 0, OptHelp},
//	{"help-tuner", no_argument, 0, OptHelpTuner},
//	{"help-io", no_argument, 0, OptHelpIO},
//	{"help-stds", no_argument, 0, OptHelpStds},
//	{"help-vidcap", no_argument, 0, OptHelpVidCap},
//	{"help-vidout", no_argument, 0, OptHelpVidOut},
//	{"help-overlay", no_argument, 0, OptHelpOverlay},
//	{"help-vbi", no_argument, 0, OptHelpVbi},
//	{"help-sdr", no_argument, 0, OptHelpSdr},
//	{"help-meta", no_argument, 0, OptHelpMeta},
//	{"help-selection", no_argument, 0, OptHelpSelection},
//	{"help-misc", no_argument, 0, OptHelpMisc},
//	{"help-streaming", no_argument, 0, OptHelpStreaming},
//	{"help-edid", no_argument, 0, OptHelpEdid},
//	{"help-all", no_argument, 0, OptHelpAll},
#ifndef NO_LIBV4L2
//	{"wrapper", no_argument, 0, OptUseWrapper},
#endif
//	{"concise", no_argument, 0, OptConcise},
//	{"get-output", no_argument, 0, OptGetOutput},
//	{"set-output", required_argument, 0, OptSetOutput},
//	{"list-outputs", no_argument, 0, OptListOutputs},
//	{"list-inputs", no_argument, 0, OptListInputs},
//	{"get-input", no_argument, 0, OptGetInput},
//	{"set-input", required_argument, 0, OptSetInput},
//	{"get-audio-input", no_argument, 0, OptGetAudioInput},
//	{"set-audio-input", required_argument, 0, OptSetAudioInput},
//	{"get-audio-output", no_argument, 0, OptGetAudioOutput},
//	{"set-audio-output", required_argument, 0, OptSetAudioOutput},
//	{"get-freq", no_argument, 0, OptGetFreq},
//	{"set-freq", required_argument, 0, OptSetFreq},
//	{"list-standards", no_argument, 0, OptListStandards},
//	{"list-formats", no_argument, 0, OptListFormats},
//	{"list-formats-ext", no_argument, 0, OptListFormatsExt},
//	{"list-fields", no_argument, 0, OptListFields},
//	{"list-framesizes", required_argument, 0, OptListFrameSizes},
//	{"list-frameintervals", required_argument, 0, OptListFrameIntervals},
//	{"list-formats-overlay", no_argument, 0, OptListOverlayFormats},
//	{"list-formats-sdr", no_argument, 0, OptListSdrFormats},
//	{"list-formats-sdr-out", no_argument, 0, OptListSdrOutFormats},
//	{"list-formats-out", no_argument, 0, OptListOutFormats},
//	{"list-formats-meta", no_argument, 0, OptListMetaFormats},
//	{"list-fields-out", no_argument, 0, OptListOutFields},
//	{"clear-clips", no_argument, 0, OptClearClips},
//	{"clear-bitmap", no_argument, 0, OptClearBitmap},
//	{"add-clip", required_argument, 0, OptAddClip},
//	{"add-bitmap", required_argument, 0, OptAddBitmap},
//	{"find-fb", no_argument, 0, OptFindFb},
//	{"subset", required_argument, 0, OptSubset},
//	{"get-standard", no_argument, 0, OptGetStandard},
//	{"set-standard", required_argument, 0, OptSetStandard},
//	{"get-detected-standard", no_argument, 0, OptQueryStandard},
//	{"get-parm", no_argument, 0, OptGetParm},
//	{"set-parm", required_argument, 0, OptSetParm},
//	{"get-output-parm", no_argument, 0, OptGetOutputParm},
//	{"set-output-parm", required_argument, 0, OptSetOutputParm},
//	{"info", no_argument, 0, OptGetDriverInfo},
//	{"list-ctrls", no_argument, 0, OptListCtrls},
//	{"list-ctrls-menus", no_argument, 0, OptListCtrlsMenus},
	{"set-ctrl", required_argument, 0, OptSetCtrl},
	{"get-ctrl", required_argument, 0, OptGetCtrl},
//	{"get-tuner", no_argument, 0, OptGetTuner},
//	{"set-tuner", required_argument, 0, OptSetTuner},
//	{"list-freq-bands", no_argument, 0, OptListFreqBands},
//	{"silent", no_argument, 0, OptSilent},
//	{"verbose", no_argument, 0, OptVerbose},
//	{"log-status", no_argument, 0, OptLogStatus},
//	{"get-fmt-overlay", no_argument, 0, OptGetOverlayFormat},
//	{"set-fmt-overlay", optional_argument, 0, OptSetOverlayFormat},
//	{"try-fmt-overlay", optional_argument, 0, OptTryOverlayFormat},
//	{"get-fmt-sliced-vbi", no_argument, 0, OptGetSlicedVbiFormat},
//	{"set-fmt-sliced-vbi", required_argument, 0, OptSetSlicedVbiFormat},
//	{"try-fmt-sliced-vbi", required_argument, 0, OptTrySlicedVbiFormat},
//	{"get-fmt-sliced-vbi-out", no_argument, 0, OptGetSlicedVbiOutFormat},
//	{"set-fmt-sliced-vbi-out", required_argument, 0, OptSetSlicedVbiOutFormat},
//	{"try-fmt-sliced-vbi-out", required_argument, 0, OptTrySlicedVbiOutFormat},
//	{"get-fmt-vbi", no_argument, 0, OptGetVbiFormat},
//	{"set-fmt-vbi", required_argument, 0, OptSetVbiFormat},
//	{"try-fmt-vbi", required_argument, 0, OptTryVbiFormat},
//	{"get-fmt-vbi-out", no_argument, 0, OptGetVbiOutFormat},
//	{"set-fmt-vbi-out", required_argument, 0, OptSetVbiOutFormat},
//	{"try-fmt-vbi-out", required_argument, 0, OptTryVbiOutFormat},
//	{"get-fmt-sdr", no_argument, 0, OptGetSdrFormat},
//	{"set-fmt-sdr", required_argument, 0, OptSetSdrFormat},
//	{"try-fmt-sdr", required_argument, 0, OptTrySdrFormat},
//	{"get-fmt-sdr-out", no_argument, 0, OptGetSdrOutFormat},
//	{"set-fmt-sdr-out", required_argument, 0, OptSetSdrOutFormat},
//	{"try-fmt-sdr-out", required_argument, 0, OptTrySdrOutFormat},
//	{"get-fmt-meta", no_argument, 0, OptGetMetaFormat},
//	{"set-fmt-meta", required_argument, 0, OptSetMetaFormat},
//	{"try-fmt-meta", required_argument, 0, OptTryMetaFormat},
//	{"get-sliced-vbi-cap", no_argument, 0, OptGetSlicedVbiCap},
//	{"get-sliced-vbi-out-cap", no_argument, 0, OptGetSlicedVbiOutCap},
//	{"get-fbuf", no_argument, 0, OptGetFBuf},
//	{"set-fbuf", required_argument, 0, OptSetFBuf},
//	{"get-cropcap", no_argument, 0, OptGetCropCap},
//	{"get-crop", no_argument, 0, OptGetCrop},
//	{"set-crop", required_argument, 0, OptSetCrop},
//	{"get-cropcap-output", no_argument, 0, OptGetOutputCropCap},
//	{"get-crop-output", no_argument, 0, OptGetOutputCrop},
//	{"set-crop-output", required_argument, 0, OptSetOutputCrop},
//	{"get-cropcap-overlay", no_argument, 0, OptGetOverlayCropCap},
//	{"get-crop-overlay", no_argument, 0, OptGetOverlayCrop},
//	{"set-crop-overlay", required_argument, 0, OptSetOverlayCrop},
//	{"get-cropcap-output-overlay", no_argument, 0, OptGetOutputOverlayCropCap},
//	{"get-crop-output-overlay", no_argument, 0, OptGetOutputOverlayCrop},
//	{"set-crop-output-overlay", required_argument, 0, OptSetOutputOverlayCrop},
//	{"get-selection", required_argument, 0, OptGetSelection},
//	{"set-selection", required_argument, 0, OptSetSelection},
//	{"get-selection-output", required_argument, 0, OptGetOutputSelection},
//	{"set-selection-output", required_argument, 0, OptSetOutputSelection},
//	{"get-jpeg-comp", no_argument, 0, OptGetJpegComp},
//	{"set-jpeg-comp", required_argument, 0, OptSetJpegComp},
//	{"get-modulator", no_argument, 0, OptGetModulator},
//	{"set-modulator", required_argument, 0, OptSetModulator},
//	{"get-priority", no_argument, 0, OptGetPriority},
//	{"set-priority", required_argument, 0, OptSetPriority},
//	{"wait-for-event", required_argument, 0, OptWaitForEvent},
//	{"poll-for-event", required_argument, 0, OptPollForEvent},
//	{"overlay", required_argument, 0, OptOverlay},
//	{"sleep", required_argument, 0, OptSleep},
	{"list-devices", no_argument, 0, OptListDevices},
//	{"list-dv-timings", no_argument, 0, OptListDvTimings},
//	{"query-dv-timings", no_argument, 0, OptQueryDvTimings},
//	{"get-dv-timings", no_argument, 0, OptGetDvTimings},
//	{"set-dv-bt-timings", required_argument, 0, OptSetDvBtTimings},
//	{"get-dv-timings-cap", no_argument, 0, OptGetDvTimingsCap},
//	{"freq-seek", required_argument, 0, OptFreqSeek},
//	{"encoder-cmd", required_argument, 0, OptEncoderCmd},
//	{"try-encoder-cmd", required_argument, 0, OptTryEncoderCmd},
//	{"decoder-cmd", required_argument, 0, OptDecoderCmd},
//	{"try-decoder-cmd", required_argument, 0, OptTryDecoderCmd},
//	{"set-edid", required_argument, 0, OptSetEdid},
//	{"clear-edid", optional_argument, 0, OptClearEdid},
//	{"get-edid", optional_argument, 0, OptGetEdid},
//	{"info-edid", optional_argument, 0, OptInfoEdid},
//	{"fix-edid-checksums", no_argument, 0, OptFixEdidChecksums},
//	{"tuner-index", required_argument, 0, OptTunerIndex},
//	{"list-buffers", no_argument, 0, OptListBuffers},
//	{"list-buffers-out", no_argument, 0, OptListBuffersOut},
//	{"list-buffers-vbi", no_argument, 0, OptListBuffersVbi},
//	{"list-buffers-sliced-vbi", no_argument, 0, OptListBuffersSlicedVbi},
//	{"list-buffers-vbi-out", no_argument, 0, OptListBuffersVbiOut},
//	{"list-buffers-sliced-vbi-out", no_argument, 0, OptListBuffersSlicedVbiOut},
//	{"list-buffers-sdr", no_argument, 0, OptListBuffersSdr},
//	{"list-buffers-sdr-out", no_argument, 0, OptListBuffersSdrOut},
//	{"list-buffers-meta", no_argument, 0, OptListBuffersMeta},
//	{"stream-count", required_argument, 0, OptStreamCount},
//	{"stream-skip", required_argument, 0, OptStreamSkip},
//	{"stream-loop", no_argument, 0, OptStreamLoop},
//	{"stream-sleep", required_argument, 0, OptStreamSleep},
//	{"stream-poll", no_argument, 0, OptStreamPoll},
//	{"stream-no-query", no_argument, 0, OptStreamNoQuery},
#ifndef NO_STREAM_TO
//	{"stream-to", required_argument, 0, OptStreamTo},
//	{"stream-to-host", required_argument, 0, OptStreamToHost},
#endif
//	{"stream-mmap", optional_argument, 0, OptStreamMmap},
//	{"stream-user", optional_argument, 0, OptStreamUser},
//	{"stream-dmabuf", no_argument, 0, OptStreamDmaBuf},
//	{"stream-from", required_argument, 0, OptStreamFrom},
//	{"stream-from-host", required_argument, 0, OptStreamFromHost},
//	{"stream-out-pattern", required_argument, 0, OptStreamOutPattern},
//	{"stream-out-square", no_argument, 0, OptStreamOutSquare},
//	{"stream-out-border", no_argument, 0, OptStreamOutBorder},
//	{"stream-out-sav", no_argument, 0, OptStreamOutInsertSAV},
//	{"stream-out-eav", no_argument, 0, OptStreamOutInsertEAV},
//	{"stream-out-pixel-aspect", required_argument, 0, OptStreamOutPixelAspect},
//	{"stream-out-video-aspect", required_argument, 0, OptStreamOutVideoAspect},
//	{"stream-out-alpha", required_argument, 0, OptStreamOutAlphaComponent},
//	{"stream-out-alpha-red-only", no_argument, 0, OptStreamOutAlphaRedOnly},
//	{"stream-out-rgb-lim-range", required_argument, 0, OptStreamOutRGBLimitedRange},
//	{"stream-out-hor-speed", required_argument, 0, OptStreamOutHorSpeed},
//	{"stream-out-vert-speed", required_argument, 0, OptStreamOutVertSpeed},
//	{"stream-out-perc-fill", required_argument, 0, OptStreamOutPercFill},
//	{"stream-out-mmap", optional_argument, 0, OptStreamOutMmap},
//	{"stream-out-user", optional_argument, 0, OptStreamOutUser},
//	{"stream-out-dmabuf", no_argument, 0, OptStreamOutDmaBuf},
//	{"list-patterns", no_argument, 0, OptListPatterns},
	{0, 0, 0, 0}
};

static void usage(void)
{
	for (int i = 0; long_options[i].name; i++) {
		std::cout << long_options[i].name << std::endl;
	}
	exit(2);
}

int main(int argc, char* argv[]) {

	if (argc <= 1) usage();

	char short_options[26 * 2 * 3 + 1];
	int idx = 0;
	for (int i = 0; long_options[i].name; i++) {
		if (!isalpha(long_options[i].val))
			continue;
		short_options[idx++] = long_options[i].val;
		if (long_options[i].has_arg == required_argument) {
			short_options[idx++] = ':';
		}
		else if (long_options[i].has_arg == optional_argument) {
			short_options[idx++] = ':';
			short_options[idx++] = ':';
		}
	}

	int device = 0;
	bool getzoom = false, gettilt = false, getpan = false;
	long _zoom=0, _tilt=0, _pan=0;
	bool setzoom = false, settilt = false, setpan = false;
	long zoom, tilt, pan;
	char order[100];
	int n = 0;

	int c;
	char variable[100], value[100];
	while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
		switch (c) {
		case OptListDevices:
			list_devices();
			break;
		case OptSetDevice:
			if (!strncmp(optarg, "/dev/video", strlen("/dev/video"))) device = atoi(optarg + strlen("/dev/video"));
			else device = atoi(optarg);
			break;
		case OptGetCtrl:
			if (!strcmp(optarg, "zoom_absolute")) {
				getzoom = true;
				order[n++] = 0;
			}
			else if (!strcmp(optarg, "pan_absolute")) {
				getpan = true;
				order[n++] = 1;
			}
			else if (!strcmp(optarg, "tilt_absolute")) {
				gettilt = true;
				order[n++] = 2;
			}
			break;
		case OptSetCtrl:
			if (sscanf(optarg, "%[^=]=%[^=]", variable, value) == 2) {
				if (!strcmp(optarg, "zoom_absolute")) {
					setzoom = true;
					zoom = atoi(value);
				}
				else if (!strcmp(optarg, "pan_absolute")) {
					setpan = true;
					pan = atoi(value);
				}
				else if (!strcmp(optarg, "tilt_absolute")) {
					settilt = true;
					tilt = atoi(value);
				}
			}
			break;
		}
	}

	zoom_tilt_pan(
		device,
		getzoom, gettilt, getpan, &_zoom, &_tilt, &_pan,
		setzoom, settilt, setpan, zoom, tilt, pan
	);
	for (int i = 0; i < n; i++) {
		switch (order[i]) {
		case 0:
			std::cout << _zoom << std::endl;
			break;
		case 1:
			std::cout << _tilt << std::endl;
			break;
		case 2:
			std::cout << _pan << std::endl;
			break;
		}
	}

    return 0;
}
