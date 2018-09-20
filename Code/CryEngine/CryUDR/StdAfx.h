// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule eCryM_UniversalDebugRecordings
#include <CryCore/Platform/platform.h>
#include <CryUDR/InterfaceIncludes.h>

// actual serialization is used for dumping recordings to files
#include <CrySerialization/Forward.h>
#include <CrySerialization/Enum.h>
#include <CrySerialization/Math.h>
#include <CrySerialization/Color.h>
#include <CrySerialization/CryStrings.h>
#include <CrySerialization/IArchiveHost.h>

#include <CryRenderer/IRenderAuxGeom.h>

#include "CVars.h"
#include "RenderPrimitives.h"
#include "RenderPrimitiveCollection.h"
#include "Node.h"
#include "NodeStack.h"
#include "Tree.h"
#include "TreeManager.h"
#include "RecursiveSyncObject.h"
#include "Hub.h"
