#pragma once

#include "CoreMinimal.h"
#include "BFHubService_Base.h"

#include "BFHubService_Ping.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(BFHubService_Ping, Log, All);

USTRUCT()
struct FBFHubRequestData_Ping
{
	GENERATED_BODY()

	UPROPERTY()
	int64 Nonce = 0;
};

USTRUCT()
struct FBFHubResponseData_Ping
{
	GENERATED_BODY()

	UPROPERTY()
	int64 Nonce = 0;

	UPROPERTY()
	int64 TimeStamp = 0;
};

/**
 * This service need for ping hub server because socket can close connection if we not send any messages
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPingReceived, float, latency);

UCLASS()
class BFHUBSOCKETS_API UBFHubService_Ping : public UBFHubService_Base
{
	GENERATED_BODY()

public:
	float GetLastPingTime() const { return LastPingTime; }

	virtual void Init() override;
	virtual void Start() override;
	virtual void Stop() override;

	UPROPERTY()
	FPingReceived OnPingReceived;

private:
	void StartTimer();
	void StopTimer();

	UFUNCTION()
	void SendPing() const;

	UFUNCTION()
	void OnAnyMessageSent();

	UFUNCTION()
	void OnResponse(const FBFHubResponseData_Ping& Data);

	static int64 GetCurrentTimestampInMS();

private:
	float PingInterval = 28.0f;

	FTimerHandle PingTimerHandle;

	int32 LastPingTime = 0;
	int32 HighestPingTime = 0;
};
