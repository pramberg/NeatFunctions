// Copyright Viktor Pramberg. All Rights Reserved.


#include "K2Node_NeatConstructor.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintCompilationManager.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_EnumLiteral.h"
#include "K2Node_IfThenElse.h"
#include "KismetCompiler.h"
#include "NeatFunctionsStyle.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "NeatFunctionsRuntime/Public/NeatFunctionsStatics.h"
#include "Styling/SlateIconFinder.h"

void UK2Node_NeatConstructor::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* NodeClass = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(NodeClass))
	{
		for (const UFunction* Fn : TObjectRange<UFunction>())
		{
			const bool bNeatSpawnActorFunction = Fn ? Fn->HasMetaData(NeatConstructorMetadataName) || Fn->HasMetaData(NeatConstructorFinishMetadataName) : false;
			if (!bNeatSpawnActorFunction)
				continue;

			UBlueprintFieldNodeSpawner* NodeSpawner = UBlueprintFieldNodeSpawner::Create(NodeClass, Fn);
			check(NodeSpawner != nullptr);

			NodeSpawner->DefaultMenuSignature.MenuName = UK2Node_CallFunction::GetUserFacingFunctionName(Fn);
			NodeSpawner->DefaultMenuSignature.Category = UK2Node_CallFunction::GetDefaultCategoryForFunction(Fn, FText::GetEmpty());
			NodeSpawner->DefaultMenuSignature.Tooltip = FText::FromString(UK2Node_CallFunction::GetDefaultTooltipForFunction(Fn));
			NodeSpawner->DefaultMenuSignature.Keywords = UK2Node_CallFunction::GetKeywordsForFunction(Fn);

			NodeSpawner->DefaultMenuSignature.Icon = FSlateIcon(FNeatFunctionsStyle::Get().GetStyleSetName(), "NeatFunctions.FunctionIcon");
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

	FCreatePinParams Params;
	Params.Index = GetPinIndex(GetThenPin()) + 1;
	UEdGraphPin* ThenIsNotValid = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Else, Params);
	ThenIsNotValid->PinFriendlyName = INVTEXT("Not Valid");
	ThenIsNotValid->PinToolTip = TEXT("Executed when there wasn't an object spawned successfully.");
	GetThenPin()->PinFriendlyName = INVTEXT("Valid");
	GetThenPin()->PinToolTip = TEXT("Executed when we spawned an object successfully.");

	CreatePinsForFunction(GetTargetFunction());
	CreatePinsForFunction(GetFinishFunction());
}

namespace
{
	// This is an exact copy of FKismetCompilerUtilities::GenerateAssignmentNodes, except that it has a UK2Node* Input instead of UK2Node_CallFunction*.
	// It's really unfortunate, but necessary, since we have to insert an if node between our CallFunction node and our assignment nodes.
	UEdGraphPin* GenerateAssignmentNodes(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph,
	                                     UK2Node* CallBeginSpawnNode, UEdGraphNode* SpawnNode,
	                                     UEdGraphPin* CallBeginResult, const UClass* ForClass)
	{
		static const FName ObjectParamName(TEXT("Object"));
		static const FName ValueParamName(TEXT("Value"));
		static const FName PropertyNameParamName(TEXT("PropertyName"));

		const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
		UEdGraphPin* LastThen = CallBeginSpawnNode->GetThenPin();

		// Create 'set var by name' nodes and hook them up
		for (int32 PinIdx = 0; PinIdx < SpawnNode->Pins.Num(); PinIdx++)
		{
			// Only create 'set param by name' node if this pin is linked to something
			UEdGraphPin* OrgPin = SpawnNode->Pins[PinIdx];
			const bool bHasDefaultValue = !OrgPin->DefaultValue.IsEmpty() || !OrgPin->DefaultTextValue.IsEmpty() ||
				OrgPin->DefaultObject;
			if (NULL == CallBeginSpawnNode->FindPin(OrgPin->PinName) &&
				(OrgPin->LinkedTo.Num() > 0 || bHasDefaultValue))
			{
				FProperty* Property = FindFProperty<FProperty>(ForClass, OrgPin->PinName);
				// NULL property indicates that this pin was part of the original node, not the 
				// class we're assigning to:
				if (!Property)
				{
					continue;
				}

				if (OrgPin->LinkedTo.Num() == 0)
				{
					// We don't want to generate an assignment node unless the default value 
					// differs from the value in the CDO:
					FString DefaultValueAsString;

					if (FBlueprintCompilationManager::GetDefaultValue(ForClass, Property, DefaultValueAsString))
					{
						if (Schema->DoesDefaultValueMatch(*OrgPin, DefaultValueAsString))
						{
							continue;
						}
					}
					else if (ForClass->ClassDefaultObject)
					{
						FBlueprintEditorUtils::PropertyValueToString(Property, (uint8*)ForClass->ClassDefaultObject, DefaultValueAsString);

						if (DefaultValueAsString == OrgPin->GetDefaultAsString())
						{
							continue;
						}
					}
				}

				const FString& SetFunctionName = Property->GetMetaData(FBlueprintMetadata::MD_PropertySetFunction);
				if (!SetFunctionName.IsEmpty())
				{
					UClass* OwnerClass = Property->GetOwnerClass();
					UFunction* SetFunction = OwnerClass->FindFunctionByName(*SetFunctionName);
					check(SetFunction);

					UK2Node_CallFunction* CallFuncNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(SpawnNode, SourceGraph);
					CallFuncNode->SetFromFunction(SetFunction);
					CallFuncNode->AllocateDefaultPins();

					// Connect this node into the exec chain
					Schema->TryCreateConnection(LastThen, CallFuncNode->GetExecPin());
					LastThen = CallFuncNode->GetThenPin();

					// Connect the new object to the 'object' pin
					UEdGraphPin* ObjectPin = Schema->FindSelfPin(*CallFuncNode, EGPD_Input);
					CallBeginResult->MakeLinkTo(ObjectPin);

					// Move Value pin connections
					UEdGraphPin* SetFunctionValuePin = nullptr;
					for (UEdGraphPin* CallFuncPin : CallFuncNode->Pins)
					{
						if (!Schema->IsMetaPin(*CallFuncPin))
						{
							check(CallFuncPin->Direction == EGPD_Input);
							SetFunctionValuePin = CallFuncPin;
							break;
						}
					}
					check(SetFunctionValuePin);

					CompilerContext.MovePinLinksToIntermediate(*OrgPin, *SetFunctionValuePin);
				}
				else if (UFunction* SetByNameFunction = Schema->FindSetVariableByNameFunction(OrgPin->PinType))
				{
					UK2Node_CallFunction* SetVarNode = nullptr;
					if (OrgPin->PinType.IsArray())
					{
						SetVarNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallArrayFunction>(SpawnNode, SourceGraph);
					}
					else
					{
						SetVarNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(SpawnNode, SourceGraph);
					}
					SetVarNode->SetFromFunction(SetByNameFunction);
					SetVarNode->AllocateDefaultPins();

					// Connect this node into the exec chain
					Schema->TryCreateConnection(LastThen, SetVarNode->GetExecPin());
					LastThen = SetVarNode->GetThenPin();

					// Connect the new object to the 'object' pin
					UEdGraphPin* ObjectPin = SetVarNode->FindPinChecked(ObjectParamName);
					CallBeginResult->MakeLinkTo(ObjectPin);

					// Fill in literal for 'property name' pin - name of pin is property name
					UEdGraphPin* PropertyNamePin = SetVarNode->FindPinChecked(PropertyNameParamName);
					PropertyNamePin->DefaultValue = OrgPin->PinName.ToString();

					UEdGraphPin* ValuePin = SetVarNode->FindPinChecked(ValueParamName);
					if (OrgPin->LinkedTo.Num() == 0 &&
						OrgPin->DefaultValue != FString() &&
						OrgPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Byte &&
						OrgPin->PinType.PinSubCategoryObject.IsValid() &&
						OrgPin->PinType.PinSubCategoryObject->IsA<UEnum>())
					{
						// Pin is an enum, we need to alias the enum value to an int:
						UK2Node_EnumLiteral* EnumLiteralNode = CompilerContext.SpawnIntermediateNode<UK2Node_EnumLiteral>(SpawnNode, SourceGraph);
						EnumLiteralNode->Enum = CastChecked<UEnum>(OrgPin->PinType.PinSubCategoryObject.Get());
						EnumLiteralNode->AllocateDefaultPins();
						EnumLiteralNode->FindPinChecked(UEdGraphSchema_K2::PN_ReturnValue)->MakeLinkTo(ValuePin);

						UEdGraphPin* InPin = EnumLiteralNode->FindPinChecked(UK2Node_EnumLiteral::GetEnumInputPinName());
						check(InPin);
						InPin->DefaultValue = OrgPin->DefaultValue;
					}
					else
					{
						// For non-array struct pins that are not linked, transfer the pin type so that the node will expand an auto-ref that will assign the value by-ref.
						if (OrgPin->PinType.IsArray() == false && OrgPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && OrgPin->LinkedTo.Num() == 0)
						{
							ValuePin->PinType.PinCategory = OrgPin->PinType.PinCategory;
							ValuePin->PinType.PinSubCategory = OrgPin->PinType.PinSubCategory;
							ValuePin->PinType.PinSubCategoryObject = OrgPin->PinType.PinSubCategoryObject;
							CompilerContext.MovePinLinksToIntermediate(*OrgPin, *ValuePin);
						}
						else
						{
							// For interface pins we need to copy over the subcategory
							if (OrgPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Interface)
							{
								ValuePin->PinType.PinSubCategoryObject = OrgPin->PinType.PinSubCategoryObject;
							}

							CompilerContext.MovePinLinksToIntermediate(*OrgPin, *ValuePin);
							SetVarNode->PinConnectionListChanged(ValuePin);
						}
					}
				}
			}
		}

		return LastThen;
	}
}

void UK2Node_NeatConstructor::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UK2Node_CallFunction* BeginSpawnFunc = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	BeginSpawnFunc->SetFromFunction(GetTargetFunction());
	BeginSpawnFunc->AllocateDefaultPins();

	BeginSpawnFunc->GetReturnValuePin()->PinType = GetResultPin()->PinType;
	BeginSpawnFunc->PinTypeChanged(BeginSpawnFunc->GetReturnValuePin());

	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *BeginSpawnFunc->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*GetClassPin(), *BeginSpawnFunc->FindPin(FName("Class")));
	CompilerContext.MovePinLinksToIntermediate(*GetResultPin(), *BeginSpawnFunc->GetReturnValuePin());

	for (UEdGraphPin* CurrentPin : Pins)
	{
		if (CurrentPin && CurrentPin->Direction == EGPD_Input && CurrentPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
		{
			if (UEdGraphPin* DestPin = BeginSpawnFunc->FindPin(CurrentPin->PinName))
			{
				CompilerContext.CopyPinLinksToIntermediate(*CurrentPin, *DestPin);
			}
		}
	}

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

	Schema->TryCreateConnection(BeginSpawnFunc->GetThenPin(), IfElseNode->GetExecPin());

	CompilerContext.CopyPinLinksToIntermediate(*FindPin(UEdGraphSchema_K2::PN_Else), *IfElseNode->GetElsePin());

	UK2Node_CallFunction* FinishSpawnFunc = nullptr;
	if (GetFinishFunction())
	{
		FinishSpawnFunc = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		FinishSpawnFunc->SetFromFunction(GetFinishFunction());
		FinishSpawnFunc->AllocateDefaultPins();

		CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *FinishSpawnFunc->GetThenPin());

		for (UEdGraphPin* CurrentPin : Pins)
		{
			if (CurrentPin && CurrentPin->Direction == EGPD_Input && CurrentPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				if (UEdGraphPin* DestPin = FinishSpawnFunc->FindPin(CurrentPin->PinName))
				{
					CompilerContext.CopyPinLinksToIntermediate(*CurrentPin, *DestPin);
				}
			}
		}

		const FName InputName = GetFinishFunctionObjectInputName();
		BeginSpawnFunc->GetReturnValuePin()->MakeLinkTo(FinishSpawnFunc->FindPin(InputName));
	}

	UEdGraphPin* LastThen = GenerateAssignmentNodes(CompilerContext, SourceGraph, IfElseNode, this, BeginSpawnFunc->GetReturnValuePin(), GetClassToSpawn());

	if (FinishSpawnFunc)
	{
		LastThen->MakeLinkTo(FinishSpawnFunc->GetExecPin());
		LastThen = FinishSpawnFunc->GetThenPin();
	}

	CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *LastThen);
}

UClass* UK2Node_NeatConstructor::GetClassPinBaseClass() const
{
	if (UFunction* Fn = GetTargetFunction())
	{
		const FClassProperty* ClassProp = CastField<FClassProperty>(Fn->FindPropertyByName("Class"));
		if (!ClassProp)
			return nullptr;

		return ClassProp->MetaClass;
	}

	return AActor::StaticClass();
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
	if (UFunction* Fn = GetTargetFunction())
	{
		return Fn->GetDisplayNameText();
	}
	return Super::GetBaseNodeTitle();
}

FText UK2Node_NeatConstructor::GetDefaultNodeTitle() const
{
	if (UFunction* Fn = GetTargetFunction())
	{
		return FText::Format(INVTEXT("{0}\nNo Class Selected"), Fn->GetDisplayNameText());
	}
	return Super::GetDefaultNodeTitle();
}

FText UK2Node_NeatConstructor::GetNodeTitleFormat() const
{
	if (UFunction* Fn = GetTargetFunction())
	{
		return FText::Format(INVTEXT("{0}\n{ClassName}"), Fn->GetDisplayNameText());
	}
	return Super::GetNodeTitleFormat();
}

FText UK2Node_NeatConstructor::GetTooltipText() const
{
	return FText::FromString(UK2Node_CallFunction::GetDefaultTooltipForFunction(GetTargetFunction()));
}

FSlateIcon UK2Node_NeatConstructor::GetIconAndTint(FLinearColor& OutColor) const
{
	return FSlateIconFinder::FindIconForClass(GetClassPinBaseClass());
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

	// Since we're not a CallFunction node, WorldContext pins get asset pickers. 
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
		// first look in the skeleton class:
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
	UClass* ParentClass = FunctionReference.GetMemberParentClass(GetBlueprintClassFromNode());
	UBlueprint* OwningBP = ParentClass ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy) : nullptr;
	if (UClass* SkeletonClass = OwningBP ? OwningBP->SkeletonGeneratedClass : nullptr)
	{
		TargetFunction = SkeletonClass->FindFunctionByName(FunctionReference.GetMemberName());
	}
	return TargetFunction;
}

UFunction* UK2Node_NeatConstructor::GetFinishFunction() const
{
	const UFunction* TargetFunc = GetTargetFunction();
	const FString* FinishFuncName = TargetFunc ? TargetFunc->FindMetaData(NeatConstructorFinishMetadataName) : nullptr;
	if (UFunction* FinishFunc = FinishFuncName ? TargetFunc->GetOwnerClass()->FindFunctionByName(FName(*FinishFuncName)) : nullptr)
		return FinishFunc;

	const FClassProperty* TargetFuncClass = CastField<FClassProperty>(TargetFunc->FindPropertyByName("Class"));
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
		for (TFieldIterator<FProperty> PropIt(Fn); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			FProperty* Param = *PropIt;
			const bool bIsFunctionInput = !Param->HasAnyPropertyFlags(CPF_ReturnParm) && (!Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm));
			if (!bIsFunctionInput)
				continue;

			const FObjectProperty* ObjectProp = CastField<FObjectProperty>(Param);
			if (!ObjectProp)
				continue;

			if (PinBase->IsChildOf(ObjectProp->PropertyClass))
			{
				return ObjectProp->GetFName();
			}
		}
	}

	return NAME_None;
}
