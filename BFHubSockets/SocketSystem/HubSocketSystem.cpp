// 

#include "HubSocketSystem.h"

#include "BFHubSettings.h"
#include "IWebSocket.h"
#include "MessageHandle.h"
#include "WebSocketsModule.h"
#include "BFHttpModule/HttpClient/NetLog.h"
#include "BFHubSockets/Services/BFHubService_Ping.h"
#include "BFHubSockets/Services/GameClientAPI/Authorization/BFHubService_Authorization.h"
#include "BFHubSockets/Services/GameServerAPI/BFHubService_ServerInit.h"
#include "HelpersPlugin/Helpers/EnumHelpers.h"
#include "HelpersPlugin/Helpers/LogHelpers.h"

#include "Logging/StructuredLog.h"
DEFINE_LOG_CATEGORY(BFHubSocketSystem);
#undef CHANNEL
#define CHANNEL(Verbosity, Format, ...) BF_LOGFMT_SIDE(BFHubSocketSystem, Verbosity, Format, ##__VA_ARGS__)
#define LOG(Format, ...) CHANNEL(Log, Format, ##__VA_ARGS__)
#define WARNING(Format, ...) CHANNEL(Warning, Format, ##__VA_ARGS__)
#define ERROR(Format, ...) CHANNEL(Error, Format, ##__VA_ARGS__)
#define VERBOSE(Format, ...) CHANNEL(Verbose, Format, ##__VA_ARGS__)

void UHubSocketSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CreateServicesLocator();
	Services->RegisterService<UBFHubService_Ping>();

	if (GetGameInstance()->IsDedicatedServerInstance())
	{
		Services->RegisterService<UBFHubService_ServerInit>()->SubscribeOnAuthorized(
			FInitilizedDelegate::FDelegate::CreateUObject(this, &UHubSocketSystem::OnAuthorized));

		// not used now
		StartConnectionCmdlineUrl();
		// by default we get url from agones, it's not working in editor
	}
	else
	{
		Services->RegisterService<UBFHubService_Authorization>()->SubscribeOnAuthorized(
			FAuthoraizedDelegate::FDelegate::CreateUObject(this, &UHubSocketSystem::OnAuthorized));

		StartConnectionSettingsUrl();
	}

	Services->InitServices();
}

void UHubSocketSystem::Deinitialize()
{
	if (Services)
	{
		Services->StopServices();
	}

	if (Socket.IsValid())
	{
		Socket->Close();
	}

	Super::Deinitialize();
}


void UHubSocketSystem::StartConnection(const FString& Url)
{
	LOG("Trying to connect to the: {0}", Url);
	if (Url.IsEmpty())
	{
		WARNING("Failed to connect. Destination url is empty");
		return;
	}
	if (ConnectionURL.IsEmpty() == false && Url.Equals(ConnectionURL))
	{
		WARNING("Failed to connect. The destination url is same: {0}", Url);
		return;
	}

	LOG("Set new url: {0}", Url);

	ConnectionURL = Url;
	CreateSocket();
	Connect();
}

UServiceLocator* UHubSocketSystem::GetServicesLocator()
{
	CreateServicesLocator();

	return Services;
}

void UHubSocketSystem::CreateServicesLocator()
{
	if (Services == nullptr)
	{
		Services = NewObject<UServiceLocator>(this);
	}
}

void UHubSocketSystem::StartConnectionCmdlineUrl()
{
	// Note used now, but maybe for debugging
	FString Url;
	if (FParse::Value(FCommandLine::Get(), TEXT("-HUB_SOCKET_URL="), Url))
	{
		LOG("Got hub socket url from cmdline: {0}", Url);

		StartConnection(Url);
	}
}

void UHubSocketSystem::StartConnectionSettingsUrl()
{
	const FString SocketDomain = BFHub::GetBFHubSettings()->SocketDomain;
	if (SocketDomain.IsEmpty() == false)
	{
		LOG("Using url from settings: {0}", SocketDomain);
		StartConnection(SocketDomain);
	}
	else
	{
		LOG("Using default url: {0}", ConnectionURL);
	}
}

void UHubSocketSystem::CreateSocket()
{
	if (ConnectionURL.IsEmpty())
	{
		ERROR("Empty connection url");
		return;
	}

	Socket = FWebSocketsModule::Get().CreateWebSocket(ConnectionURL, TEXT("wss"));

	if (Socket.IsValid() == false)
	{
		ERROR("Failed to create WebSocket");
		return;
	}

	// Connection
	Socket->OnConnected().AddUObject(this, &UHubSocketSystem::OnConnected);
	Socket->OnClosed().AddUObject(this, &UHubSocketSystem::OnClosed);
	Socket->OnConnectionError().AddUObject(this, &UHubSocketSystem::OnConnectionError);

	// Messaging
	Socket->OnMessageSent().AddUObject(this, &UHubSocketSystem::OnMessageSent);
	Socket->OnMessage().AddUObject(this, &UHubSocketSystem::OnMessage);

	SetConnectionState(EBFSocketConnectionState::Created);
}

void UHubSocketSystem::Connect()
{
	if (Socket.IsValid())
	{
		Socket->Connect();
		SetConnectionState(EBFSocketConnectionState::Connecting);
	}
	else
	{
		ERROR("Socket not created, cant connect");
	}
}

void UHubSocketSystem::SetConnectionState(EBFSocketConnectionState State)
{
	LOG("Change connection state: {0}", EnumValueToString(State));

	ConnectionState = State;
	OnConnectionStateChanged.Broadcast(ConnectionState);

	TrySendQueuedMessages();
}

bool UHubSocketSystem::TrySend(const FString& InRawString) const
{
	if (IsConnected())
	{
		Socket->Send(InRawString);
		return true;
	}

	return false;
}

void UHubSocketSystem::TrySendQueuedMessages()
{
	if (IsConnected())
	{
		if (ConnectionState == EBFSocketConnectionState::Authorized)
		{
			if (QueuedMessages.IsEmpty() == false)
			{
				LOG("Send queued messages after authorization");
				DequeueMessages(QueuedMessages);
			}
		}
		else if (ConnectionState == EBFSocketConnectionState::Established)
		{
			if (QueuedNonAuthMessages.IsEmpty() == false)
			{
				LOG("Send queued non auth messages after connection established");
				DequeueMessages(QueuedNonAuthMessages);
			}
		}
	}
}

void UHubSocketSystem::DequeueMessages(TQueue<FString>& Queue) const
{
	for (FString Message; Queue.Dequeue(Message);)
	{
		Socket->Send(Message);
	}
}

void UHubSocketSystem::EnqueueMessage(const FHubServiceAction& Key, const FString& InRawMessage)
{
	WARNING("Can't send message, socket is not connected! Current message request queued - key: {0}. ", Key.ToString());

	if (Key.RequiredAuth == false)
	{
		QueuedNonAuthMessages.Enqueue(InRawMessage);
	}
	else
	{
		QueuedMessages.Enqueue(InRawMessage);
	}
}

bool UHubSocketSystem::IsConnected() const
{
	if (Socket.IsValid() == false)
	{
		return false;
	}
	return Socket->IsConnected() && (ConnectionState == EBFSocketConnectionState::Authorized || ConnectionState == EBFSocketConnectionState::Established);
}

FCallbackErrorHandle::FOnError& UHubSocketSystem::BindError(const FHubServiceAction& Key)
{
	ensureMsgf(Handlers.Contains(Key), TEXT("Bind error handle allow only after bind message handle! %s"), *Key.Method);

	return StaticCastSharedPtr<FCallbackErrorHandle>(Handlers.FindChecked(Key))->ErrorHandler;
}

void UHubSocketSystem::Unbind(const FHubServiceAction& Key)
{
	if (Handlers.Contains(Key))
	{
		Handlers[Key]->Clear();
	}
}

void UHubSocketSystem::StartReconnectTimer()
{
	if (GetDefault<USocketSettings>()->bAutoReconnectEnabled == false)
	{
		return;
	}

	if (GetWorld() && !GetWorld()->GetTimerManager().IsTimerActive(ReconnectTimerHandle))
	{
		WARNING("Start reconnecting timer in {0} seconds", CurrentReconnectTimeInterval);

		GetWorld()->GetTimerManager().SetTimer(ReconnectTimerHandle, this, &UHubSocketSystem::Connect, CurrentReconnectTimeInterval, true);

		SetConnectionState(EBFSocketConnectionState::WaitingForReconnect);

		// increase time interval every reconnect for avoid spam connections on server
		// reset when connection will be establishing
		const auto Settings = GetDefault<USocketSettings>();
		CurrentReconnectTimeInterval *= Settings->ReconnectTimeIncreaseStep;
		CurrentReconnectTimeInterval = FMath::Clamp(CurrentReconnectTimeInterval, Settings->ReconnectTimeStartInterval, Settings->ReconnectTimeMaxInterval);
	}
}

void UHubSocketSystem::StopReconnectTimer()
{
	if (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(ReconnectTimerHandle))
	{
		GetWorld()->GetTimerManager().ClearTimer(ReconnectTimerHandle);

		CurrentReconnectTimeInterval = GetDefault<USocketSettings>()->ReconnectTimeStartInterval;

		Services->ReauthorizeServices();

		LOG("Stop reconnecting timer");
	}
}

void UHubSocketSystem::OnConnected()
{
	LOG("Connected to Hub with WebSocket!");

	StopReconnectTimer();

	SetConnectionState(EBFSocketConnectionState::Connected);

	/** after connect we waiting first message from hub
	 * for identify connection as enstablised
	 * see OnMessage */
}

void UHubSocketSystem::OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	StopCommunication();

	if (StatusCode > 1000)
	{
		WARNING("Connection closed with abnormal code: {0}, Reason: {1}", StatusCode, Reason);

		StartReconnectTimer();
	}
	else
	{
		LOG("Connection closed");
	}
}

void UHubSocketSystem::StopCommunication()
{
	SetConnectionState(EBFSocketConnectionState::Closed);

	Services->StopServices();
}

void UHubSocketSystem::OnConnectionError(const FString& ErrorString)
{
	ERROR("Connection Error: {0}", ErrorString);

	StopCommunication();

	StartReconnectTimer();
}

void UHubSocketSystem::OnMessageSent(const FString& MessageString)
{
	NetLog::LogMessage(ELogVerbosity::Verbose, "Message Sent: {0}", MessageString);
	MessageSentDelegate.Broadcast();
}

void UHubSocketSystem::OnMessage(const FString& MessageString)
{
	if (ConnectionState == EBFSocketConnectionState::Connected)
	{
		LOG("First message received - connection established");

		SetConnectionState(EBFSocketConnectionState::Established);

		Services->StartServices();
		return;
	}

	if (ConnectionState == EBFSocketConnectionState::Established || ConnectionState == EBFSocketConnectionState::Authorized)
	{
		NetLog::LogMessage(ELogVerbosity::Verbose, "Message Received: {0}", MessageString);

		FHubResponseMessageHeader Header;
		if (TryConvertMessageToHeader(MessageString, Header))
		{
			HandleMessageData(Header);
		}
	}
	else
	{
		WARNING("Skip handle message: {0}, connection state: {1}", MessageString, EnumValueToString(ConnectionState));
	}
}

void UHubSocketSystem::OnAuthorized()
{
	LOG("Authorized on hub completed!");

	SetConnectionState(EBFSocketConnectionState::Authorized);

	Services->StartAuthorizedServices();
}

void UHubSocketSystem::LogVerbose(const FString& Message)
{
	VERBOSE("{0}", Message);
}

void UHubSocketSystem::LogWarning(const FString& Message)
{
	WARNING("{0}", Message);
}

void UHubSocketSystem::LogError(const FString& Message)
{
	ERROR("{0}", Message);
}

bool UHubSocketSystem::TryConvertMessageToHeader(const FString& MessageString, FHubResponseMessageHeader& Header) const
{
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(MessageString);

	if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
	{
		ERROR("Failed to parse message: {0}, this is not json!", MessageString);
		return false;
	}

	if (!FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), FHubResponseMessageHeader::StaticStruct(), &Header))
	{
		ERROR("Header structures not match for message: {0}", MessageString);
		return false;
	}

	return true;
}

void UHubSocketSystem::HandleMessageData(const FHubResponseMessageHeader& Header)
{
	if (Handlers.Contains(Header) == false)
	{
		ERROR("Not found handler for: {0}", Header.Method);
		return;
	}

	if (Header.Type == EHubMessageType::ERROR)
	{
		FHubErrorData ErrorData;
		ErrorData.RawData = Header.Data;
		if (FJsonObjectConverter::JsonObjectStringToUStruct(Header.Data, &ErrorData))
		{
			ERROR("Got error message for {0} with code: {1}, message: {2}", Header.Method, EnumValueToString(ErrorData.Code), ErrorData.ErrorMessage);

			Handlers[Header]->HandleError(ErrorData);
		}
		else
		{
			ERROR("Got error message for {0} with wrong data: {1}", Header.Method, Header.Data);
		}
	}
	else
	{
		Handlers[Header]->HandleMessage(Header.Data);
	}
}

FString UHubSocketSystem::GetErrorString(const EHubSocketErrorCodes ErrorCode)
{
	if (const FString* ErrorString = HubErrorCodeToStringMap.Find(ErrorCode))
	{
		return *ErrorString;
	}

	// Return a default message if the error code isn't found
	return TEXT("Unknown error.");
}

FString UHubSocketSystem::GetErrorStringFrom(int32 ErrorCodeAsInt)
{
	if (const EHubSocketErrorCodes* ErrorCode = reinterpret_cast<EHubSocketErrorCodes*>(&ErrorCodeAsInt))
	{
		return GetErrorString(*ErrorCode); // Use the existing function for enum handling
	}

	// Return a default message if the int32 can't be mapped to a valid enum
	return TEXT("Unknown error.");
}