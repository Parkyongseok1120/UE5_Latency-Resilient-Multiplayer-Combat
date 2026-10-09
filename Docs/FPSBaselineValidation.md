# M1 / M4 FPS Baseline 검증과 계측

최신 기준 커밋은 `9f57b3f461e6f8d7571ef781c1008f7aeb2e5ce9`이다. 이전 체크아웃의 작업과 측정 결과를 복원하지 않고 이 커밋에서 다시 구현하고 실행했다. 결과 수치는 [FPSBaselineResults.md](FPSBaselineResults.md)에 기록한다.

## 재실행

UE 5.7 Editor와 Game Development 타깃을 빌드한 뒤 프로젝트 루트에서 실행한다. 소스만 바꾼 경우 실행 중인 PIE를 중지하고 Editor 타깃을 다시 빌드한다.

```powershell
# Dedicated Server + 직접 조작할 클라이언트 2개
.\Tools\Start-FPSBaseline.ps1 -LatencyMs 100 -ShowWindows

# 자동 조작을 GPU 화면으로 확인하고 양쪽 프레임 저장
.\Tools\Start-FPSBaseline.ps1 -LatencyMs 100 -Benchmark -CaptureFrames -ShowWindows

# 각 지연 조건 3회, 매번 24개 발사 표본
.\Tools\Measure-FPSBaseline.ps1
```

`-EngineRoot`, `-Port`, `-Repeats`, `-Samples`, `-Latencies`로 환경과 반복 수를 지정할 수 있다. 일반 Benchmark는 클라이언트도 `-nullrhi`로 실행하며, CaptureFrames 또는 ShowWindows를 지정하면 렌더링한다. Launcher 엔진의 `UnrealEditor.exe -server` 독립 프로세스를 사용한다. 배포용 Dedicated Server 패키징은 별도 작업이다. 도구는 해당 호출이 생성한 프로세스만 정리한다.

## 자동 기능 검증

실제 게임의 `AddMovementInput`, `Jump`, `FireWeapon`, `StartReload` 경로로 조작한다. 좌표 강제 이동이나 테스트 피해 RPC는 사용하지 않는다. 초기 소유권·접속·지상 상태가 준비된 후 이동·점프하고 중앙선에 정렬한다. Red가 Blue를 조준해 순차 발사하며 두 클라이언트가 상태를 관찰한다.

| 점검 | 확인 방법 |
|---|---|
| 서로 같은 서버에 접속 | GameState의 Red/Blue PlayerState와 상대 Pawn 확인 |
| 이동 / 점프 | 상대 좌표 변화와 로컬 점프의 양의 수직 속도 |
| 발사 / HP 복제 | 실제 ServerFire와 서버 effects/hit 응답, 양쪽 HP 감소 |
| 재장전 | 시작 상태 관찰, 재장전 중 발사 차단, 완료 후 탄약 총량 보존 |
| 사망 / 점수 | 양쪽 사망·킬 관찰, Red 점수와 Blue 사망 횟수 일치 |
| 사망 중 발사 | Blue의 Dead 상태에서 발사 불가, 탄약 변화 없음 |
| 리스폰 | 사망 후 새로운 Pawn, 양의 HP와 생존 상태 확인 (다음 발사가 오면 100 HP 스냅샷을 놓칠 수 있음) |
| 엄폐 / 아군 피해 | Unreal Automation의 서버 hitscan 검증 |
| 초기 팀 스폰 | InitNewPlayer → PostLogin 경로로 팀 배정·캐시·실제 Red/Blue 스폰 위치 확인 |

두 클라이언트 모두 PASS를 남겨야 실행이 통과한다. 요청한 표본 수를 얻은 뒤 사망/리스폰이 아직 없으면 기능 검증용 추가 발사를 진행한다. `VALIDATION` 로그는 통계에서 제외한다. 표본 중 miss는 완료한 발사로 포함하고, HP와 명중 확인 지연은 빈 값으로 저장한다.

## 지표 정의

모든 시간은 공격자 프로세스의 `FPlatformTime::Seconds()` 단조 시계를 사용한다. 서로 다른 컴퓨터/프로세스 시계를 빼지 않는다. 발사 ID로 effects와 명중 확인 응답을 연결하고, 한 번에 하나의 발사만 대기해 HP 변화의 대응을 확인한다. 피해를 주는 공격자는 한 명이며, 순차 발사와 정지한 상대를 사용한다. 최초와 리스폰 후 모두 일반 이동 입력으로 상대를 중앙선에 정렬하고, 상대의 횡방향 좌표와 수평 속도가 안정된 뒤 발사한다. 발사마다 상대까지의 거리도 `RangeCm`으로 기록한다. 기본 무작위 spread는 유지한다.

- **FeedbackMs**: 발사 요청 직전 → 서버 승인 MulticastShot을 공격자 클라이언트가 수신한 시점. 로컬 사격 시각 효과가 실행되는 코드 지점의 지연이다. 화면 픽셀 표시나 물리적 마우스 입력 지연은 포함하지 않는다.
- **HitConfirmMs**: 같은 시작 → 서버 ClientConfirmHit 수신. 플레이어에게 실제 피해를 준 발사만 집계한다.
- **HealthMs**: 같은 시작 → 공격자 클라이언트의 Tick에서 상대 HP 감소를 처음 관찰. 속성 수신·틱 관찰 시점의 지연이며 HUD 픽셀 표시 시점은 아니다.
- **HitRatePercent**: 서버가 플레이어 피해를 적용한 측정 발사 / 전체 측정 발사. 기본 무기 spread와 거리, 정지 표적 조건이 포함된다. 이동 표적의 지연 보상 정확도를 뜻하지 않는다.

`LatencyMs`는 서버와 클라이언트 각각의 송신 패킷 추가 지연이다. 100ms 조건은 대략 200ms 추가 RTT다. 지터와 손실은 0, 클라이언트 최대 프레임은 60으로 고정한다. 같은 PC에서 독립 서버 프로세스 1개와 클라이언트 2개를 실행한다. 전송 주기·프레임·스케줄링이 추가되므로 수치는 설정 RTT와 정확히 같지 않다. `P50`, `P95`는 정렬된 표본에서 nearest-rank로 계산한다. hit 지표의 표본 수는 전체 표본보다 작다.

`Saved/FPSBaseline/Measurements/<시각>/` 아래 `RawSamples.csv`, `Runs.csv`, `Summary.csv`를 생성한다. 각 행의 Logs 경로에서 양쪽 PASS와 네트워크 설정을 추적할 수 있다. 측정 중 다른 빌드·테스트를 겹치지 않는다.

## 직접 조작할 때 남은 체크

자동 입력과 GPU 캡처는 실제 키보드/마우스의 입력 포커스와 조작감을 확인하지 않는다. 각 0/50/100/150/200ms에서 다음 항목을 수동 체크하고 기록한다.

- 두 창을 번갈아 클릭한 뒤 WASD·마우스·점프가 해당 창에만 적용되는지 확인.
- 우클릭 ADS의 FOV 변화, 좌클릭 누르기/떼기 연사, R 재장전, 사망 후 입력 복구 확인.
- 엄폐물을 사이에 둔 사격과 리스폰 후 상대 팀 색상·HUD 일치 확인.
- 체감 지연, 문제 재현 조건, 각 조건별 결과를 기록. 자동 통과와 수동 확인을 구분해서 보고한다.
