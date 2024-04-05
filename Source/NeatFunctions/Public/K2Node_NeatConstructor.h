// Copyright Viktor Pramberg. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "K2Node_NeatConstructor.generated.h"

/**
 * 
 */
UCLASS()
class NEATFUNCTIONS_API UK2Node_NeatConstructor : public UK2Node_ConstructObjectFromClass
{
	GENERATED_BODY()

public:
	static inline FLazyName NeatConstructorMetadataName {"NeatConstructor" };
	static inline FLazyName NeatConstructorFinishMetadataName {"NeatConstructorFinish" };
	
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void AllocateDefaultPins() override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

	virtual UClass* GetClassPinBaseClass() const override;
	
	virtual void CreatePinsForClass(UClass* InClass, TArray<UEdGraphPin*>* OutClassPins) override;
	
	virtual bool IsSpawnVarPin(UEdGraphPin* Pin) const override;

	virtual FText GetBaseNodeTitle() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTitleFormat() const override;

	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const override;

	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	
	virtual void EarlyValidation(FCompilerResultsLog& MessageLog) const override;

	void CreatePinsForFunction(const UFunction* InFunction);
	
	UFunction* GetTargetFunction() const;
	UFunction* GetTargetFunctionFromSkeletonClass() const;
	
	UFunction* GetFinishFunction() const;
	FName GetFinishFunctionObjectInputName() const;

	UPROPERTY()
	FMemberReference FunctionReference;
};
