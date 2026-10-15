// guids.cpp - the single TU that instantiates the GUIDs (uuids.h has no
// include guard: including it twice with initguid active would redefine
// every GUID, so only this TU defines INITGUID and it must not pull in
// <streams.h>).

#include <initguid.h> // must come first: DEFINE_GUID now defines

#include "Common.h" // pulls <dshow.h> -> <uuids.h> once: all DShow GUIDs
                    // and CLSID_VirtualCameraFilter are instantiated here.
