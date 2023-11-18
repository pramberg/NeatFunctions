// Copyright Viktor Pramberg. All Rights Reserved.
#pragma once
#include "K2Node_CallFunction.h"
#include "K2Node_NeatCallFunction.generated.h"

/**
 * Call function node that can handle delegate event binding in the node itself, like an async node would.
 * Use "NeatDelegateFunction" as UFUNCTION metadata if you wish to use this node.
 *
 * Example:
 *
 * DECLARE_DYNAMIC_DELEGATE(FMyDelegate);
 *
 * UFUNCTION(BlueprintCallable, meta = (NeatDelegateFunction))
 * void MyFunction(FMyDelegate InDelegate)
 * {
 *		InDelegate.ExecuteIfBound();
 * }
 */
UCLASS()
class NEATFUNCTIONS_API UK2Node_NeatCallFunction : public UK2Node_CallFunction
{
	GENERATED_BODY()

public:
	static const FName DelegateFunctionMetadataName;
	
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void AllocateDefaultPins() override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;

protected:
	using FForEachDelegateFunction = TFunctionRef<void(const FDelegateProperty&)>;
	void ForEachEligableDelegateProperty(FForEachDelegateFunction InFn);
	
	// Destroys the connection between this node's Then pin and some other node's Exec pin. Those types of connections are generally created by the autowire
	// functionality, which we have no other way of intercepting unfortunately. This function is called when we have received a new connection between this
	// node and some other node, when the pin of this node is actually part of a delegate, and should therefore be replaced with that delegate's Then pin instead. 
	void QueueDestroyAutomaticExecConnection(TWeakObjectPtr<UEdGraphNode> OtherNode);
	
	UPROPERTY()
	bool bIsNeatFunction = false;
};
