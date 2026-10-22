#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTimeSyncComponent.generated.h"

// 왕복 시간의 절반으로 서버 시계를 맞춤. 최종 시각 검사는 서버에서 처리함.
UCLASS()
class LATENCY_MULTICOMBAT_API UCombatTimeSyncComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCombatTimeSyncComponent();
	bool IsReady() const;
	double GetServerTime() const;
	float GetRoundTripSeconds() const { return RoundTripSeconds; }

protected:
	virtual void TickComponent(
		float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
	UFUNCTION(Server, Unreliable)
	void ServerRequestTime(uint32 RequestId, double SentAt);
	UFUNCTION(Client, Unreliable)
	void ClientReceiveTime(uint32 RequestId, double SentAt, double ServerTime);
	double LastRequestAt = -100.0;
	double LastServerRequestAt = -100.0;
	double Offset = 0.0;
	float RoundTripSeconds = 0.f;
	uint32 RequestSequence = 0;
	bool bReady = false;
};
