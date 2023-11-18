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
	virtual void ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;

protected:
	UPROPERTY()
	bool bIsNeatFunction = false;
};
