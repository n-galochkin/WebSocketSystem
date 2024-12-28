#pragma once

#include "CoreMinimal.h"
#include "../SocketSystem/HubServicesBaseData.h"
#include "BFHubSockets/SocketSystem/HubSocketSystem.h"
#include "BFHubSockets/SocketSystem/ServiceLocator.h"

#include "BFHubService_Base.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBFHubService_Base, Log, All);

DECLARE_DELEGATE_OneParam(FCompleteActionDelegate, const bool)

USTRUCT(BlueprintType)
struct FHubServiceErrorNotificationMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FString ServiceName;

	UPROPERTY(BlueprintReadWrite)
	FHubErrorData ErrorData;
};

/**
 * Base class for hub services
 */
UCLASS(Abstract)
class BFHUBSOCKETS_API UBFHubService_Base : public UObject, public IBFHubService
{
	GENERATED_BODY()

public:
	FCallbackMessageHandle<FHubEmptyData>::FOnCallback& BindHandle(const FHubServiceAction& Key);

	template <typename T>
	void SendRequestToHub(const T& InStructure) const;

	template <typename T>
	void SendRequestToHub(const FHubServiceAction& Key, const T& InStructure) const;

	void SendRequestToHub(const FHubServiceAction& Key) const;

	template <typename T>
	typename FCallbackMessageHandle<T>::FOnCallback& GetBindedHandle();

	template <typename T>
	typename FCallbackMessageHandle<T>::FOnCallback& BindHandle(const FHubServiceAction& Key);

	FCallbackErrorHandle::FOnError& GetBindedErrorHandle() const;
	FCallbackErrorHandle::FOnError& GetBindedErrorHandle(const FHubServiceAction& Key) const;


	FCompleteActionDelegate CompleteActionDelegate;

protected:
	virtual void Init() override;
	virtual void Start() override;
	virtual void StartAuthorized() override;
	virtual void Reauthorize() override;
	virtual void Stop() override;

	UFUNCTION()
	virtual void OnErrorReceived(const FHubErrorData& ErrorData);
	virtual void NotifyError(const FHubErrorData& ErrorData);

	UPROPERTY()
	UHubSocketSystem* SocketSystem;

	// Key for binding to hub request/response
	FHubServiceAction BaseAction;

	// Name of the service for notifications
	FString ServiceReadableName;
};


template <typename T>
void UBFHubService_Base::SendRequestToHub(const T& InStructure) const
{
	SendRequestToHub(BaseAction, InStructure);
}

template <typename T>
void UBFHubService_Base::SendRequestToHub(const FHubServiceAction& Key, const T& InStructure) const
{
	SocketSystem->Send(Key, InStructure);
}


template <typename T>
typename FCallbackMessageHandle<T>::FOnCallback& UBFHubService_Base::GetBindedHandle()
{
	auto& Callback = SocketSystem->Bind<T>(BaseAction);
	GetBindedErrorHandle().AddUObject(this, &UBFHubService_Base::OnErrorReceived);
	return Callback;
}

template <typename T>
typename FCallbackMessageHandle<T>::FOnCallback& UBFHubService_Base::BindHandle(const FHubServiceAction& Key)
{
	auto& Callback = SocketSystem->Bind<T>(Key);
	GetBindedErrorHandle(Key).AddUObject(this, &UBFHubService_Base::OnErrorReceived);
	return Callback;
}
