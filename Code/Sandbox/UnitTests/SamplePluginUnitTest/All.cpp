// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved. 
#include <UnitTest.h>
#include <CryCore/Platform/CryWindows.h> // necessary due to dll_string.h
#include "SamplePlugin.h"

TEST(SamplePluginTests, All)
{
	CSamplePlugin plugin;
	REQUIRE(plugin.GetPluginName() == string("Sample Plugin"));
}