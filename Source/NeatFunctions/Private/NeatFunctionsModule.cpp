// Copyright Viktor Pramberg. All Rights Reserved.
#include "NeatFunctionsStyle.h"
#include "Modules/ModuleManager.h"

class FNeatFunctionsModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		FNeatFunctionsStyle::Get();
	}
};

IMPLEMENT_MODULE(FNeatFunctionsModule, NeatFunctions)