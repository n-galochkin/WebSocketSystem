#pragma once

#include "CoreMinimal.h"
#include "JsonObjectConverter.h"
#include "HubServicesBaseData.h"

struct FBaseMessageHandle
{
	virtual ~FBaseMessageHandle() = default;

	virtual bool HandleMessage(const FString& InMessage) = 0;
	virtual bool HandleError(const FHubErrorData& ErrorData) const = 0;

	virtual void Clear() = 0;
};

struct FCallbackErrorHandle : FBaseMessageHandle
{
	friend class UHubSocketSystem;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnError, const FHubErrorData&);

protected:
	FOnError ErrorHandler;

	virtual bool HandleError(const FHubErrorData& ErrorData) const override
	{
		if (ErrorHandler.IsBound())
		{
			ErrorHandler.Broadcast(ErrorData);
		}

		return true;
	}

	virtual void Clear() override
	{
		ErrorHandler.Clear();
	}
};

template <class TStruct>
struct FCallbackMessageHandle : FCallbackErrorHandle
{
	friend class UHubSocketSystem;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCallback, const TStruct&);

protected:
	FOnCallback MessageHandler;

	virtual bool HandleMessage(const FString& InMessage) override
	{
		TStruct Structure;
		if(InMessage.IsEmpty())
		{
			MessageHandler.Broadcast(Structure);
			return true;
		}
		if (FJsonObjectConverter::JsonObjectStringToUStruct(InMessage, &Structure))
		{
			if (MessageHandler.IsBound())
			{
				MessageHandler.Broadcast(Structure);
			}

			return true;
		}

		return false;
	}

	virtual void Clear() override
	{
		FCallbackErrorHandle::Clear();

		MessageHandler.Clear();
	}
};
