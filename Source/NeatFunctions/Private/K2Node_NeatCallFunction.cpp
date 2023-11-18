// Copyright Viktor Pramberg. All Rights Reserved.
#include "K2Node_NeatCallFunction.h"
#include "NeatFunctionsStyle.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "KismetCompiler.h"

#include "K2Node_CustomEvent.h"
#include "SGraphPin.h"
#include "KismetNodes/SGraphNodeK2Default.h"
#include "Widgets/Colors/SSimpleGradient.h"

const FName UK2Node_NeatCallFunction::DelegateFunctionMetadataName("NeatDelegateFunction");

void UK2Node_NeatCallFunction::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* NodeClass = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(NodeClass))
	{
		for (const UFunction* Fn : TObjectRange<UFunction>())
		{
			const bool bNeatDelegateFunction = Fn ? Fn->HasMetaData(DelegateFunctionMetadataName) : false;
			if (!bNeatDelegateFunction)
				continue;
		
			UBlueprintFunctionNodeSpawner* NodeSpawner = UBlueprintFunctionNodeSpawner::Create(NodeClass, Fn);
			check(NodeSpawner != nullptr);
			
			NodeSpawner->DefaultMenuSignature.Icon = FSlateIcon(FNeatFunctionsStyle::Get().GetStyleSetName(), "NeatFunctions.FunctionIcon");
			NodeSpawner->DefaultMenuSignature.IconTint = FLinearColor::White;
			NodeSpawner->CustomizeNodeDelegate.BindLambda([Fn](UEdGraphNode* Node, bool)
			{
				ThisClass* ThisNode = Cast<ThisClass>(Node);
				ThisNode->SetFromFunction(Fn);
			});
			
			ActionRegistrar.AddBlueprintAction(NodeClass, NodeSpawner);
		}
	}
}

namespace
{
	bool IsDelegateEligable(const FDelegateProperty* InProp)
	{
		const bool bHasReturnValue = InProp->SignatureFunction->GetReturnProperty() != nullptr;

		bool bHasOutParam = false;
		for (TFieldIterator<FProperty> PropIt(InProp->SignatureFunction); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			const FProperty* Param = *PropIt;
			bHasOutParam &= Param->HasAnyPropertyFlags(CPF_OutParm);
		}
		
		return !bHasReturnValue && !bHasOutParam;
	}
}

void UK2Node_NeatCallFunction::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	// This should always be called. Otherwise it might have an invalid cached value (if copying from event graph to function graph, for example)
	bIsNeatFunction = GetDefault<UK2Node_CustomEvent>()->IsCompatibleWithGraph(GetGraph());
	if (!bIsNeatFunction)
		return;
	
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	
	for (const FDelegateProperty* Prop : TFieldRange<FDelegateProperty>(GetTargetFunction()))
	{
		if (!IsDelegateEligable(Prop))
			continue;
		
		if (UEdGraphPin* DelPin = FindPin(Prop->GetFName()))
			RemovePin(DelPin);
		
		UEdGraphPin* ExecPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, Prop->GetFName());
		ExecPin->PinToolTip = Prop->GetToolTipText().ToString();
		ExecPin->PinFriendlyName = Prop->GetDisplayNameText();
		
		for (TFieldIterator<FProperty> PropIt(Prop->SignatureFunction); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			const FProperty* Param = *PropIt;
			const bool bIsFunctionInput = !Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm);
			if (bIsFunctionInput)
			{
				UEdGraphPin* Pin = CreatePin(EGPD_Output, NAME_None, FName(FString::Printf(TEXT("%s_%s"), *Prop->GetName(), *Param->GetName())));
				K2Schema->ConvertPropertyToPinType(Param, /*out*/ Pin->PinType);
				Pin->PinFriendlyName = Param->GetDisplayNameText();

				GeneratePinTooltipFromFunction(*Pin, Prop->SignatureFunction);
			}
		}
	}
}

void UK2Node_NeatCallFunction::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	if (!bIsNeatFunction)
		return;
	
	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	
	UK2Node_CallFunction* CallFunc = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallFunc->SetFromFunction(GetTargetFunction());
	CallFunc->AllocateDefaultPins();

	bool bIsValid = true;
	// Move all pins except for delegates we've created.
	for (UEdGraphPin* Pin : CallFunc->Pins)
	{
		if (UEdGraphPin* PinOnSelf = FindPin(Pin->GetFName()))
		{
			// We have created exec pins with the same name as the delegate property on the CallFunction. Skip those, we handle those in the next step.
			if (Pin->PinType == PinOnSelf->PinType && Pin->Direction == PinOnSelf->Direction)
				bIsValid &= CompilerContext.MovePinLinksToIntermediate(*PinOnSelf, *Pin).CanSafeConnect();
		}
	}

	for (const FDelegateProperty* Prop : TFieldRange<FDelegateProperty>(GetTargetFunction()))
	{
		if (!IsDelegateEligable(Prop))
			continue;
		
		const UK2Node_CustomEvent* EventNode = UK2Node_CustomEvent::CreateFromFunction(FVector2D::ZeroVector, SourceGraph, FString::Printf(TEXT("%s_%s"), *Prop->GetName(), *CompilerContext.GetGuid(this)), Prop->SignatureFunction);

		UEdGraphPin* DelegatePin = EventNode->FindPin(UK2Node_Event::DelegateOutputName);
		UEdGraphPin* DelegateInputPin = CallFunc->FindPin(Prop->GetFName(), EGPD_Input);
		Schema->TryCreateConnection(DelegatePin, DelegateInputPin);

		UEdGraphPin* EventThenPin = EventNode->GetThenPin();
		UEdGraphPin* ThenPinForCurrentDelegate = FindPin(Prop->GetFName());
		bIsValid &= CompilerContext.MovePinLinksToIntermediate(*ThenPinForCurrentDelegate, *EventThenPin).CanSafeConnect();

		for (TFieldIterator<FProperty> PropIt(Prop->SignatureFunction); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			const FProperty* Param = *PropIt;
			const bool bIsFunctionInput = !Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm);
			if (bIsFunctionInput)
			{
				bIsValid &= CompilerContext.MovePinLinksToIntermediate(*FindPin(FName(FString::Printf(TEXT("%s_%s"), *Prop->GetName(), *Param->GetName()))), *EventNode->FindPin(Param->GetFName())).CanSafeConnect();
			}
		}
	}

	BreakAllNodeLinks();
}

void UK2Node_NeatCallFunction::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);
}

FSlateIcon UK2Node_NeatCallFunction::GetIconAndTint(FLinearColor& OutColor) const
{
	return FSlateIcon(FNeatFunctionsStyle::Get().GetStyleSetName(), "NeatFunctions.FunctionIcon");
}

class SNeatCallFunctionNode : public SGraphNodeK2Default
{
public:
	SLATE_BEGIN_ARGS(SNeatCallFunctionNode){}
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, UK2Node* InNode)
	{
		this->GraphNode = InNode;
		this->UpdateGraphNode();
	}

	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override
	{
		// Add separator on top of each delegate, to make it very clear what pins belong to what context.
		const UK2Node_NeatCallFunction* NodeAsFn = Cast<UK2Node_NeatCallFunction>(GetNodeObj());
		const UEdGraphPin* PinObj = PinToAdd->GetPinObj();
		
		const UFunction* Fn = NodeAsFn ? NodeAsFn->GetTargetFunction() : nullptr;
		const FProperty* Property = Fn ? Fn->FindPropertyByName(PinObj->PinName) : nullptr;
		const bool bIsDelegatePin = Property && Property->IsA<FDelegateProperty>();
		if (PinToAdd->GetDirection() == EGPD_Output && PinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && bIsDelegatePin)
		{
			RightNodeBox->AddSlot().AutoHeight().Padding(5.0f)
			[
				SNew(SBox).MinDesiredHeight(2.0f)
				[
					SNew(SSimpleGradient)
					.StartColor(FLinearColor::Transparent)
					.EndColor(FLinearColor::White)
				]
			];
		}
		
		SGraphNodeK2Default::AddPin(PinToAdd);
	}
};

TSharedPtr<SGraphNode> UK2Node_NeatCallFunction::CreateVisualWidget()
{
	return SNew(SNeatCallFunctionNode, this);
}
