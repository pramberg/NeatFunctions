// Copyright Viktor Pramberg. All Rights Reserved.
#include "NeatFunctionsStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"

FNeatFunctionsStyle::FNeatFunctionsStyle() : FSlateStyleSet("NeatFunctionsStyle")
{
	const FVector2D Icon16x16(16.0f, 16.0f);
	
	const FString& PathToPlugin = IPluginManager::Get().FindPlugin("NeatFunctions")->GetBaseDir();
	SetContentRoot(PathToPlugin / TEXT("Content/Editor"));
	
	Set("NeatFunctions.FunctionIcon", new IMAGE_BRUSH_SVG("FunctionIcon", Icon16x16));
	
	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FNeatFunctionsStyle::~FNeatFunctionsStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}