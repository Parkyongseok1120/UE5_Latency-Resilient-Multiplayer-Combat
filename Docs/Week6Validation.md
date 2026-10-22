# 6주차 구현과 검증 결과

첨부 5회차 보고서의 `04 차주 계획`을 기준으로 기존 책임 분리·데이터 에셋 구조를 보완하고, 서버 리와인드 Hitscan Lag Compensation과 이동 표적 실험을 구현했다. 보고서에는 차주가 5주차로 표기되어 있지만 이번 작업은 사용자 요청에 따라 6주차로 기록한다. 셰이더 예시는 차주 계획의 별도 구현 요구가 없어 새 셰이더 산출물로 취급하지 않았다.

## 구현 결과

캐릭터에는 `RewindHistoryComponent`, 무기에는 `WeaponHitValidationComponent`, PlayerController에는 `CombatTimeSyncComponent`를 추가했다. Fire는 발사 순서·권한, Ammo는 실제 탄약·재장전, Health와 GameMode는 피해·사망·팀·점수·리스폰을 계속 담당한다.

`Content/Data/DA_CombatNetwork.uasset`가 기본 네트워크 조정 에셋이다. 캐릭터·무기 수치의 NaN·무한대·음수 검증을 보완했고, 기존 utility namespace는 `FCombatRules` static struct로 바꿨다. 주요 처리에는 짧은 한국어 주석을 추가했다. 세부 정책과 실행 방법은 [구현 문서](LagCompensationPlan.md)를 참고한다.

## 자동 검증

UE 5.7.4 / Windows 11 / Win64 Development에서 Editor와 Game 타깃을 빌드했다. `LatencyCombat` 자동화 테스트 17개에는 기존 9개와 추가 8개가 포함된다. 최종 결과와 로그는 `Saved/Week6/CompletedAutomation/` 및 `Saved/Week6/CompletedAutomation.log`에 저장한다.

추가 테스트는 이력 순환·경계·보간·teleport/팀/생명/사망 경계, 캡슐의 최근접·접선·상하단·내부 교차, 발사 시각·순서·생명 ID·비유한 입력, 이동 표적 on/off, 리스폰·접속 종료, 피해 직전 재검사, 데이터 수치 검증, ADS FOV·연사·발사 해제·재장전/사망 중 직접 요청 차단을 검사한다.

실제 별도 서버와 클라이언트 둘을 사용한 PktLag=100ms·200ms의 보정 on, 0ms의 보정 off smoke에서 HP·방어구·사망·킬·팀 점수·리스폰이 양쪽에서 일치했다. 이동 표적 실험의 모든 조건에서도 공격자의 발사 결과와 관찰자의 이동·HP 복제를 확인했다.

## 이동 표적 비교

서버가 18m 앞 표적을 좌우 진폭 240cm, 주기 2.4초의 같은 사인 경로로 이동시킨다. 공통 epoch 이후 5초부터 0.65초 간격으로 발사하고, 같은 순서에 같은 spread seed를 사용한다. 매 4번째 발사는 위로 빗맞히고 나머지는 클라이언트에 표시된 캡슐을 겨냥한다. 예상 명중은 발사 당시 표시된 캡슐에 탄 퍼짐을 적용한 ray의 교차 여부다. 서버 이력이나 실제 결과를 예상값으로 복사하지 않는다.

조건당 24발, 다섯 지연 × 두 모드 × 1회로 총 240발이다. loss/jitter=0, 서버·클라이언트는 60fps 제한이다. PktLag은 송신 패킷당 편도 추가 지연이며 추가 RTT는 약 두 배다. 고정 시나리오의 결과로 일반 플레이 전체의 정확도를 뜻하지 않는다.

| 편도 추가 지연 | Off 예상 명중 복원 | On 예상 명중 복원 | Off 응답 P95 | On 응답 P95 | On 관측 RTT 평균 |
|---:|---:|---:|---:|---:|---:|
| 0ms | 18/18 | 17/18 | 16.555ms | 33.172ms | 0.000ms |
| 50ms | 6/18 | 18/18 | 149.760ms | 149.817ms | 130.557ms |
| 100ms | 3/18 | 17/18 | 266.346ms | 249.736ms | 227.086ms |
| 150ms | 2/18 | 18/18 | 349.830ms | 349.764ms | 311.809ms |
| 200ms | 2/18 | 18/18 | 449.837ms | 449.840ms | 426.405ms |

예상 명중 총 90발 중 Off는 31발(34.44%), On은 88발(97.78%)을 복원했다. 두 모드 모두 예상 miss 30발을 miss로 처리했고 정상 요청 거절은 0이다. On의 누락은 0ms·100ms에서 각 1발이다. 수신 프레임, 시각 추정, 서버 ping 평균과 표시 시점의 차이로 경계 명중이 달라질 수 있지만, 이번 로그로 두 누락의 개별 원인을 확정하지 않는다.

원시 240표본은 [RawSamples.csv](Measurements/Week6/RawSamples.csv), 실행 목록은 [Runs.csv](Measurements/Week6/Runs.csv), 혼동행렬·P50/P95·RTT는 [Summary.csv](Measurements/Week6/Summary.csv)에 보관한다. 표본에는 로그 경로·요청 번호·모드·예상/실제 명중·거절 이유·응답 시간·이력의 나이가 있다. 상세 서버·클라이언트 로그는 각 `Saved/FPSBaseline/` 실행 폴더에 남긴다.

## 입력과 렌더링

실제 게임 창을 활성화하고 HUD·상대 팀 표시를 확인했다. 창 안 좌클릭으로 탄약이 30→29로 줄었고, R 키로 `RELOADING`이 표시된 뒤 30/89로 복구되는 것을 확인했다. Windows 입력 자동화로 수행했으며 사람이 직접 길게 누른 수동 플레이 기록과 구분한다. ADS FOV와 연사·해제는 엔진 자동화 테스트로 검증했다.

RTX 3090 / Ryzen 9 7900에서 960×540 D3D12 클라이언트 두 개를 렌더링하고 편도 100ms, 24발을 추가 실행했다. 캡처에서 상대 캐릭터·팀·HUD·1인칭 무기를 확인했다. 시험용 HP 10000은 해당 옵션에서만 적용되며 실전 체력·피해 규칙을 바꾸지 않는다.

| 모드 | 공격자 평균 프레임 | 공격자 평균 GPU | GPU 표본 |
|---|---:|---:|---:|
| On | 17.048ms (58.66fps) | 4.727ms | 1006프레임 |
| Off | 17.049ms (58.65fps) | 5.840ms | 1004프레임 |

각각 1회 실행이며 60fps 제한, 에디터 기반 실행과 동시 프로세스 영향이 있어 성능 개선율이나 GPU 비용 차이로 일반화하지 않는다. GPU 값은 `RHIGetGPUFrameCycles`의 실제 표본이다. NullRHI에서는 -1로 기록해 미측정과 0ms를 구분한다. 새 셰이더의 전후 비교는 포함하지 않았다.

렌더링 로그는 `Saved/FPSBaseline/20261022-023702-334-100ms-rewind/`(On), `Saved/FPSBaseline/20261022-023904-689-100ms-rewind/`(Off)다. 캡처는 `Saved/Week6/RewindShooter.png`, `RewindObserver.png`에 저장한다.

## 재현

```powershell
.\Tools\Measure-Rewind.ps1
.\Tools\Start-FPSBaseline.ps1 -LatencyMs 100 -SmokeTest
.\Tools\Start-FPSBaseline.ps1 -LatencyMs 100 -RewindBenchmark -CaptureFrames
```

현재 구현은 upright capsule hitscan과 현재 월드 엄폐물을 사용한다. 본별 hitbox, 과거 동적 엄폐물, 손실·지터 환경의 정량 성능과 비대칭 지연 정확도는 이번 검증 범위에 포함하지 않는다. 시각 구간 밖 요청은 이력으로 임의 추정하지 않고 거절하거나 해당 표적을 판정에서 제외한다.
