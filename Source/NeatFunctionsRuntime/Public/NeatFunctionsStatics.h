// Copyright Viktor Pramberg. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NeatFunctionsStatics.generated.h"

/**
 * 
 */
UCLASS()
class NEATFUNCTIONSRUNTIME_API UNeatFunctionsStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Function used internally when a developer does not specify a finish spawning actor function.
	// We don't use UGameplayStatics::FinishSpawningActor() to avoid having to deal with extra pins that aren't required.
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = true))
	static void DefaultFinishSpawningActor(AActor* Actor);
};
