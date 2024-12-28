#include "BFHubService_Base.h"

#include "BFHubSettings.h"
#include "BFCoreTags/ServiceTags.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY(LogBFHubService_Base);

void UBFHubService_Base::Init()
{
	SocketSystem = Cast<UHubSocketSystem>(GetOuter());
}

void UBFHubService_Base::Start()
{
}

void UBFHubService_Base::StartAuthorized()
{
}

void UBFHubService_Base::Reauthorize()
{
}

void UBFHubService_Base::Stop()
{
}

FCallbackMessageHandle<FHubEmptyData>::FOnCallback& UBFHubService_Base::BindHandle(const FHubServiceAction& Key)
{
	return BindHandle<FHubEmptyData>(Key);
}

void UBFHubService_Base::SendRequestToHub(const FHubServiceAction& Key) const
{
	SendRequestToHub(Key, FHubEmptyData());
}

FCallbackErrorHandle::FOnError& UBFHubService_Base::GetBindedErrorHandle() const
{
	return SocketSystem->BindError(BaseAction);
}

FCallbackErrorHandle::FOnError& UBFHubService_Base::GetBindedErrorHandle(const FHubServiceAction& Key) const
{
	return SocketSystem->BindError(Key);
}

void UBFHubService_Base::OnErrorReceived(const FHubErrorData& ErrorData)
{
	UE_LOGFMT(LogBFHubService_Base, Error, "Error received with code {0}", ErrorData.GetErrorCodeText());

	if (GetDefault<UBFHubSettings>()->EnableErrorPopups)
	{
		NotifyError(ErrorData);
	}
}

void UBFHubService_Base::NotifyError(const FHubErrorData& ErrorData)
{
	const FString ServiceName = ServiceReadableName.IsEmpty() == false ? ServiceReadableName : "Hub Service Error:";
	const FHubServiceErrorNotificationMessage NotificationMessage{ServiceName, ErrorData};
	UGameplayMessageSubsystem::Get(this).BroadcastMessage(ServiceTags::FHubChannel::Tag_BaseError(), NotificationMessage);
}
