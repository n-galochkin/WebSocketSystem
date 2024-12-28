// 

#pragma once

#include "CoreMinimal.h"
#include "JsonObjectConverter.h"
#include "MessageHandle.h"
#include "ServiceLocator.h"
#include "HubServicesBaseData.h"
#include "SocketSettings.h"
#include "HelpersPlugin/Helpers/GetHelpers.h"
#include "HubSocketSystem.generated.h"

class IWebSocket;

DECLARE_LOG_CATEGORY_EXTERN(BFHubSocketSystem, Log, All);

DECLARE_MULTICAST_DELEGATE(FMessageSentDelegate);

UENUM(BlueprintType)
enum class EBFSocketConnectionState : uint8
{
	None,
	Created,
	Connecting,
	Connected,
	Established,
	Authorized,
	Closed,
	WaitingForReconnect,
};

/**
 * Socket system send and receive messages to hub server, delegate data structs to services and handle connection
 */
UCLASS()
class BFHUBSOCKETS_API UHubSocketSystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Hub Socket Error")
	static FString GetErrorString(EHubSocketErrorCodes ErrorCode);

	UFUNCTION(BlueprintPure, Category = "Hub Socket Error")
	static FString GetErrorStringFrom(int32 ErrorCodeAsInt);

	void StartConnectionCmdlineUrl();
	void StartConnectionSettingsUrl();
	void StartConnection(const FString& Url);

	template <typename T>
	void Send(const FHubServiceAction& Key, const T& InStructure);

	template <typename TStruct>
	typename FCallbackMessageHandle<TStruct>::FOnCallback& Bind(const FHubServiceAction& Key);

	FCallbackErrorHandle::FOnError& BindError(const FHubServiceAction& Key);

	void Unbind(const FHubServiceAction& Key);

	// Register or Get service by type
	template <typename T>
	static T* GetService(const UObject* WorldContextObject);

	template <typename T>
	T* GetService();

	// Deprecated! use GetService for registration and getting services
	UServiceLocator* GetServicesLocator();


	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConnectionStateChanged, EBFSocketConnectionState, State);

	UPROPERTY(BlueprintAssignable)
	FOnConnectionStateChanged OnConnectionStateChanged;

	UFUNCTION(BlueprintCallable)
	EBFSocketConnectionState GetConnectionState() const
	{
		return ConnectionState;
	}

private:
	TSharedPtr<IWebSocket> Socket;
	FString ConnectionURL;

	UFUNCTION()
	void SetConnectionState(EBFSocketConnectionState State);

	EBFSocketConnectionState ConnectionState;

	UPROPERTY()
	UServiceLocator* Services;
	TMap<FHubServiceAction, TSharedPtr<FBaseMessageHandle>> Handlers;

	// will be increase if reconnect fail
	float CurrentReconnectTimeInterval = GetDefault<USocketSettings>()->ReconnectTimeStartInterval;
	FTimerHandle ReconnectTimerHandle;

	void EnqueueMessage(const FHubServiceAction& Key, const FString& InRawMessage);
	TQueue<FString> QueuedNonAuthMessages;
	TQueue<FString> QueuedMessages;
	void TrySendQueuedMessages();
	void DequeueMessages(TQueue<FString>& Queue) const;
	bool IsConnected() const;

	FMessageSentDelegate MessageSentDelegate;

public:
	void SubscribeToMessageSentEvent(const FMessageSentDelegate::FDelegate& Delegate)
	{
		MessageSentDelegate.Add(Delegate);
	}

private:
	void CreateServicesLocator();
	void CreateSocket();
	void Connect();
	void StartReconnectTimer();
	void StopReconnectTimer();
	void StopCommunication();

	bool TrySend(const FString& InRawString) const;

	bool TryConvertMessageToHeader(const FString& MessageString, FHubResponseMessageHeader& Header) const;
	void HandleMessageData(const FHubResponseMessageHeader& Header);

	UFUNCTION()
	void OnConnected();

	UFUNCTION()
	void OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean);

	UFUNCTION()
	void OnConnectionError(const FString& ErrorString);

	UFUNCTION()
	void OnMessageSent(const FString& MessageString);

	UFUNCTION()
	void OnMessage(const FString& MessageString);

	UFUNCTION()
	void OnAuthorized();

	void LogVerbose(const FString& Message);
	void LogWarning(const FString& Message);
	void LogError(const FString& Message);
};

template <typename T>
T* UHubSocketSystem::GetService(const UObject* WorldContextObject)
{
	if (const auto Subsystem = GetGISubsystem<UHubSocketSystem>(WorldContextObject))
	{
		return Subsystem->GetService<T>();
	}

	return nullptr;
}

template <typename T>
T* UHubSocketSystem::GetService()
{
	return GetServicesLocator()->RegisterService<T>();
}

template <typename T>
void UHubSocketSystem::Send(const FHubServiceAction& Key, const T& Data)
{
	// Workaround for linker error because we cant use LogCategory which defined in cpp from main game module 
	LogVerbose(FString::Printf(TEXT("Sending request with method \"%s\""), *Key.Method));

	FHubRequestMessageHeader Message;
	//we can't send controller as enum name like "EVENT" because hub expect int value
	Message.Controller = static_cast<int32>(Key.Controller);
	Message.Method = Key.Method;

	if (FJsonObjectConverter::UStructToJsonObjectString(Data, Message.Data, 0, 0, 0, nullptr, false) == false)
	{
		LogError(FString::Printf(TEXT("Failed to setup data for method \"%s\""), *Key.Method));
		return;
	}

	FString StringToSend;
	if (FJsonObjectConverter::UStructToJsonObjectString(Message, StringToSend, 0, 0, 0, nullptr, false) == false)
	{
		LogError(FString::Printf(TEXT("Failed to setup header for method \"%s\""), *Key.Method));
		return;
	}

#if WITH_EDITOR
	if (GetDefault<USocketSettings>()->bUseFakeResponse)
	{
		const FString FakeDataString = GetFakeResponseData<T>(Key);
		if (FakeDataString.IsEmpty() == false)
		{
			FHubResponseMessageHeader ResponseHeader{
				EHubMessageType::RESPONSE,
				Key.Controller,
				Key.Method,
				FakeDataString
			};

			if (TryConvertMessageToHeader(FakeDataString, ResponseHeader))
			{
				LogWarning(FString::Printf(TEXT("Fake response for method \"%s\""), *Key.Method));
				HandleMessageData(ResponseHeader);
			}

			return;
		}
	}
#endif

	if (Key.RequiredAuth && ConnectionState != EBFSocketConnectionState::Authorized)
	{
		EnqueueMessage(Key, StringToSend);
		return;
	}
	if (TrySend(StringToSend) == false)
	{
		EnqueueMessage(Key, StringToSend);
	}
}

template <typename TStruct>
typename FCallbackMessageHandle<TStruct>::FOnCallback& UHubSocketSystem::Bind(const FHubServiceAction& Key)
{
	if (Handlers.Contains(Key) == false)
	{
		TSharedPtr<FCallbackMessageHandle<TStruct>> NewHandler = MakeShareable(new FCallbackMessageHandle<TStruct>());
		Handlers.Add(Key, NewHandler);
	}

	return StaticCastSharedPtr<FCallbackMessageHandle<TStruct>>(Handlers.FindChecked(Key))->MessageHandler;
}
