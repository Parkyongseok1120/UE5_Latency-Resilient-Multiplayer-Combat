# 서버 리와인드 기반 Hitscan Lag Compensation

2026-10-22 기준 M3 구현을 완료했다. 기존 캐릭터·무기·탄약·표현의 책임 분리를 유지하고, 이력 기록과 명중 판정을 별도 컴포넌트로 연결했다. 실제 서버·클라이언트 비교 결과는 [6주차 검증 결과](Week6Validation.md)에 정리한다.

## 설정과 상태 소유권

`Content/Data/DA_CombatNetwork`가 기본 네트워크 조정 에셋이다. 최대 발사 시각 보정 구간, 이력 기록 주기, 미래 시각 허용 오차, 표적 표시 지연, 최대 보간 거리와 시각 동기화 주기를 조정한다. 기본 GameMode가 이 에셋을 참조한다. 다른 GameMode Blueprint에서는 `NetworkData`를 교체하면 된다. 서버가 검증한 설정 스냅샷만 GameState에 복제한다.

| 담당 | 소유하는 정보와 처리 |
|---|---|
| `CombatTimeSyncComponent` / PlayerController | RTT 중간 시점으로 서버 시계 추정, 실제 관측 RTT |
| `RewindHistoryComponent` / Character | 시각·캡슐·카메라 원점·속도·팀·생존 상태·생명 ID의 제한 크기 ring buffer |
| `WeaponFireComponent` / Weapon | 발사 명령 검증, 처리 번호, 서버 발사 간격, 실제 탄약 소비, 승인·거절 응답 |
| `WeaponHitValidationComponent` / Weapon | 현재 월드 trace 또는 과거 캡슐 교차, 피해 직전 생명 상태 확인 |
| 기존 Ammo / Health / GameMode | 재장전·탄약, HP·방어구, 팀 피해 규칙·점수·리스폰 |

`FCombatRules` static 함수가 캐릭터·무기 수치 검증과 거리 감쇠를 공유한다. 프로젝트 C++ 소스의 namespace는 static struct 또는 파일 내부 static 함수로 바꿨다. 주요 처리에는 짧은 한국어 주석을 남겼다.

## 발사 처리

`CombatInput → Weapon.Fire → ServerFire(FCombatShotCommand) → ValidateCommand → HitValidation.Resolve → Ammo.ConsumeRound → HitValidation.ApplyValidatedDamage → Character.TakeDamage / Health → ClientShotResult` 순서로 실행한다.

명령에는 발사 번호, Pawn 생명 ID, 서버 시계로 환산한 입력 시각과 조준 방향만 들어간다. 원점·명중 대상·피해량은 서버가 결정한다. 서버는 소유 캐릭터, 생명 ID, 증가하는 번호, 유한한 방향·시각, 시각 구간과 역행, 재장전·탄약·사망·경기 종료, 서버 도착 간격과 명령 시각의 발사 간격을 검사한다. 거절된 새 번호도 처리 번호에 포함해 재사용을 막는다. 승인·거절은 소유 클라이언트에 Reliable 응답으로 전달하며, 표현 multicast는 기존처럼 Unreliable이다.

기본 `MaxRewindSeconds=0.75`는 요청 시각의 최대 나이다. 원점은 해당 시점의 서버 카메라 이력에서 구한다. 표적 시각은 `발사 시각 - min(서버가 관측한 RTT/2 + DisplayDelaySeconds, MaxTargetViewDelay)`다. 기본 BodyProxy는 capsule에 부착돼 mesh 스무딩이 적용되지 않으므로 추가 표시 지연은 0이다. 별도 스무딩을 적용할 때만 에셋에서 지연을 맞춘다. 호스트의 로컬 발사에는 원격 수신 지연을 더하지 않는다.

이력은 최대 발사 시각 구간과 표적 표시 지연을 합친 길이에 맞춰 보관하며, 최대 512프레임으로 제한한다. 구간 경계 조회와 프레임 보간을 지원한다. 미래·보관 구간 밖 조회, 생명·팀·생존 상태 경계, 큰 위치 불연속 사이 보간은 허용하지 않는다. 리스폰마다 새 Pawn·무기·생명 ID·이력을 사용한다.

판정 중 실제 액터나 충돌체를 이동시키지 않는다. 현재 캐릭터를 제외한 월드 차폐 trace와 과거 캡슐의 최근접 교차 거리를 비교한다. 아군 캡슐도 총알을 막으며, 아군 피해 여부는 GameMode가 결정한다. 동적 엄폐물은 현재 서버 월드를 사용한다. 과거 동적 엄폐물, 본별 hitbox와 headshot은 현재 범위 밖이다.

피해 직전 현재 대상의 유효성·컨트롤러·생명 ID·생존 상태를 재검사한다. Character의 기존 피해 경로에서 팀·경기 상태도 재확인한다. 탄 퍼짐은 클라이언트 번호 대신 서버 처리 순서로 결정한다. 비교 실험에서는 같은 순서의 발사에 같은 spread seed를 사용한다.

## 실행

```powershell
# 단일 이동 표적 비교
.\Tools\Start-FPSBaseline.ps1 -RewindBenchmark -LatencyMs 100
.\Tools\Start-FPSBaseline.ps1 -RewindBenchmark -LatencyMs 100 -DisableLagCompensation

# 다섯 지연 조건 × on/off × 24발
.\Tools\Measure-Rewind.ps1

# 반복 수 확대
.\Tools\Measure-Rewind.ps1 -Repeats 3

# 화면 캡처와 프레임/GPU 시간
.\Tools\Start-FPSBaseline.ps1 -RewindBenchmark -LatencyMs 100 -CaptureFrames
```

측정 도구는 서버 하나와 클라이언트 둘을 실행하고, 생성한 프로세스만 정리한다. `-DisableLagCompensation`은 서버 기능과 복제 설정에 반영된다. 일반 플레이에도 사용할 수 있다. 테스트 동작은 Development에서 명시적 `-FPSRewindBenchmark` 옵션이 있을 때만 활성화된다.

에셋이 없는 소스 체크아웃에서는 `Tools/CreateCombatNetworkAsset.py`를 UE Python commandlet으로 실행할 수 있다. 기존 에셋은 덮어쓰지 않는다.

Prediction/Reconciliation의 로컬 효과·표시 탄약 예측은 [M2 설계](CombatPredictionDesign.md)의 후속 범위다. 이번 차주 계획의 M2 항목인 책임 분리와 데이터 에셋화는 기존 구조를 재사용했다.
