#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BaseParameters.generated.h"


USTRUCT(BlueprintType)
struct FBaseSocketResponse
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sockets|Data")
	FString Message;

};

USTRUCT(BlueprintType)
struct FEmptyServiceResponse
{
	GENERATED_BODY()

};

