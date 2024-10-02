#include "PIDefines.h"

#ifdef __PIMac__
	#include <Carbon.r>
	#include "PIGeneral.r"
	#include "PIUtilities.r"
#elif defined(__PIWin__)
	#define Rez
	#include "PIGeneral.h"
	#include "PIUtilities.r"
#endif

#include "PIActions.h"
#include "PITerminology.h"

#define vendorName			"Duke University"
#define categoryName		"Platypus"
#define plugInVersion		"1.0"
#define plugInSuiteID		'Plat'
#define plugInClassID		plugInSuiteID
#define plugInEventID		plugInClassID

#define Key_Parameters 		'parM'

#ifdef __PIWin__
	resource 'vers' ( 1, purgeable )
	{
		6, 0, 0, 0, 6, "1.0"
	}
#endif

#define PLUGIN_RESOURCE_ID	16000
#define PLUGIN_NAME			"PLATYPUS_1.0"
#define PLUGIN_LABEL 		"Platypus"
#define PLUGIN_ENTRY		"PluginMain"
#define PLUGIN_ID			"E28E4093-B898-40CF-9F32-46229DCF7741"
							 
resource 'PiPL' ( PLUGIN_RESOURCE_ID, PLUGIN_NAME, purgeable )
{
	{
		Kind { Filter },
		Name { PLUGIN_LABEL "..." },
		Category { categoryName },
		Version { (latestFilterVersion << 16 ) | latestFilterSubVersion },
		#ifdef __PIWin__
			CodeWin64X86 { PLUGIN_ENTRY },
			CodeWin32X86 { PLUGIN_ENTRY },
		#else
			#if defined(__i386__)
				CodeMacIntel32 { PLUGIN_ENTRY },
			#endif
			#if (defined(__x86_64__))
				CodeMacIntel64 { PLUGIN_ENTRY },
			#endif
		#endif
		SupportedModes
		{
			noBitmap, 
			doesSupportGrayScale,
			noIndexedColor, 
			doesSupportRGBColor,
			noCMYKColor, 
			noHSLColor,
			noHSBColor, 
			noMultichannel,
			noDuotone, 
			noLABColor
		},

		HasTerminology
		{
			plugInClassID,
			plugInEventID,
			PLUGIN_RESOURCE_ID,
			PLUGIN_ID
		},
		
		EnableInfo { 
				"in (PSHOP_ImageMode, GrayScaleMode, RGBMode)" 
		},

		FilterLayerSupport { doesSupportFilterLayers },

		FilterCaseInfo
		{
			{
				/* Flat data, no selection */
				inWhiteMat, outWhiteMat,
				doNotWriteOutsideSelection,
				filtersLayerMasks, doesNotWorkWithBlankData,
				copySourceToDestination,
					
				/* Flat data with selection */
				inWhiteMat, outWhiteMat,
				doNotWriteOutsideSelection,
				filtersLayerMasks, doesNotWorkWithBlankData,
				copySourceToDestination,
				
				/* Floating selection */
				inWhiteMat, outWhiteMat,
				doNotWriteOutsideSelection,
				filtersLayerMasks, doesNotWorkWithBlankData,
				copySourceToDestination,
					
				/* Editable transparency, no selection */
				inWhiteMat, outWhiteMat,
				doNotWriteOutsideSelection,
				filtersLayerMasks, doesNotWorkWithBlankData,
				copySourceToDestination,
					
				/* Editable transparency, with selection */
				inWhiteMat, outWhiteMat,
				doNotWriteOutsideSelection,
				filtersLayerMasks, doesNotWorkWithBlankData,
				copySourceToDestination,
					
				/* Preserved transparency, no selection */
				inWhiteMat, outWhiteMat,
				doNotWriteOutsideSelection,
				filtersLayerMasks, doesNotWorkWithBlankData,
				copySourceToDestination,
					
				/* Preserved transparency, with selection */
				inWhiteMat, outWhiteMat,
				doNotWriteOutsideSelection,
				filtersLayerMasks, doesNotWorkWithBlankData,
				copySourceToDestination
			}
		}	
	}
};

resource 'aete' (PLUGIN_RESOURCE_ID, PLUGIN_NAME " dictionary", purgeable)
{
	1, 0, english, roman,									/* aete version and language specifiers */
	{
		vendorName,											/* vendor suite name */
		PLUGIN_LABEL,										/* optional description */
		plugInSuiteID,										/* suite ID */
		1,													/* suite code, must be 1 */
		1,													/* suite level, must be 1 */
		{													/* structure for filters */
			vendorName " " PLUGIN_LABEL,					/* unique filter name */
			"",												/* optional description */
			plugInClassID,									/* class ID, must be unique or Suite ID */
			plugInEventID,									/* event ID, must be unique to class ID */
			
			NO_REPLY,										/* never a reply */
			IMAGE_DIRECT_PARAMETER,							/* direct parameter, used by Photoshop */
			{												/* parameters here, if any */
				"parameters",								/* parameter name */
				Key_Parameters,								/* parameter key ID */
				typeChar,									/* parameter type ID */
				"",											/* optional description */
				flagsSingleParameter						/* parameter flags */
			}
		},
		{
		},
		{
		},
		{
		}
	}
};

#undef PLUGIN_RESOURCE_ID
#undef PLUGIN_NAME
#undef PLUGIN_LABEL
#undef PLUGIN_ENTRY
#undef PLUGIN_ID
