
#include "BFHubService_ServerInit.h"

#include "BFHubSettings.h"
#include "BFHubSockets/SocketSystem/HubSocketSystem.h"
#include "HelpersPlugin/Helpers/CoreHelpers.h"

#include "Logging/StructuredLog.h"
DEFINE_LOG_CATEGORY(BFHubService_ServerInit);
#undef CHANNEL
#define CHANNEL(Verbosity, Format, ...) UE_LOGFMT(BFHubService_ServerInit, Verbosity, Format, ##__VA_ARGS__)
#define LOG(Format, ...) CHANNEL(Log, Format, ##__VA_ARGS__)
#define WARNING(Format, ...) CHANNEL(Warning, Format, ##__VA_ARGS__)
#define ERROR(Format, ...) CHANNEL(Error, Format, ##__VA_ARGS__)

void UBFHubService_ServerInit::Init()
{
	Super::Init();

	BaseAction.Fill("init", EHubControllerType::AUTH);
	BaseAction.RequiredAuth = false;

	GetBindedHandle<FBFHubResponseData_ServerInit>().AddUObject(this, &UBFHubService_ServerInit::OnResponse);
}

void UBFHubService_ServerInit::Reauthorize()
{
	Super::Reauthorize();

	// reauthorize only if we are initialized(authorized)
	if (bIsInitialized)
	{
		SendInit();
	}
}

void UBFHubService_ServerInit::SendInit()
{
	if (ServerName.IsEmpty() || ServerPassword.IsEmpty())
	{
		ERROR("No auth data to send init");
		return;
	}

	const FString BuildVersion = UCoreHelper::GetGameBuildVersionString();
	SendRequestToHub(FBFHubRequestData_ServerInit{ServerName, ServerPassword, BuildVersion, ServerRegionId});
}

void UBFHubService_ServerInit::SubscribeOnInitialized(const FInitilizedDelegate::FDelegate& Delegate)
{
	if (bIsInitialized)
	{
		Delegate.ExecuteIfBound();
		return;
	}
	OnInitialized.Add(Delegate);
}

void UBFHubService_ServerInit::SubscribeOnAuthorized(const FInitilizedDelegate::FDelegate& Delegate)
{
	if(bIsAuthorized)
	{
		Delegate.ExecuteIfBound();
	}
	OnAuthorized.Add(Delegate);
}

void UBFHubService_ServerInit::SetDevCredentials()
{
	const auto* HubSettings = GetDefault<UBFHubSettings>();

	SetCredentials(HubSettings->DevServerName, HubSettings->DevServerPassword, FString());
}

void UBFHubService_ServerInit::SetCredentials(const FString& InServerName, const FString& InServerPassword, const FString& InServerRegionId)
{
	ServerName = InServerName;
	ServerPassword = InServerPassword;
	ServerRegionId = InServerRegionId;

	LOG("New credentials with ServerName: {0}", ServerName);
}

void UBFHubService_ServerInit::OnResponse(const FBFHubResponseData_ServerInit& Data)
{
	if (Data.MatchId.IsEmpty())
	{
		WARNING("Init response received with empty match id");
	}
	else
	{
		MatchId = Data.MatchId;
	}

	// we have the same reauthorize logic but many systems can be initialized only once
	if (bIsInitialized == false)
	{
		bIsInitialized = true;
		OnInitialized.Broadcast();
	}

	bIsAuthorized = true;
	OnAuthorized.Broadcast();
}
