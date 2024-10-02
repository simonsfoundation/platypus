#ifdef __MACH__
	#include <CoreFoundation/CoreFoundation.h>
#endif

#include "PIAbout.h"
#include "PIUIHooksSuite.h"
#include "PIActions.h"
#include "PITerminology.h"
#include "PIStringTerminology.h"
#include "PIFilter.h"
#include "PIChannelPortsSuite.h"			// Channel Ports Suite Photoshop header file.
#include "PIColorSpaceSuite.h"
#include "PIBufferSuite.h"					// Buffer Suite Photoshop header file.
#include "suites.h"
#include "tiff.h"
#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

static const char *kProductName = "Platypus";
static const char *kProductBundleID = "com.digitalfilmtools.platypus.photoshop";
static const char *kProductCopyright = "Platypus (c) 2015 Duke University";

#undef min
#undef max

#ifdef __MACH__
	#define DLLExport extern "C" __attribute__((visibility("default")))
	extern "C" int doUserInterface(CFStringRef cmd, CFArrayRef args);
#else 
	#define DLLExport extern "C" __declspec(dllexport)
#endif


// suites
static SPBasicSuite *s_SPBasic = NULL;
static FilterRecord *s_filterRecord;
static const PSChannelPortsSuite1 *s_chanPorts;

#define Key_Parameters 	'parM'

extern "C" int messageBox(void *parent, const char *title, const char *message, const char *okText = NULL, const char *cancelText = NULL);

struct PSLayer : public ReadLayerDesc
{
	int numChannels() const
	{
		int count = 0;
		const ReadChannelDesc *chan = compositeChannelsList;
		while (chan)
		{
			count++;
			chan = chan->next;
		}
		return count;		
	}

	bool isRGB() const
	{
		if (numChannels() >= 3)
		{
			bool got_r = false;
			bool got_g = false;
			bool got_b = false;
			const ReadChannelDesc *chan = compositeChannelsList;
			while (chan)
			{
				if (chan->channelType == ctRed)
					got_r = true;
				else if (chan->channelType == ctGreen)
					got_g = true;
				else if (chan->channelType == ctBlue)
					got_b = true;
				chan = chan->next;
			}
			return got_r && got_g && got_b;
		}
		return false;
	}

	bool hasMask() const { return layerMask != NULL; }
	bool isActive() const { return activeChannel() != NULL; }
	const ReadChannelDesc *activeChannel() const
	{
		const ReadChannelDesc *chan = compositeChannelsList;
		while (chan)
		{
			if (chan->target)
				return chan;

			chan = chan->next;
		}
		return NULL;
	}
};

static bool isShiftDown()
{
	#if defined(WIN32)
		return ::GetAsyncKeyState(VK_LSHIFT) != 0;
	#endif
    return false;
}

// launch the UI with the specified arguments and wait for it to exit
static int DoUserInterface(const std::string &appPath, const std::vector<std::string> &args, void *parentWindow)
{
    int result = 0;
	#ifdef WIN32
		HWND prevWin = ::GetForegroundWindow();
	#endif
    
    #if defined(__MACH__)
        CFStringRef cmd = CFStringCreateWithCString(kCFAllocatorDefault, appPath.c_str(), kCFStringEncodingUTF8);
        CFMutableArrayRef array = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
        for (size_t i = 0; i < args.size(); i++)
        {
            CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, args[i].c_str(), kCFStringEncodingUTF8);
            CFArrayAppendValue(array, str);
            //CFRelease(str);
        }
        result = doUserInterface(cmd, array);
        CFRelease(array);
        CFRelease(cmd);
        return result;
    #endif

	#if defined(WIN32)

		std::string cmdLine = appPath;
		for (auto arg : args)
			cmdLine += " \"" + arg + "\"";

		PROCESS_INFORMATION pi = { 0 };
		STARTUPINFO si = { 0 };
		si.cb = sizeof(si);
		if (::CreateProcessA(NULL, (LPSTR)cmdLine.c_str(), NULL, NULL, FALSE, CREATE_DEFAULT_ERROR_MODE | DETACHED_PROCESS, NULL, NULL, &si, &pi))
		{
			DWORD timeout = 10;
			while (::WaitForSingleObject(pi.hProcess, timeout))
			{
				MSG msg;
				while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
				{
					if (msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST)
					{
						// throw away mouse events
					}
					else if (msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST)
					{
						// throw away keyboard events
					}
					else
					{
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}
				}
			}

			DWORD exitCode;
			::GetExitCodeProcess(pi.hProcess, &exitCode);
			result = exitCode;

			::CloseHandle(pi.hProcess);
			::CloseHandle(pi.hThread);
		}
	#endif

	// restore previous active window
	#ifdef WIN32
		if (prevWin == (HWND)parentWindow)
			::SetForegroundWindow(prevWin);
	#endif

	return result;
}

static PSLayer getOutputLayer(const ReadImageDocumentDesc *doc)
{
	PSLayer fakeLayer;
	::memset(&fakeLayer, 0, sizeof(PSLayer));
	fakeLayer.compositeChannelsList = doc->targetCompositeChannels;
	fakeLayer.layerMask = doc->targetLayerMask;
	fakeLayer.name = "Current Layer";
	fakeLayer.isVisible = 1;
	
	return fakeLayer;
}

static PSLayer getInputLayer(const ReadImageDocumentDesc *doc)
{
    return getOutputLayer(doc);
}

static std::string getTempPath()
{
    const char *env = ::getenv("TMPDIR");
    if (!env)
        env = ::getenv("TEMP");
    
    if (env)
        return env;
    
    return std::string();
}

static std::string getAppPath(SPPluginRef pluginRef)
{
	std::string result;

	// get the path to ourself and use that for the app name
	SPPlatformFileSpecification pluginPath = { 0 };
	try
	{
		SuiteHandler<SPPluginsSuite> piSuite(s_SPBasic, kSPPluginsSuite, kSPPluginsSuiteVersion4);
		piSuite->GetPluginFileSpecification(pluginRef, &pluginPath);
	}
	catch (...)
	{
		#ifdef WIN32
			//GetPluginPath(pluginPath.path, sizeof(pluginPath.path));
		#endif
	}

	#ifdef WIN32
		// this is the path to Plugin.8bf
		result = pluginPath.path;

		size_t name_pos = result.rfind('\\');
		size_t ext_pos = result.rfind(".8bf");
		if (ext_pos != std::string::npos)
			result.replace(ext_pos, 4, ".exe");

	#else
		CFStringRef bundle_id = CFStringCreateWithCString(NULL, kProductBundleID, kCFStringEncodingASCII);
		CFBundleRef bundle = CFBundleGetBundleWithIdentifier(bundle_id);
		CFRelease(bundle_id);
		if (bundle)
		{
			CFURLRef bundle_url = CFBundleCopyBundleURL(bundle);
            CFURLRef bundle_root = CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault, bundle_url);
			CFRelease(bundle_url);

            CFStringRef bundleName = (CFStringRef)CFBundleGetValueForInfoDictionaryKey(bundle, kCFBundleExecutableKey);
            CFStringRef appPath = CFStringCreateWithFormat(kCFAllocatorDefault, nil,
                    CFSTR("%@.app/Contents/MacOS/%@"),
                    bundleName, bundleName);
            
            CFURLRef appPathURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault, bundle_root, appPath, false);
            
			CFRelease(bundle_root);
			CFRelease(appPath);
            
			char path[1024];
			::CFURLGetFileSystemRepresentation(appPathURL, true, (UInt8 *)path, sizeof(path));
			result = path;

			CFRelease(appPathURL);
		}
	#endif

	return result;
}

//
// TILE I/O
//

static void readPlane(const PixelMemoryDesc &plane, const VRect *tile, PSScaling &scaling, const ReadChannelDesc *chan)
{
	if (s_chanPorts)
	{
		if ((tile->right - tile->left) == scaling.destinationRect.right && (tile->bottom - tile->top) == scaling.destinationRect.bottom && chan->depth == plane.depth)
		{
			VRect bounds = scaling.sourceRect;
			s_chanPorts->ReadPixelsFromLevel(chan->port, 0, &bounds, &plane);
		}
		else
		{
			VRect bounds = scaling.destinationRect;
			s_chanPorts->ReadScaledPixels(chan->port, &bounds, &scaling, &plane);
		}
	}
	else
	{
	}
}

static void ReadTile(const ReadLayerDesc *layer, const ReadChannelDesc *layerMask, const VRect *tile, const VPoint *size, unsigned char *buffer, int row_bytes, int chan_bytes)
{
	const ReadImageDocumentDesc *doc = s_filterRecord->documentInfo;
	const ReadChannelDesc *chan = layer->compositeChannelsList;
	const BufferProcs *bufferProcs = s_filterRecord->bufferProcs;

	PixelMemoryDesc plane;
    plane.data = buffer;
    plane.rowBits = row_bytes * 8;
    plane.colBits = 8 * chan_bytes;
    plane.bitOffset = 0;
    plane.depth = chan_bytes * 8;

	// read the planes
	PSScaling scaling;
	scaling.sourceRect.left = tile->left + chan->bounds.left;
	scaling.sourceRect.top = tile->top + chan->bounds.top;
	scaling.sourceRect.right = tile->right + chan->bounds.left;
	scaling.sourceRect.bottom = tile->bottom + chan->bounds.top;
	scaling.destinationRect.left = 0;
	scaling.destinationRect.top = 0;
	scaling.destinationRect.right = size->h;
	scaling.destinationRect.bottom = size->v;

    readPlane(plane, tile, scaling, chan);
}

static void WriteTile(const ReadLayerDesc *layer, const ReadChannelDesc *layerMask, const VRect *tile, const unsigned char *buffer, int row_bytes, int chan_bytes)
{
	const ReadChannelDesc *chan = layer->compositeChannelsList;

	if (s_chanPorts)
	{
        PIChannelPort port = chan->writePort;

        PixelMemoryDesc plane = { 0 };
        int pixel_bytes = chan_bytes;
        plane.rowBits = row_bytes * 8;
        plane.colBits = pixel_bytes * 8;
        plane.bitOffset = 0;
        plane.depth = chan_bytes * 8;
        plane.data = (void *)buffer;

        VRect write_tile = *tile;
        write_tile.left += chan->bounds.left;
        write_tile.top += chan->bounds.top;
        write_tile.right += chan->bounds.left;
        write_tile.bottom += chan->bounds.top;
        s_chanPorts->WritePixelsToBaseLevel(port, &write_tile, &plane);
    }
}

//
// Image File
//
class ImageFile
{
	const FilterRecord *m_fr;
    std::string m_path;
public:
	ImageFile(const std::string &path, const FilterRecord *fr) : m_fr(fr), m_path(path)
	{
	}
	~ImageFile()
	{
		::remove(m_path.c_str());
	}

	bool write(const PSLayer &layer, const ReadChannelDesc *layerMask = NULL) const
	{
		Point imageSize = m_fr->imageSize;
        
        ::remove(m_path.c_str());

		TIFFImage tiff;
		if (tiff.create(m_path.c_str(), imageSize.h, imageSize.v))
		{
			int chanBytes = m_fr->depth / 8;
			int pixelBytes = chanBytes;
			int rowBytes = imageSize.h * pixelBytes;
			int tileHeight = m_fr->outTileHeight;
			int tileBytes = tileHeight * rowBytes;

			bool error = false;

			// allocate a buffer for the tile data
			BufferID id;
			if (m_fr->bufferProcs->allocateProc(tileBytes, &id) == noErr)
			{
				Ptr ptr = m_fr->bufferProcs->lockProc(id, false);
				if (ptr)
				{
					VRect tile;
					tile.left = 0;
					tile.right = imageSize.h;

					// read in destination tile sizes
					tile.top = 0;
					while (tile.top < imageSize.v && !error)
					{
						tile.bottom = tile.top + tileHeight;
						tile.bottom = std::min((int)tile.bottom, (int)imageSize.v);

						VPoint readSize;
						readSize.h = imageSize.h;
						readSize.v = tile.bottom - tile.top;
						ReadTile(&layer, layerMask, &tile, &readSize, (unsigned char *)ptr, rowBytes, chanBytes);

						if (!tiff.write(ptr, tile.top, tile.bottom - tile.top, rowBytes))
							error = true;

						tile.top = tile.bottom;
					}
				}
				else
					error = true;
				// release the buffer
				m_fr->bufferProcs->unlockProc(id);
				m_fr->bufferProcs->freeProc(id);
			}
			else
				error = true;
			
			tiff.close();

			return !error;
		}
		return false;
	}

	bool read(PSLayer &layer, ReadChannelDesc *layerMask = NULL) const
	{
		TIFFImage tiff;
		if (tiff.open(m_path.c_str()))
		{
			Point imageSize = m_fr->imageSize;
			
			if (tiff.width() != imageSize.h || tiff.height() != imageSize.v)
				return false;
			
			int chanBytes = m_fr->depth / 8;
			int pixelBytes = chanBytes;
			int rowBytes = imageSize.h * pixelBytes;
			int tileHeight = m_fr->outTileHeight;
			int tileBytes = tileHeight * rowBytes;

			bool error = false;

			// allocate a buffer for the tile data
			BufferID id;
			if (m_fr->bufferProcs->allocateProc(tileBytes, &id) == noErr)
			{
				Ptr ptr = m_fr->bufferProcs->lockProc(id, false);
				if (ptr)
				{
					Rect selection = m_fr->filterRect;
					
					VRect tile;
					tile.left = selection.left;
					tile.right = selection.right;

					// read in destination tile sizes
					tile.top = selection.top;
					while (tile.top < selection.bottom && !error)
					{
						tile.bottom = tile.top + tileHeight;
						tile.bottom = std::min((int)tile.bottom, (int)selection.bottom);

						if (tiff.read(ptr, tile.top, tile.bottom - tile.top, rowBytes))
							WriteTile(&layer, layerMask, &tile, (unsigned char *)ptr + (tile.left * pixelBytes), rowBytes, chanBytes);
						else
							error = true;

						tile.top = tile.bottom;
					}
				}
				else
					error = true;
				// release the buffer
				m_fr->bufferProcs->unlockProc(id);
				m_fr->bufferProcs->freeProc(id);
			}
			else
				error = true;
			
			tiff.close();

			return !error;
		}
		return false;
	}
};

//
// MAIN
//
DLLExport MACPASCAL void PluginMain (const short selector,
						             FilterRecord *filterRecord,
						             long *data,
						             short *result)
{
	*result = noErr;

	SPPluginRef pluginRef = NULL;
	const PlatformData *platformData = NULL;
	if (selector == filterSelectorAbout)
	{
		s_SPBasic = ((const AboutRecord *)filterRecord)->sSPBasic;
		pluginRef = (SPPluginRef)((const AboutRecord *)filterRecord)->plugInRef;
		platformData = (const PlatformData *)((const AboutRecord *)filterRecord)->platformData;
	}
	else if (selector == filterSelectorStart)
	{
		s_SPBasic = filterRecord->sSPBasic;
		pluginRef = (SPPluginRef)filterRecord->plugInRef;
		platformData = (const PlatformData *)filterRecord->platformData;
 	}
	else
		return;

	// disable the host window
	#ifdef WIN32
		HWND hWnd = (HWND)platformData->hwnd;
		if (hWnd)
			::EnableWindow(hWnd, false);
	#else
		void *hWnd = NULL;
	#endif

	if (selector == filterSelectorAbout)
	{
        messageBox(hWnd, kProductName, kProductCopyright);
        *result = noErr;
	}
	else if (selector == filterSelectorStart)
	{
		try
		{
			SuiteHandler<PSChannelPortsSuite1> chanPorts(s_SPBasic, kPSChannelPortsSuite, kPSChannelPortsSuiteVersion2);
			s_chanPorts = chanPorts.get();
		}
		catch (...)
		{
			messageBox(hWnd, kProductName, "ChannelPorts not available.");
			return;
		}

		const ReadImageDocumentDesc *doc = filterRecord->documentInfo;

		s_filterRecord = filterRecord;

		// fetch the target layer
		PSLayer input = getInputLayer(doc);
		PSLayer output = getOutputLayer(doc);
        
        if (input.numChannels() != 1)
        {
            messageBox(hWnd, kProductName, "Image mode must be Grayscale.");
            *result = 1;
        }
        else
        {
            std::string tempPath = getTempPath();
            std::string appPath = getAppPath(pluginRef);
            std::string imagePath = tempPath + '/' + "platypus.tif";
            
            ImageFile imageFile(imagePath, filterRecord);
            imageFile.write(input);
            
            std::vector<std::string> args;
            args.push_back(imagePath);
            
            int exitCode = DoUserInterface(appPath, args, hWnd);
            
            // check for abort
            if (filterRecord->abortProc())
                exitCode = 1;

            if (exitCode == 0)
            {
                *result = noErr;

                // read image into the output layer
                imageFile.read(output);
            }
            else
                *result = userCanceledErr;
        }
            
 		s_filterRecord = NULL;
	}

	// enable the host window
	#ifdef WIN32
		::EnableWindow(hWnd, true);
		::SetActiveWindow(hWnd);
	#endif
}
