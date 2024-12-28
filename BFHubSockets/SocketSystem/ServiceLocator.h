#pragma once

#include "CoreMinimal.h"

#include "ServiceLocator.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(BFServiceLocator, Log, All);

UINTERFACE(BlueprintType, NotBlueprintable)
class BFHUBSOCKETS_API UBFHubService : public UInterface
{
	GENERATED_BODY()
};

class BFHUBSOCKETS_API IBFHubService
{
	GENERATED_BODY()

public:
	virtual void Init() = 0;

	virtual void Start() = 0;

	virtual void StartAuthorized() = 0;

	virtual void Reauthorize() = 0;

	virtual void Stop() = 0;
};

enum class EOperationsPerformedStates : uint8
{
	None = 0,
	Inited = 1 << 0,
	Started = 1 << 1,
	StartedAuth = 1 << 2,
	Stopped = 1 << 3,
};

inline bool operator &(uint8 lhs, EOperationsPerformedStates rhs)
{
	return lhs & static_cast<uint8>(rhs);
}

inline uint8& operator^=(uint8& lhs, EOperationsPerformedStates rhs)
{
	lhs = lhs ^ static_cast<uint8>(rhs);
	return lhs;
}

inline uint8& operator|=(uint8& lhs, EOperationsPerformedStates rhs)
{
	lhs = lhs | static_cast<uint8>(rhs);
	return lhs;
}

UCLASS()
class BFHUBSOCKETS_API UServiceLocator : public UObject
{
	GENERATED_BODY()

private:
	UPROPERTY()
	TMap<UClass*, TScriptInterface<IBFHubService>> Services;

public:
	template <typename T>
	T* RegisterService();
	void RegisterService(UClass* ServiceClass);

	template <typename T>
	void UnregisterService();
	void UnregisterService(const UClass* ServiceClass);

	template <typename T>
	T* GetService();

	void InitServices();
	void StartServices();
	void StartAuthorizedServices();
	void ReauthorizeServices();
	void StopServices();

private:
	uint8 OperationsStatesMask = 0;

	bool IsOperationPerformed(const EOperationsPerformedStates Operation) const;
	void SetOperationPerformed(const EOperationsPerformedStates Operation);
	void SetOperationUnperformed(const EOperationsPerformedStates Operation);
};

template <typename T>
T* UServiceLocator::RegisterService()
{
	if (!ensureAlwaysMsgf(T::StaticClass()->ImplementsInterface(UBFHubService::StaticClass()),
						TEXT("T must implement the IBFHubService interface")))
	{
		return nullptr;
	}

	RegisterService(T::StaticClass());

	return GetService<T>();
}

template <typename T>
void UServiceLocator::UnregisterService()
{
	UnregisterService(T::StaticClass());
}

template <typename T>
T* UServiceLocator::GetService()
{
	return Cast<T>(Services.FindRef(T::StaticClass()).GetObject());
}
