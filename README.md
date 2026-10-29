# UE5_Latency-Resilient-Multiplayer-Combat
서버 권위(Server Authority)를 기반으로 멀티플레이 전투 시스템을 구현하고, 네트워크 지연 환경에서 Prediction, Reconciliation, Lag Compensation 등을 적용해 입력 반응성과 타격 판정이 어떻게 달라지는지 직접 테스트하고 비교하는 프로젝트.

## FPS Baseline

UE 5.7의 서버 권위 FPS 팀 데스매치 Baseline을 구현했다. 기본 맵은 `/Game/Level/FPSBaseline`이며, 별도 입력·캐릭터·무기 에셋을 지정하지 않아도 기본 조작과 전투를 실행할 수 있다. 기존 `TestMap`, `BP_Default`, 데이터 에셋은 그대로 사용할 수 있다.

- WASD 이동, 마우스 시점, Space 점프, 좌클릭 발사, B 단발/연사 전환, 우클릭 ADS, R 재장전. 기본 탄창30발·예비330발로 총360발을 소지한다.
- 1인칭 손·팔(`SKM_CombatArms_Closed`)과 스켈레탈 라이플(`SKM_Rifle`)의 손잡이 정렬·반동·재장전, 상대 캐릭터의 팀 색상 마네킹(`SKM_Manny_Simple` + `ABP_CombatRifle`) 라이플 자세·이동·사격 모션 표시. 캐릭터 메시 상대 Z=-90, Yaw=-90을 적용하고 네트워크 이동 보정에도 유지한다.
- 3인칭 왼손을 라이플 손잡이에 고정하는 IK와 상하 조준에 따른 허리·목 보정. 엔진의 복제된 시선 각도를 사용해 다른 클라이언트에서도 같은 조준 자세를 표시한다.
- 조준점, 서버 명중 확인 표시, 좌하단 HP·방어구·탄약 HUD 및 **우측 실시간 서버·네트워크·Lag Compensation 수치/그래프 패널**(RTT·프레임 선 그래프, 최근 발사별 응답/리와인드 막대 차트, 판정 통계).
- 균형 팀 배정, 팀별 스폰, 사망 시 이동·발사 차단, 기본 3초 후 리스폰, 기본 50킬 승리.

발사 입력은 Character가 소유한 `CombatWeapon` 액터의 `WeaponFireComponent::ServerFire(FCombatShotCommand)`로 전달한다. 명령에는 발사 번호·생명 ID·서버 시계로 환산한 발사 시각·조준 방향을 포함한다. 서버가 발사 속도·탄약·재장전·사망·경기 종료 상태를 검사하고, 서버가 검증한 과거 카메라 원점과 표적 캡슐 이력으로 hitscan을 수행한다. 기능 off에서는 현재 월드 trace를 사용한다. 클라이언트는 피해량·발사 위치·명중 대상을 전달하지 않는다. 서버가 데이터 에셋의 피해량과 미터 단위 거리 감쇠를 계산해 HP와 방어구를 변경한다. 같은 팀에 대한 피해는 기본적으로 비활성화되어 있다.

HP·방어구·사망은 PlayerState의 `HealthComponent`, K/D·팀은 PlayerState, 팀 점수는 GameState가 관리하고 모든 클라이언트에 복제한다. 탄약은 무기 액터의 `WeaponAmmoComponent`가 관리하고 무기 소유 클라이언트에만 복제한다. 사격 표시와 명중 확인은 서버가 승인한 뒤 전달된다. 서버 리와인드 Lag Compensation은 구현되어 있고, 전투의 로컬 Prediction/Reconciliation은 후속 단계다. 이동은 기본 `CharacterMovement`의 네트워크 처리를 사용한다.

## 실행

사격 시 짧은 로컬 카메라 셰이크, 피격 시 빨간 화면 테두리, HP50 이하부터 가장자리에 나타나는 다색 사각 블록 글리치, 사망 후 시각 레그돌을 적용한다. [사격·피격·사망 표현과 강도 조정](Docs/CombatFeedback.md)을 참고한다.

프로젝트를 UE 5.7에서 빌드한 뒤 `FPSBaseline` 맵을 실행한다. 에디터의 플레이 설정에서 Net Mode를 `Play As Client`, Number of Players를 2 이상으로 설정하면 Dedicated Server를 기준으로 플레이할 수 있다. 입력 포커스를 원하는 게임 창에 맞춘다.

프로젝트 기본 PIE 설정은 `Play As Client`, 플레이어 2명, `New Editor Window (PIE)`다. 기존에 프로젝트를 열었던 에디터에서는 `Saved/Config`의 개인 설정이 기본값보다 우선하므로 플레이 버튼 옆 메뉴의 Multiplayer Options에서 Net Mode를 한 번 변경해야 한다. 실행 중인 플레이를 중지한 뒤 설정을 바꾸고 다시 실행한다.

두 창 제목이 모두 `NetMode: Standalone`이고 각 플레이어 목록에 한 명만 표시되면 두 개의 독립 게임을 실행한 상태다. 창 수를 2로 설정하는 것만으로는 같은 서버에 연결되지 않는다. `Play As Client`로 실행하면 두 창이 `Client`로 표시되고, 각 목록에 Red와 Blue 플레이어가 함께 나타난다. 상대는 팀 색상이 적용된 스켈레탈 캐릭터와 3인칭 총기로 표시된다.

PowerShell에서 별도 서버 프로세스와 클라이언트 창 2개를 띄우려면 프로젝트 루트에서 실행한다.

```powershell
.\Tools\Start-FPSBaseline.ps1 -LatencyMs 100 -ShowWindows
```

스크립트가 시작한 두 클라이언트를 닫으면 해당 서버도 종료된다. `-EngineRoot`로 엔진 경로를, `-Port`로 기본 18777 포트를 변경할 수 있다. Launcher 엔진에서도 `UnrealEditor.exe -server`를 사용한 독립 Dedicated Server 프로세스로 테스트한다. 배포용 Server 바이너리 패키징은 이 스크립트의 범위에 포함되지 않는다.

## 자동 검증과 지연 실험

오른쪽 HUD는 기본적으로 간단한 현재 상태만 표시하며, `Tab`을 누르면 **실시간 실험 지표** 상세 패널이 슬라이드되어 나온다. 상세 패널에서 리와인드 ON/OFF와 추가 지연 0/50/100/150/200ms를 바꿀 수 있다. 숫자 `1`로 커서를 열어 버튼을 클릭하거나 `F6`(리와인드), `F7`(지연 순환), `F8`(현재 조건 통계 초기화)을 사용한다. RTT·FPS·프레임 시간·사격 응답·발사 경과·표적 되감기 시간의 현재/평균/최소/최대를 표시한다. 조건은 서버가 승인해 세션 전체에 적용하고, 각 창의 통계는 로컬 플레이어 기준으로 새로 집계한다. [조작·표본 정의·워밍업 및 검증 방법](Docs/LiveExperimentHUD.md)을 참고한다.

서버 1개와 헤드리스 클라이언트 2개를 실행해 실제 게임 RPC를 검증한다. 첫 번째 클라이언트는 Red 공격자, 두 번째는 Blue 피해자다. 두 클라이언트가 HP 감소, 사망 1회, 킬·팀 점수 1 증가, HP 복구와 리스폰을 모두 관찰해야 PASS를 기록한다. 테스트 자동 조작은 Development에서 `-FPSBaselineSmoke`를 지정했을 때만 실행한다.

```powershell
# 단일 조건
.\Tools\Start-FPSBaseline.ps1 -LatencyMs 100 -SmokeTest

# 보고서의 다섯 조건
0, 50, 100, 150, 200 | ForEach-Object { .\Tools\Start-FPSBaseline.ps1 -LatencyMs $_ -SmokeTest }
```

지연 값은 양쪽 프로세스의 **송신 패킷당 추가 지연**이다. `-LatencyMs 100`은 서버→클라이언트 100ms, 클라이언트→서버 100ms를 추가하므로 왕복 지연은 대략 200ms 증가한다. 실제 핑에는 엔진 틱과 로컬 처리 시간도 포함된다. 패킷 손실과 지연 편차는 0으로 고정했다. 설정 기준은 [Epic Games 네트워크 에뮬레이션 문서](https://dev.epicgames.com/documentation/ko-kr/unreal-engine/using-network-emulation-in-unreal-engine?application_version=5.7)와 UE 5.7의 `FPacketSimulationSettings`이다.

각 실행의 `Server.log`, `Shooter.log`, `Observer.log`는 `Saved/FPSBaseline/<실행시각>-<지연>ms-<실행종류>/`에 저장한다. SmokeTest는 결과 일치와 재진입을 확인하는 기능 테스트다. 아래 Benchmark는 발사별 응답 시간도 기록한다.

## 4주차 검증 / 정량 측정 / 예측 설계

최신 커밋 `9f57b3f` 기준으로 `InitNewPlayer`에서 스폰 위치를 저장하기 전에 팀을 배정하고, 리스폰마다 팀과 자리 점유 상태를 다시 검사하도록 수정했다. Development 전용 `-FPSBaselineBenchmark`는 이동·점프·재장전·사망 중 발사 차단·점수·리스폰을 실제 게임 호출로 검증하고, 발사 식별자를 사용해 서버 효과 응답, 명중 확인, 상대 HP 반영 시간을 같은 클라이언트의 단조 시계로 측정한다. 리스폰 후에도 상대를 중앙선에 정렬하고 발사별 거리를 기록한다. 재현 도구는 저장소에 포함하는 `Tools/`에 둔다.

```powershell
# 다섯 지연 조건 × 3회 × 24개 표본 = 360개 발사
.\Tools\Measure-FPSBaseline.ps1

# GPU로 양쪽 상대 표시를 확인하고 캡처
.\Tools\Start-FPSBaseline.ps1 -LatencyMs 100 -Benchmark -CaptureFrames -ShowWindows
```

원시 표본·실행별 결과·평균/P50/P95는 `Saved/FPSBaseline/Measurements/<시각>/`의 CSV에 저장한다. 전체 발사에는 miss도 포함하며, HP/명중 확인 지연은 hit만 집계한다. 정지한 상대와 기본 spread 조건의 명중률을 기록하므로 이동 표적의 Lag Compensation 성능을 의미하지 않는다. 헤드리스 계측은 렌더링·물리 입력 지연을 측정하지 않는다.

실행 조건과 수동 체크 항목은 [검증 문서](Docs/FPSBaselineValidation.md), 이번 실행 수치는 [측정 결과](Docs/FPSBaselineResults.md), 후속 공격 Prediction/Reconciliation의 범위와 승인·보정 흐름은 [설계 문서](Docs/CombatPredictionDesign.md)에 정리했다. 공격 예측 자체는 아직 적용하지 않았다.

Unreal Automation의 `LatencyCombat`에는 서버 발사 간격, 초기 hitscan 피해, 조기 재장전 완료 차단, 탄약 이전, 클라이언트 상태 변경 차단, 중복 사망, 동시 리스폰, 엄폐/아군 피해 차단, 초기 팀 스폰과 측정 응답 대응 검증이 포함되어 있다. 에디터 Session Frontend에서 실행하거나 다음 명령으로 실행한다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$PWD\Latency_MultiCombat.uproject" -unattended -nullrhi -nosplash -NoSound '-ExecCmds=Automation RunTests LatencyCombat' '-TestExit=Automation Test Queue Empty'
```

새 테스트 맵을 생성해야 하는 체크아웃에서는 다음 명령을 사용할 수 있다. 이미 존재하는 `FPSBaseline` 맵은 덮어쓰지 않는다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' .\Latency_MultiCombat.uproject -EnablePlugins=PythonScriptPlugin -run=pythonscript "-script=$PWD\Scripts\CreateFPSBaselineMap.py" -unattended -nullrhi -NoSound
```

## 6주차 Lag Compensation

서버 이력 기록, RTT 기반 시각 동기화, 발사 번호·생명 ID·시각 검증, 과거 캡슐 판정과 현재 월드 엄폐 검사를 구현했다. 설정은 `/Game/Data/DA_CombatNetwork` 에셋에서 조정하며 기본 GameMode가 사용한다. 기존 무기·탄약·Health 컴포넌트를 재사용하고, 판정은 `WeaponHitValidationComponent`로 분리했다. 수치 검증과 거리 감쇠는 `FCombatRules` static 함수로 공유한다.

```powershell
# 이동 표적 5개 지연 × 보정 on/off, 총 240발
.\Tools\Measure-Rewind.ps1

# 화면 캡처와 실제 GPU 시간 기록
.\Tools\Start-FPSBaseline.ps1 -RewindBenchmark -LatencyMs 100 -CaptureFrames

# 보정 off로 현재 월드 판정 비교
.\Tools\Start-FPSBaseline.ps1 -RewindBenchmark -LatencyMs 100 -DisableLagCompensation
```

Win64 Development Editor·Game 빌드와 `LatencyCombat` 17개 테스트를 검증했다. 240표본 실험에서 예상 명중 복원은 off 31/90, on 88/90이며, 의도적 miss의 오명중과 정상 요청 거절은 0이었다. 원시 표본과 한계는 [6주차 검증 결과](Docs/Week6Validation.md)에 있다. 현재 범위는 upright capsule이며 동적 엄폐물은 현재 서버 월드를 기준으로 처리한다.

## 소스 구조

무기는 별도 `CombatWeapon` 액터이며 발사·판정, 탄약·재장전, 표현은 각각 컴포넌트가 소유한다. Character는 이동과 Pawn 생명주기를 연결하고, 입력은 Enhanced Input 컴포넌트와 입력 데이터 에셋에서 관리한다. 레거시 입력 경로와 Character·PlayerState의 중계 API를 제거했다. 클래스명·파일명의 프로젝트 접두어를 제거했고 기존 Blueprint 부모 클래스는 CoreRedirects로 연결한다. 상세 책임, SSOT 위치와 이전 API 대응은 [구조 문서](Docs/CombatArchitecture.md)에 정리한다.

M3의 구현 정책과 재현 방법은 [Lag Compensation 문서](Docs/LagCompensationPlan.md), 6주차 테스트와 정량 결과는 [검증 결과](Docs/Week6Validation.md)에 정리한다.
