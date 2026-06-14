#ifndef PERIMETER_NETINCLUDES_H
#define PERIMETER_NETINCLUDES_H

#include <string>
#include <vector>
#include <list>
#include <algorithm>
#include <cinttypes>
#include <climits>
#include <unordered_map>
#ifdef PERIMETER_SDL3
#include <SDL3_net/SDL_net.h>
#else
#include <SDL_net.h>
#endif

#include "tweaks.h"
#include "xutil.h"
#include "xmath.h"
#include "Timers.h"

#include "Umath.h"
#include "IRenderDevice.h"
#include "IVisGeneric.h"
#include "VisGenericDefine.h"
#include "RenderMT.h"

#include "DebugUtil.h"
#include "SystemUtil.h"

#include "NetConnection.h"
#include "NetComEventBuffer.h"
#include "CommonEvents.h"

#endif //PERIMETER_NETINCLUDES_H
