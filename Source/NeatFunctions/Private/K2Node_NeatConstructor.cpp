// Copyright Viktor Pramberg. All Rights Reserved.


#include "K2Node_NeatConstructor.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintCompilationManager.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_IfThenElse.h"
#include "KismetCompiler.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "NeatFunctionsRuntime/Public/NeatFunctionsStatics.h"
#include "Styling/SlateIconFinder.h"

DEFINE_LOG_CATEGORY_STATIC(LogNeatFunctions, Log, All);

namespace
{
	const UClass* GetClassParameterMetaClass(const UFunction& InFunction)
	{
		const FClassProperty* ClassProp = CastField<FClassProperty>(InFunction.FindPropertyByName(NAME_Class));
		return ClassProp ? ClassProp->MetaClass : nullptr;
	}

	bool HasValidReturnValue(const UFunction& InFunction)
	{
		const FObjectProperty* ReturnProperty = CastField<FObjectProperty>(InFunction.GetReturnProperty());
		return ReturnProperty ?  GetClassParameterMetaClass(InFunction)->IsChildOf(ReturnProperty->PropertyClass) : false;
	}

	const FObjectProperty* GetFinishFunctionObjectProperty(const UFunction& InFunction, const UClass& SpawnClassParameter)
	{
		for (TFieldIterator<FProperty> PropIt(&InFunction); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			FProperty* Param = *PropIt;
			const bool bIsFunctionInput = !Param->HasAnyPropertyFlags(CPF_ReturnParm) && (!Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm));
			if (!bIsFunctionInput)
				continue;

			const FObjectProperty* ObjectProp = CastField<FObjectProperty>(Param);
			if (!ObjectProp)
				continue;

			if (SpawnClassParameter.IsChildOf(ObjectProp->PropertyClass))
			{
				return ObjectProp;
			}
		}

		return nullptr;
	}

	bool HasValidFinishFunction(const UFunction& InFunction, FString& OutError)
	{
		const FString* FinishFuncName = InFunction.FindMetaData(UK2Node_NeatConstructor::NeatConstructorFinishMetadataName);
		if (!FinishFuncName)
			return true;

		const UFunction* FinishFunction = InFunction.GetOwnerClass()->FindFunctionByName(FName(*FinishFuncName));
		if (FinishFunction == nullptr)
		{
			OutError = FString::Printf(TEXT("Cannot find function called %s in class %s."), **FinishFuncName, *GetNameSafe(InFunction.GetOwnerClass()));
			return false;
		}

		const FObjectProperty* ReturnProperty = CastField<FObjectProperty>(InFunction.GetReturnProperty());
		const FObjectProperty* Prop = GetFinishFunctionObjectProperty(*FinishFunction, *ReturnProperty->PropertyClass);
		if (!Prop)
		{
			OutError = FString::Printf(TEXT("%s does not have a valid input. Must have an input that the spawned object can be converted to."), **FinishFuncName);
			return false;
		}
		
		return true;
	}

	bool ValidateFunction(const UFunction* InFunction)
	{
		if (!InFunction)
			return false;

		const UFunction& Fn = *InFunction;

		const bool bHasAnyMetadata = Fn.HasMetaData(UK2Node_NeatConstructor::NeatConstructorMetadataName) || Fn.HasMetaData(UK2Node_NeatConstructor::NeatConstructorFinishMetadataName);
		if (!bHasAnyMetadata)
			return false;

		if (!GetClassParameterMetaClass(Fn))
		{
			UE_LOG(LogNeatFunctions, Error, TEXT("Cannot create NeatConstructor for %s. Function does not have a parameter named \"Class\". This parameter must be of `TSubclassOf<SomeType>`"), *GetNameSafe(&Fn))
			return false;
		}

		if (!HasValidReturnValue(Fn))
		{
			UE_LOG(LogNeatFunctions, Error, TEXT("Cannot create NeatConstructor for %s. Function does not have a valid return value. Must return an object of the same class as the \"Class\" parameter."), *GetNameSafe(&Fn))
			return false;
		}

		FString Error;
		if (!HasValidFinishFunction(Fn, Error))
		{
			UE_LOG(LogNeatFunctions, Error, TEXT("Cannot create NeatConstructor for %s. Function does not have a valid finish function. %s"), *GetNameSafe(&Fn), *Error);
			return false;
		}
		
		return true;
	}
}

void UK2Node_NeatConstructor::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* NodeClass = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(NodeClass))
	{
		for (const UFunction* Fn : TObjectRange<UFunction>())
		{
			if (!ValidateFunction(Fn))
				continue;

			UBlueprintFieldNodeSpawner* NodeSpawner = UBlueprintFieldNodeSpawner::Create(NodeClass, Fn);
			check(NodeSpawner != nullptr);

			NodeSpawner->DefaultMenuSignature.MenuName = UK2Node_CallFunction::GetUserFacingFunctionName(Fn);
			NodeSpawner->DefaultMenuSignature.Category = UK2Node_CallFunction::GetDefaultCategoryForFunction(Fn, FText::GetEmpty());
			NodeSpawner->DefaultMenuSignature.Tooltip = FText::FromString(UK2Node_CallFunction::GetDefaultTooltipForFunction(Fn));
			NodeSpawner->DefaultMenuSignature.Keywords = UK2Node_CallFunction::GetKeywordsForFunction(Fn);

			NodeSpawner->DefaultMenuSignature.Icon = FSlateIconFinder::FindIconForClass(GetClassParameterMetaClass(*Fn));
			NodeSpawner->DefaultMenuSignature.IconTint = FLinearColor::White;
			NodeSpawner->CustomizeNodeDelegate.BindLambda([Fn](UEdGraphNode* Node, bool)
			{
				ThisClass* ThisNode = Cast<ThisClass>(Node);
				ThisNode->FunctionReference.SetFromField<UFunction>(Fn, ThisNode->GetBlueprintClassFromNode());
			});

			ActionRegistrar.AddBlueprintAction(NodeClass, NodeSpawner);
		}
	}
}

void UK2Node_NeatConstructor::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	if (GetTargetFunction()->HasMetaData(NeatObjectValidationMetadataName))
	{
		FCreatePinParams Params;
		Params.Index = GetPinIndex(GetThenPin()) + 1;
		UEdGraphPin* ThenIsNotValid = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Else, Params);
		ThenIsNotValid->PinFriendlyName = INVTEXT("Not Valid");
		ThenIsNotValid->PinToolTip = TEXT("Executed when there wasn't an object spawned successfully.");
		GetThenPin()->PinFriendlyName = INVTEXT("Valid");
		GetThenPin()->PinToolTip = TEXT("Executed when we spawned an object successfully.");
	}

	CreatePinsForFunction(GetTargetFunction());
	CreatePinsForFunction(GetFinishFunction());
}

void UK2Node_NeatConstructor::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const UEdGraphPin* SpawnClassPin = GetClassPin();
	const UClass* SpawnClass = SpawnClassPin ? Cast<UClass>(SpawnClassPin->DefaultObject) : nullptr;
	if (!SpawnClassPin || ((!SpawnClass) && (SpawnClassPin->LinkedTo.Num() == 0)))
	{
		CompilerContext.MessageLog.Error(TEXT("@@ must have a class specified"), this);
		// we break exec links so this is the only error we get, don't want this node being considered and giving 'unexpected node' type warnings
		BreakAllNodeLinks();
		return;
	}

	// A few lines down we move the class pin, so cache off the ClassToSpawn before doing that.
	const UClass* ClassToSpawn = GetClassToSpawn();

	
	UK2Node_CallFunction* BeginSpawnFunc = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	BeginSpawnFunc->SetFromFunction(GetTargetFunction());
	BeginSpawnFunc->AllocateDefaultPins();

	BeginSpawnFunc->GetReturnValuePin()->PinType = GetResultPin()->PinType;
	BeginSpawnFunc->PinTypeChanged(BeginSpawnFunc->GetReturnValuePin());

	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *BeginSpawnFunc->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*GetClassPin(), *BeginSpawnFunc->FindPin(NAME_Class));
	CompilerContext.MovePinLinksToIntermediate(*GetResultPin(), *BeginSpawnFunc->GetReturnValuePin());

	for (UEdGraphPin* CurrentPin : Pins)
	{
		if (CurrentPin && CurrentPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
		{
			if (UEdGraphPin* DestPin = BeginSpawnFunc->FindPin(CurrentPin->PinName))
			{
				// If we're an input, it's technically possible for it to be an input to both the spawn node and the finish node.
				// So don't move inputs, copy them instead.
				if (CurrentPin->Direction == EGPD_Input)
				{
					CompilerContext.CopyPinLinksToIntermediate(*CurrentPin, *DestPin);
				}
				else
				{
					CompilerContext.MovePinLinksToIntermediate(*CurrentPin, *DestPin);
				}
			}
		}
	}

	UEdGraphPin* LastThen = FKismetCompilerUtilities::GenerateAssignmentNodes(CompilerContext, SourceGraph, BeginSpawnFunc, this, BeginSpawnFunc->GetReturnValuePin(), ClassToSpawn);

	if (GetFinishFunction())
	{
		UK2Node_CallFunction* FinishSpawnFunc = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		FinishSpawnFunc->SetFromFunction(GetFinishFunction());
		FinishSpawnFunc->AllocateDefaultPins();
		
		for (UEdGraphPin* CurrentPin : Pins)
		{
			if (CurrentPin && CurrentPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				if (UEdGraphPin* DestPin = FinishSpawnFunc->FindPin(CurrentPin->PinName))
				{
					if (CurrentPin->Direction == EGPD_Input)
					{
						CompilerContext.CopyPinLinksToIntermediate(*CurrentPin, *DestPin);
					}
					else
					{
						CompilerContext.MovePinLinksToIntermediate(*CurrentPin, *DestPin);
					}
				}
			}
		}

		const FName InputName = GetFinishFunctionObjectInputName();
		BeginSpawnFunc->GetReturnValuePin()->MakeLinkTo(FinishSpawnFunc->FindPin(InputName));
		
		LastThen->MakeLinkTo(FinishSpawnFunc->GetExecPin());
		LastThen = FinishSpawnFunc->GetThenPin();
	}
	
	if (GetTargetFunction()->HasMetaData(NeatObjectValidationMetadataName))
	{
		UK2Node_CallFunction* IsValidFuncNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		const FName IsValidFuncName = GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, IsValid);
		IsValidFuncNode->FunctionReference.SetExternalMember(IsValidFuncName, UKismetSystemLibrary::StaticClass());
		IsValidFuncNode->AllocateDefaultPins();
		UEdGraphPin* IsValidInputPin = IsValidFuncNode->FindPinChecked(TEXT("Object"));

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		Schema->TryCreateConnection(BeginSpawnFunc->GetReturnValuePin(), IsValidInputPin);

		UK2Node_IfThenElse* IfElseNode = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
		IfElseNode->AllocateDefaultPins();
		Schema->TryCreateConnection(IsValidFuncNode->GetReturnValuePin(), IfElseNode->GetConditionPin());
		
		CompilerContext.CopyPinLinksToIntermediate(*FindPin(UEdGraphSchema_K2::PN_Else), *IfElseNode->GetElsePin());
		
		LastThen->MakeLinkTo(IfElseNode->GetExecPin());
		LastThen = IfElseNode->GetThenPin();
	}

	CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *LastThen);
}

UClass* UK2Node_NeatConstructor::GetClassPinBaseClass() const
{
	if (const UFunction* Fn = GetTargetFunction())
	{
		const FClassProperty* ClassProp = CastField<FClassProperty>(Fn->FindPropertyByName(NAME_Class));
		if (!ClassProp)
			return nullptr;

		return ClassProp->MetaClass;
	}

	return AActor::StaticClass();
}

void UK2Node_NeatConstructor::CreatePinsForClass(UClass* InClass, TArray<UEdGraphPin*>* OutClassPins)
{
	TArray<UEdGraphPin*> CreatedPins;
	Super::CreatePinsForClass(InClass, &CreatedPins);

	const FString& IgnorePropertyListStr = GetTargetFunction()->GetMetaData(FName(TEXT("HideSpawnParms")));
	if (!IgnorePropertyListStr.IsEmpty())
	{
		TArray<FString> IgnorePropertyList;
		IgnorePropertyListStr.ParseIntoArray(IgnorePropertyList, TEXT(","), true);

		for (UEdGraphPin* Pin : CreatedPins)
		{
			const int32 Index = IgnorePropertyList.IndexOfByKey(Pin->GetName());
			if (Index != INDEX_NONE)
			{
				RemovePin(Pin);
				IgnorePropertyList.RemoveAtSwap(Index);
			}
		}
	}

	if (OutClassPins)
		OutClassPins->Append(MoveTemp(CreatedPins));
}

bool UK2Node_NeatConstructor::IsSpawnVarPin(UEdGraphPin* Pin) const
{
	for (TFieldIterator<FProperty> PropIt(GetTargetFunction()); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		if (PropIt->GetFName() == Pin->GetFName())
			return false;
	}

	for (TFieldIterator<FProperty> PropIt(GetFinishFunction()); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		if (PropIt->GetFName() == Pin->GetFName())
			return false;
	}

	if (Pin->GetFName() == UEdGraphSchema_K2::PN_Else)
		return false;

	if (Pin->GetFName() == GetFinishFunctionObjectInputName())
		return false;

	return Super::IsSpawnVarPin(Pin);
}

FText UK2Node_NeatConstructor::GetBaseNodeTitle() const
{
	if (const UFunction* Fn = GetTargetFunction())
	{
		return Fn->GetDisplayNameText();
	}
	return Super::GetBaseNodeTitle();
}

FText UK2Node_NeatConstructor::GetDefaultNodeTitle() const
{
	if (const UFunction* Fn = GetTargetFunction())
	{
		return FText::Format(INVTEXT("{0}\nNo Class Selected"), Fn->GetDisplayNameText());
	}
	return Super::GetDefaultNodeTitle();
}

FText UK2Node_NeatConstructor::GetNodeTitleFormat() const
{
	if (const UFunction* Fn = GetTargetFunction())
	{
		FTextBuilder Builder;
		Builder.AppendLine(Fn->GetDisplayNameText());
		Builder.AppendLine(INVTEXT("{ClassName}"));
		return Builder.ToText();
	}
	return Super::GetNodeTitleFormat();
}

FText UK2Node_NeatConstructor::GetTooltipText() const
{
	return FText::FromString(UK2Node_CallFunction::GetDefaultTooltipForFunction(GetTargetFunction()));
}

FText UK2Node_NeatConstructor::GetMenuCategory() const
{
	if (const UFunction* TargetFunction = GetTargetFunction())
	{
		return UK2Node_CallFunction::GetDefaultCategoryForFunction(TargetFunction, FText::GetEmpty());
	}
	return FText::GetEmpty();
}

FSlateIcon UK2Node_NeatConstructor::GetIconAndTint(FLinearColor& OutColor) const
{
	return FSlateIconFinder::FindIconForClass(GetClassPinBaseClass());
}

void UK2Node_NeatConstructor::EarlyValidation(FCompilerResultsLog& MessageLog) const
{
	Super::EarlyValidation(MessageLog);

	if (!GetTargetFunction())
	{
		MessageLog.Error(TEXT("@@ references function \"@@\" that has been removed from class @@"), this, *FunctionReference.GetMemberName().ToString(), FunctionReference.GetMemberParentClass());
	}
}

namespace
{
	// Avoid writing a bunch of duplicated code by maintaining CallFunction nodes for our functions that we can delegate work to.
	TMap<TWeakObjectPtr<UFunction>, TStrongObjectPtr<UK2Node_CallFunction>> HelperLUT;
	const UK2Node_CallFunction& GetHelperCallFunctionNode(UFunction* InFunction)
	{
		if (const TStrongObjectPtr<UK2Node_CallFunction>* Node = HelperLUT.Find(InFunction))
		{
			check(Node->IsValid());
			return *Node->Get();
		}

		UK2Node_CallFunction* NewNode = NewObject<UK2Node_CallFunction>();
		NewNode->FunctionReference.SetFromField<UFunction>(InFunction, false);
		HelperLUT.Add(InFunction, TStrongObjectPtr(NewNode));
		return *NewNode;
	}
}

bool UK2Node_NeatConstructor::CanJumpToDefinition() const
{
	return GetHelperCallFunctionNode(GetTargetFunction()).CanJumpToDefinition();
}

void UK2Node_NeatConstructor::JumpToDefinition() const
{
	GetHelperCallFunctionNode(GetTargetFunction()).JumpToDefinition();
}

void UK2Node_NeatConstructor::CreatePinsForFunction(const UFunction* InFunction)
{
	if (!InFunction)
		return;

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	TSet<FName> PinsToHide;
	TSet<FName> InternalPins;
	FBlueprintEditorUtils::GetHiddenPinsForFunction(GetGraph(), InFunction, PinsToHide, &InternalPins);

	UEdGraph const* const Graph = GetGraph();
	UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);

	const bool bShowWorldContextPin = ((PinsToHide.Num() > 0) && BP && BP->ParentClass && BP->ParentClass->HasMetaDataHierarchical(FBlueprintMetadata::MD_ShowWorldContextPin));

	const FName InputName = GetFinishFunctionObjectInputName();

	// Create the inputs and outputs
	bool bAllPinsGood = true;
	for (TFieldIterator<FProperty> PropIt(InFunction); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		FProperty* Param = *PropIt;
		const bool bIsFunctionInput = !Param->HasAnyPropertyFlags(CPF_ReturnParm) && (!Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm));
		const bool bIsRefParam = Param->HasAnyPropertyFlags(CPF_ReferenceParm) && bIsFunctionInput;

		const EEdGraphPinDirection Direction = bIsFunctionInput ? EGPD_Input : EGPD_Output;

		// If the pin is already created by the construct object node, don't create it again.
		if (FindPin(Param->GetFName(), Direction) || Param->GetFName() == InputName)
			continue;

		UEdGraphNode::FCreatePinParams PinParams;
		PinParams.bIsReference = bIsRefParam;
		UEdGraphPin* Pin = CreatePin(Direction, NAME_None, Param->GetFName(), PinParams);
		const bool bPinGood = (Pin && K2Schema->ConvertPropertyToPinType(Param, /*out*/ Pin->PinType));

		if (bPinGood)
		{
			// Check for a display name override
			const FString& PinDisplayName = Param->GetMetaData(FBlueprintMetadata::MD_DisplayName);
			if (!PinDisplayName.IsEmpty())
			{
				Pin->PinFriendlyName = FText::FromString(PinDisplayName);
			}
			else if (InFunction->GetReturnProperty() == Param && InFunction->HasMetaData(FBlueprintMetadata::MD_ReturnDisplayName))
			{
				Pin->PinFriendlyName = InFunction->GetMetaDataText(FBlueprintMetadata::MD_ReturnDisplayName);
			}

			//Flag pin as read only for const reference property
			Pin->bDefaultValueIsIgnored = Param->HasAllPropertyFlags(CPF_ConstParm | CPF_ReferenceParm) && (!InFunction->HasMetaData(FBlueprintMetadata::MD_AutoCreateRefTerm) || Pin->PinType.IsContainer());

			const bool bAdvancedPin = Param->HasAllPropertyFlags(CPF_AdvancedDisplay);
			Pin->bAdvancedView = bAdvancedPin;
			if (bAdvancedPin && (ENodeAdvancedPins::NoPins == AdvancedPinDisplay))
			{
				AdvancedPinDisplay = ENodeAdvancedPins::Hidden;
			}

			FString ParamValue;
			if (K2Schema->FindFunctionParameterDefaultValue(InFunction, Param, ParamValue))
			{
				K2Schema->SetPinAutogeneratedDefaultValue(Pin, ParamValue);
			}
			else
			{
				K2Schema->SetPinAutogeneratedDefaultValueBasedOnType(Pin);
			}

			if (PinsToHide.Contains(Pin->PinName))
			{
				const FString PinNameStr = Pin->PinName.ToString();
				const FString& DefaultToSelfMetaValue = InFunction->GetMetaData(FBlueprintMetadata::MD_DefaultToSelf);
				const FString& WorldContextMetaValue = InFunction->GetMetaData(FBlueprintMetadata::MD_WorldContext);
				bool bIsSelfPin = ((PinNameStr == DefaultToSelfMetaValue) || (PinNameStr == WorldContextMetaValue));

				if (!bShowWorldContextPin || !bIsSelfPin)
				{
					Pin->bHidden = true;
					Pin->bNotConnectable = InternalPins.Contains(Pin->PinName);
				}
			}
		}

		bAllPinsGood = bAllPinsGood && bPinGood;
	}

	// Since we're not a CallFunction node, WorldContext pins get asset pickers. Remove it!
	if (const FString* WorldContext = InFunction->FindMetaData(FBlueprintMetadata::MD_WorldContext))
	{
		if (UEdGraphPin* Pin = FindPin(FName(*WorldContext), EGPD_Input))
			Pin->bDefaultValueIsIgnored = true;
	}
}

UFunction* UK2Node_NeatConstructor::GetTargetFunction() const
{
	if (!FBlueprintCompilationManager::IsGeneratedClassLayoutReady())
	{
		if (UFunction* SkeletonFn = GetTargetFunctionFromSkeletonClass())
		{
			return SkeletonFn;
		}
	}

	UFunction* Function = FunctionReference.ResolveMember<UFunction>(GetBlueprintClassFromNode());
	return Function;
}

UFunction* UK2Node_NeatConstructor::GetTargetFunctionFromSkeletonClass() const
{
	UFunction* TargetFunction = nullptr;
	const UClass* ParentClass = FunctionReference.GetMemberParentClass(GetBlueprintClassFromNode());
	const UBlueprint* OwningBP = ParentClass ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy) : nullptr;
	if (const UClass* SkeletonClass = OwningBP ? OwningBP->SkeletonGeneratedClass : nullptr)
	{
		TargetFunction = SkeletonClass->FindFunctionByName(FunctionReference.GetMemberName());
	}
	return TargetFunction;
}

UFunction* UK2Node_NeatConstructor::GetFinishFunction() const
{
	const UFunction* TargetFunc = GetTargetFunction();
	if (!TargetFunc)
		return nullptr;
	
	const FString* FinishFuncName = TargetFunc->FindMetaData(NeatConstructorFinishMetadataName);
	if (UFunction* FinishFunc = FinishFuncName ? TargetFunc->GetOwnerClass()->FindFunctionByName(FName(*FinishFuncName)) : nullptr)
		return FinishFunc;

	const FClassProperty* TargetFuncClass = CastField<FClassProperty>(TargetFunc->FindPropertyByName(NAME_Class));
	if (!TargetFuncClass)
		return nullptr;

	if (TargetFuncClass->MetaClass->IsChildOf<AActor>())
		return UNeatFunctionsStatics::StaticClass()->FindFunctionByName(GET_MEMBER_NAME_CHECKED(UNeatFunctionsStatics, DefaultFinishSpawningActor));
	return nullptr;
}

FName UK2Node_NeatConstructor::GetFinishFunctionObjectInputName() const
{
	const UClass* PinBase = GetClassPinBaseClass();
	if (const UFunction* Fn = GetFinishFunction())
	{
		const FObjectProperty* Prop = GetFinishFunctionObjectProperty(*Fn, *PinBase);
		return Prop ? Prop->GetFName() : NAME_None;
	}

	return NAME_None;
}
