// Copyright Viktor Pramberg. All Rights Reserved.
#include "K2Node_NeatCallFunction.h"
#include "K2Node_NeatConstructor.h"
#include "NeatFunctionsStyle.h"
#include "Modules/ModuleManager.h"

class FNeatFunctionsModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		FNeatFunctionsStyle::Get();

		FCoreDelegates::OnPostEngineInit.AddLambda([]()
		{
			for (UFunction* Fn : TObjectRange<UFunction>())
			{
				if (!Fn)
					continue;
				
				if (Fn->HasMetaData(UK2Node_NeatCallFunction::DelegateFunctionMetadataName) ||
					Fn->HasMetaData(UK2Node_NeatConstructor::NeatConstructorMetadataName) ||
					Fn->HasMetaData(UK2Node_NeatConstructor::NeatConstructorFinishMetadataName))
				{
					Fn->SetMetaData(FBlueprintMetadata::MD_BlueprintInternalUseOnly, TEXT("true"));
				}
			}
		});
		
	}
};

IMPLEMENT_MODULE(FNeatFunctionsModule, NeatFunctions)