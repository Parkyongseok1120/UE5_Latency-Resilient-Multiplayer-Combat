# UE5_Latency-Resilient-Multiplayer-Combat
서버 권위(Server Authority)를 기반으로 멀티플레이 전투 시스템을 구현하고, 네트워크 지연 환경에서 Prediction, Reconciliation, Lag Compensation 등을 적용해 입력 반응성과 타격 판정이 어떻게 달라지는지 직접 테스트하고 비교하는 프로젝트.

## FPS Baseline

UE 5.7의 서버 권위 FPS 팀 데스매치 Baseline을 구현했다. 기본 맵은 `/Game/Level/FPSBaseline`이며, 별도 입력·캐릭터·무기 에셋을 지정하지 않아도 기본 조작과 전투를 실행할 수 있다. 기존 `TestMap`, `BP_Default`, 데이터 에셋은 그대로 사용할 수 있다.

- WASD 이동, 마우스 시점, Space 점프, 좌클릭 발사와 연사, 우클릭 ADS, R 재장전.
- 1인칭 카메라와 기본 무기 표시, 상대 캐릭터의 팀 색상 표시.
- 조준점, 서버 명중 확인 표시, HP·방어구·탄약·K/D·팀 점수 HUD.
- 균형 팀 배정, 팀별 스폰, 사망 시 이동·발사 차단, 기본 3초 후 리스폰, 기본 50킬 승리.

발사 입력은 Character의 `ServerFire(AimRotation)`로 전달한다. 서버가 발사 속도·탄약·재장전·사망·경기 종료 상태를 검사하고, 서버의 카메라 위치에서 hitscan을 수행한다. 클라이언트는 피해량·발사 위치·명중 대상을 전달하지 않는다. 서버가 데이터 에셋의 피해량과 미터 단위 거리 감쇠를 계산해 HP와 방어구를 변경한다. 같은 팀에 대한 피해는 기본적으로 비활성화되어 있다.

HP·방어구·사망·K/D·팀 점수는 모든 클라이언트에 복제하고, 탄약은 무기 소유 클라이언트에 복제한다. 사격 표시와 명중 확인은 서버가 승인한 뒤 전달된다. 전투의 Prediction/Reconciliation과 Lag Compensation은 후속 단계이며, 이동은 기본 `CharacterMovement`의 네트워크 처리를 사용한다.

## 실행

프로젝트를 UE 5.7에서 빌드한 뒤 `FPSBaseline` 맵을 실행한다. 에디터의 플레이 설정에서 Net Mode를 `Play As Client`, Number of Players를 2 이상으로 설정하면 Dedicated Server를 기준으로 플레이할 수 있다. 입력 포커스를 원하는 게임 창에 맞춘다.

PowerShell에서 별도 서버 프로세스와 클라이언트 창 2개를 띄우려면 프로젝트 루트에서 실행한다.

```powershell
.\Scripts\Start-FPSBaseline.ps1 -LatencyMs 100 -ShowWindows
```

스크립트가 시작한 두 클라이언트를 닫으면 해당 서버도 종료된다. `-EngineRoot`로 엔진 경로를, `-Port`로 기본 7777 포트를 변경할 수 있다. Launcher 엔진에서도 `UnrealEditor.exe -server`를 사용한 독립 Dedicated Server 프로세스로 테스트한다. 배포용 Server 바이너리 패키징은 이 스크립트의 범위에 포함되지 않는다.

## 자동 검증과 지연 실험

서버 1개와 헤드리스 클라이언트 2개를 실행해 실제 게임 RPC를 검증한다. 첫 번째 클라이언트는 Red 공격자, 두 번째는 Blue 피해자다. 두 클라이언트가 HP 감소, 사망 1회, 킬·팀 점수 1 증가, HP 복구와 리스폰을 모두 관찰해야 PASS를 기록한다. 테스트 자동 조작은 Development에서 `-FPSBaselineSmoke`를 지정했을 때만 실행한다.

```powershell
# 단일 조건
.\Scripts\Start-FPSBaseline.ps1 -LatencyMs 100 -SmokeTest

# 보고서의 다섯 조건
.\Scripts\Test-FPSLatencySuite.ps1
```

지연 값은 양쪽 프로세스의 **송신 패킷당 추가 지연**이다. `-LatencyMs 100`은 서버→클라이언트 100ms, 클라이언트→서버 100ms를 추가하므로 왕복 지연은 대략 200ms 증가한다. 실제 핑에는 엔진 틱과 로컬 처리 시간도 포함된다. 패킷 손실과 지연 편차는 0으로 고정했다. 설정 기준은 [Epic Games 네트워크 에뮬레이션 문서](https://dev.epicgames.com/documentation/ko-kr/unreal-engine/using-network-emulation-in-unreal-engine?application_version=5.7)와 UE 5.7의 `FPacketSimulationSettings`이다.

각 실행의 `Server.log`, `Shooter.log`, `Observer.log`는 `Saved/FPSBaseline/<실행시각>-<지연>ms/`에 저장한다. 전체 조건 스크립트는 `Saved/FPSBaseline/LatencyResults.csv`도 생성한다. 이 검사는 결과 일치와 재진입을 확인하는 기능 테스트이며, 입력 지연의 정량 측정이나 이동 표적 명중률 비교는 별도 실험으로 진행한다.

Unreal Automation의 `LatencyCombat.Baseline`에는 서버 발사 간격, 초기 hitscan 피해, 조기 재장전 완료 차단, 탄약 이전, 클라이언트 상태 변경 차단, 중복 사망과 동시 리스폰 검증이 포함되어 있다. 에디터 Session Frontend에서 실행하거나 다음 명령으로 실행한다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' .\Latency_MultiCombat.uproject -unattended -nullrhi -nosplash -NoSound '-ExecCmds=Automation RunTests LatencyCombat.Baseline' '-TestExit=Automation Test Queue Empty'
```

새 테스트 맵을 생성해야 하는 체크아웃에서는 다음 명령을 사용할 수 있다. 이미 존재하는 `FPSBaseline` 맵은 덮어쓰지 않는다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' .\Latency_MultiCombat.uproject -EnablePlugins=PythonScriptPlugin -run=pythonscript "-script=$PWD\Scripts\CreateFPSBaselineMap.py" -unattended -nullrhi -NoSound
```
