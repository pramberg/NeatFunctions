// Copyright Viktor Pramberg. All Rights Reserved.


#include "NeatFunctionsStatics.h"

void UNeatFunctionsStatics::DefaultFinishSpawningActor(AActor* Actor)
{
	if (Actor)
	{
		Actor->FinishSpawning(Actor->GetTransform(), true);
	}
}
