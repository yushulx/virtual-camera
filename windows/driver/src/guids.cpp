// guids.cpp - the single translation unit that instantiates the GUIDs.

#include <initguid.h> // must come first: DEFINE_GUID now defines, not declares

#include "Common.h" // our CLSIDs + <dshow.h> (pulls uuids.h: MEDIATYPE_Video,
                    // MEDIASUBTYPE_YUY2, FORMAT_VideoInfo, ... now defined).
                    // uuids.h has no include guard - include it exactly once,
                    // which happens through Common.h.
