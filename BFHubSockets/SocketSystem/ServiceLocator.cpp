#include "ServiceLocator.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY(BFServiceLocator);
#undef CHANNEL
#define CHANNEL(Verbosity, Format, ...) UE_LOGFMT(BFServiceLocator, Verbosity, Format, ##__VA_ARGS__)
#define LOG(Format, ...) CHANNEL(Log, Format, ##__VA_ARGS__)
#define WARNING(Format, ...) CHANNEL(Warning, Format, ##__VA_ARGS__)
#define ERROR(Format, ...) CHANNEL(Error, Format, ##__VA_ARGS__)
#define VERBOSE(Format, ...) CHANNEL(Verbose, Format, ##__VA_ARGS__)

void UServiceLocator::RegisterService(UClass* ServiceClass)
{
	if (Services.Contains(ServiceClass))
	{
		return;
	}

	const TScriptInterface<IBFHubService> ServiceInterface = NewObject<UObject>(GetOuter(), ServiceClass);
	Services.Add(ServiceClass, ServiceInterface);

	LOG("Service {0} registered", ServiceClass->GetName());

	if (IsOperationPerformed(EOperationsPerformedStates::Inited))
	{
		ServiceInterface->Init();
	}

	if (IsOperationPerformed(EOperationsPerformedStates::Stopped))
	{
		ServiceInterface->Stop();
	}
	else
	{
		if (IsOperationPerformed(EOperationsPerformedStates::Started))
		{
			ServiceInterface->Start();
		}

		if (IsOperationPerformed(EOperationsPerformedStates::StartedAuth))
		{
			ServiceInterface->StartAuthorized();
		}
	}
}

void UServiceLocator::UnregisterService(const UClass* ServiceClass)
{
	if (Services.Contains(ServiceClass))
	{
		Services[ServiceClass]->Stop();
		Services.Remove(ServiceClass);

		LOG("Service {0} unregistered", ServiceClass->GetName());
	}
	else
	{
		WARNING("Service {0} not registered", ServiceClass->GetName());
	}
}

void UServiceLocator::InitServices()
{
	for (const auto& Service : Services)
	{
		Service.Value->Init();
	}

	SetOperationPerformed(EOperationsPerformedStates::Inited);
}

void UServiceLocator::StartServices()
{
	for (const auto& Service : Services)
	{
		Service.Value->Start();
	}

	SetOperationUnperformed(EOperationsPerformedStates::Stopped);

	SetOperationPerformed(EOperationsPerformedStates::Started);
}

void UServiceLocator::StartAuthorizedServices()
{
	for (const auto& Service : Services)
	{
		Service.Value->StartAuthorized();
	}

	SetOperationPerformed(EOperationsPerformedStates::StartedAuth);
}

void UServiceLocator::ReauthorizeServices()
{
	for (const auto& Service : Services)
	{
		Service.Value->Reauthorize();
	}
}

void UServiceLocator::StopServices()
{
	for (const auto& Service : Services)
	{
		Service.Value->Stop();
	}

	SetOperationUnperformed(EOperationsPerformedStates::Started);
	SetOperationUnperformed(EOperationsPerformedStates::StartedAuth);

	SetOperationPerformed(EOperationsPerformedStates::Stopped);
}

bool UServiceLocator::IsOperationPerformed(const EOperationsPerformedStates Operation) const
{
	return OperationsStatesMask & Operation;
}

void UServiceLocator::SetOperationPerformed(const EOperationsPerformedStates Operation)
{
	OperationsStatesMask |= Operation;
}

void UServiceLocator::SetOperationUnperformed(const EOperationsPerformedStates Operation)
{
	OperationsStatesMask ^= Operation;
}
