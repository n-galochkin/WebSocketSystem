#pragma once

#include "CoreMinimal.h"
#include "BFHubSockets/Services/BFHubService_Base.h"

#include "BFHubService_ServerInit.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(BFHubService_ServerInit, Log, All);

DECLARE_MULTICAST_DELEGATE(FInitilizedDelegate)

USTRUCT()
struct FBFHubRequestData_ServerInit
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString Password;

	UPROPERTY()
	FString Version;
	
	UPROPERTY()
	FString regionId;
};


USTRUCT()
struct FBFHubResponseData_ServerInit
{
	GENERATED_BODY()

	UPROPERTY()
	FString MatchId;
};


template <>
inline FString GetFakeResponseData<FBFHubRequestData_ServerInit>(const FHubServiceAction& Action)
{
	const FBFHubResponseData_ServerInit Data{
		"123"
	};

	FString JsonString;
	FJsonObjectConverter::UStructToJsonObjectString(Data, JsonString);

	return JsonString;
}

UCLASS()
class BFHUBSOCKETS_API UBFHubService_ServerInit : public UBFHubService_Base
{
	GENERATED_BODY()

public:
	void SendInit();

	FString GetMatchId() const { return MatchId; }

	// Initialization is a process of first authorization on the hub
	void SubscribeOnInitialized(const FInitilizedDelegate::FDelegate& Delegate);

	// Authorization calls every time when we authorize on the hub (every reconnect)
	void SubscribeOnAuthorized(const FInitilizedDelegate::FDelegate& Delegate);

	void SetDevCredentials();
	void SetCredentials(const FString& InServerName, const FString& InServerPassword, const FString& InServerRegionId);
	bool IsCredentialsSet() const { return ServerName.Len() > 0 && ServerPassword.Len() > 0; }

protected:
	virtual void Init() override;
	virtual void Reauthorize() override;

private:
	UFUNCTION()
	void OnResponse(const FBFHubResponseData_ServerInit& Data);

	FString MatchId = TEXT("DefaultMatchId");
	FString ServerName;
	FString ServerPassword;
	FString ServerRegionId;

	bool bIsInitialized = false;
	bool bIsAuthorized = false;

	FInitilizedDelegate OnInitialized;
	FInitilizedDelegate OnAuthorized;
};
