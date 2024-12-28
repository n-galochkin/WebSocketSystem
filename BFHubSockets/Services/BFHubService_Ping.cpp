#include "BFHubService_Ping.h"

#include "BFHubSockets/SocketSystem/HubSocketSystem.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY(BFHubService_Ping);
#undef CHANNEL
#define CHANNEL(Verbosity, Format, ...) UE_LOGFMT(BFHubService_Ping, Verbosity, Format, ##__VA_ARGS__)
#define LOG(Format, ...) CHANNEL(Log, Format, ##__VA_ARGS__)
#define WARNING(Format, ...) CHANNEL(Warning, Format, ##__VA_ARGS__)
#define ERROR(Format, ...) CHANNEL(Error, Format, ##__VA_ARGS__)
#define VERBOSE(Format, ...) CHANNEL(Verbose, Format, ##__VA_ARGS__)

void UBFHubService_Ping::Init()
{
	Super::Init();

	BaseAction.Fill("ping", EHubControllerType::AUTH);
	BaseAction.RequiredAuth = false;

	SocketSystem->SubscribeToMessageSentEvent(FMessageSentDelegate::FDelegate::CreateUObject(
		this, &UBFHubService_Ping::OnAnyMessageSent));
}

void UBFHubService_Ping::Start()
{
	Super::Start();
	GetBindedHandle<FBFHubResponseData_Ping>().AddUObject(this, &UBFHubService_Ping::OnResponse);
	StartTimer();
}

void UBFHubService_Ping::Stop()
{
	Super::Stop();
	SocketSystem->Unbind(BaseAction);
	StopTimer();
}

void UBFHubService_Ping::StartTimer()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PingTimerHandle, this, &UBFHubService_Ping::SendPing, PingInterval, true);
		VERBOSE("Ping timer started with interval {0} s", PingInterval);
	}
}

void UBFHubService_Ping::StopTimer()
{
	if (PingTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(PingTimerHandle);

		VERBOSE("Ping timer stopped");
	}
}

void UBFHubService_Ping::SendPing() const
{
	SendRequestToHub(FBFHubRequestData_Ping{GetCurrentTimestampInMS()});
}

void UBFHubService_Ping::OnAnyMessageSent()
{
	VERBOSE("Connection save aftet sent message - reset ping timer");

	StopTimer();
	StartTimer();
}

void UBFHubService_Ping::OnResponse(const FBFHubResponseData_Ping& Data)
{
	LastPingTime = GetCurrentTimestampInMS() - Data.Nonce;
	OnPingReceived.Broadcast(LastPingTime);

	if (HighestPingTime < LastPingTime)
	{
		HighestPingTime = LastPingTime;
		WARNING("Got new highest ping time: {0} ms", LastPingTime);
	}
}

int64 UBFHubService_Ping::GetCurrentTimestampInMS()
{
	return FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
}
