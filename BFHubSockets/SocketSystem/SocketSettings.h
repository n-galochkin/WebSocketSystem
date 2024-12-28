// 

#pragma once

#include "CoreMinimal.h"
#include "SocketSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta = (DisplayName = "Socket"))
class BFHUBSOCKETS_API USocketSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(GlobalConfig, EditAnywhere, BlueprintReadOnly, Category = "Reconnect")
	bool bAutoReconnectEnabled = true;

	UPROPERTY(GlobalConfig, EditAnywhere, BlueprintReadOnly, Category = "Reconnect")
	float ReconnectTimeStartInterval = 5.0f;

	UPROPERTY(GlobalConfig, EditAnywhere, BlueprintReadOnly, Category = "Reconnect")
	float ReconnectTimeMaxInterval = 60.0f;

	UPROPERTY(GlobalConfig, EditAnywhere, BlueprintReadOnly, Category = "Reconnect")
	float ReconnectTimeIncreaseStep = 2.0f;

	/** Try do not use this way, get actual hub response instead
	* Fake response useful when hub is not ready and we need to test some logic
	* But this is dangerous way because we need to keep quality for two version of data  */
	UPROPERTY(GlobalConfig, EditAnywhere, BlueprintReadOnly, Category = "Develop")
	bool bUseFakeResponse = false;
};
