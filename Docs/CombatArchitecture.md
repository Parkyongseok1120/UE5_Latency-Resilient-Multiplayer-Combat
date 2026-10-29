# 전투 구조와 상태 소유권

프로젝트 접두어를 C++ 클래스·파일명에서 제거한다. Unreal 모듈명 `Latency_MultiCombat`, API export 매크로와 빌드 타깃은 엔진 식별자로 유지한다. 기존 여섯 native 클래스의 이름은 `Config/DefaultEngine.ini`의 CoreRedirects로 새 이름에 연결한다.

## 객체 구조

```mermaid
flowchart TD
    Character[CombatCharacter: 이동·Pawn 생명주기] --> Weapon[CombatWeapon: 별도 복제 액터]
    Character --> History[RewindHistoryComponent: 서버 캡슐·원점 이력]
    Character --> Input[CombatInputComponent: Enhanced Input 바인딩]
    Input --> InputData[CombatInputDataAsset: 액션·키 매핑]
    Character --> CharacterVisual[CharacterPresentationComponent: 카메라·3인칭 스켈레탈/팀 색상 표현]
    Weapon --> Fire[WeaponFireComponent: 발사 명령·ADS·상태·RPC]
    Weapon --> Hit[WeaponHitValidationComponent: 현재·과거 명중 판정]
    Weapon --> Ammo[WeaponAmmoComponent: 탄약·재장전]
    Weapon --> WeaponVisual[WeaponPresentationComponent: 1P/3P 스켈레탈·스태틱 메시·소리·명중 시각]
    PlayerController[CombatPlayerController: 로컬 시계·계측·입력 모드] --> TimeSync[CombatTimeSyncComponent: RTT·서버 시계 추정]
    PlayerController --> Telemetry[CombatTelemetryComponent: 로컬 RTT·발사 판정 링버퍼]
    HUD[CombatHUD: 순수 뷰 렌더러] -. 조회 .-> Telemetry
    PlayerState[CombatPlayerState: 팀·K/D] --> Health[HealthComponent: HP·방어구·사망]
    GameMode[CombatGameMode: 경기 규칙·점수 처리] --> TeamSpawn[TeamSpawnComponent: 팀 배정·스폰 선택]
    GameMode --> Respawn[RespawnComponent: 개별 타이머·Pawn 교체]
```

Character가 무기 액터를 서버에서 생성하고 Owner와 Instigator를 자신으로 지정한다. 무기 액터의 Owner → Pawn → PlayerController 연결로 컴포넌트의 Server/Client RPC를 라우팅한다. 무기는 카메라에 부착하며 Pawn 종료 시 함께 제거한다. 리스폰은 새 Pawn과 새 무기 액터를 만든다. 현재 범위에는 드롭·인벤토리·무기 교체를 포함하지 않는다.

## SSOT

| 정보 | 설정 출처 / 런타임 소유자 |
|---|---|
| 캐릭터 설정·외형 | `UCharacterDataAsset` (`DA_DefaultCharacter` 기본값); 미지정 시 `FCharacterStats` 및 기본 원통 프록시 |
| 무기 설정·외형 | WeaponClass의 `CombatWeapon::WeaponData` → CharacterData의 `DefaultWeapon` (`DA_DefaultWeapon`) → `FWeaponStats` 및 기본 큐브 |
| 입력 설정 | `CombatInputDataAsset`; 미지정 시 동일 타입의 런타임 기본 매핑 |
| 검증된 무기 스탯 | `CombatWeapon::Stats`; 모든 무기 컴포넌트가 동일한 스냅샷 조회 |
| 설정 조회·보정, 거리 감쇠, 단위 변환 | `FCombatRules` static 함수 |
| 로컬 실시간 계측·그래프 버퍼 | PlayerController의 `CombatTelemetryComponent` (비복제 로컬 전용; `CombatHUD`가 읽기 전용 조회) |
| 실제 탄약·재장전 예약 | 무기의 `WeaponAmmoComponent` |
| 발사 간격·발사 번호·ADS·전투 상태 | 무기의 `WeaponFireComponent` |
| HP·방어구·사망, 스폰 HP/방어구 용량 | PlayerState의 `HealthComponent` |
| 팀·킬·사망 횟수 | `CombatPlayerState` |
| 팀 점수·경기 종료·승리 팀 | `CombatGameState` |
| 리스폰 타이머 | GameMode의 `RespawnComponent` |
| 스폰 충돌 크기 | 선택된 DefaultPawnClass의 실제 캡슐; 스폰 검사에 별도 크기 상수 없음 |

`CharacterData`는 캐릭터 설정이다. 무기 스폰 에셋 override는 별도 무기 클래스의 `WeaponData`에서 지정한다. Character의 `CurrentWeapon`, 탄약/발사/재장전 중계 API와 PlayerState의 HP 중계 API를 제거했다. 소비자는 무기·Health 컴포넌트를 직접 조회한다. `DefaultCharacterClass`는 제거하고 엔진의 `DefaultPawnClass`를 사용한다.

입력은 Enhanced Input 한 경로를 사용한다. 이전 `Legacy*`, BindAxis/BindKey, DefaultInput.ini의 AxisMappings, PlayerController의 매핑 설정과 Character의 개별 IA 필드를 제거했다. `CombatInputComponent::InputData`에 액션과 매핑을 함께 지정한다. 미지정 시 WASD/Mouse/Space/LMB/RMB/R의 기본 액션·매핑을 생성한다. 기존 마우스 축의 0.07 감도와 MouseY 반전을 modifier로 유지한다. Pawn 종료 시 해당 입력 바인딩과 매핑 컨텍스트를 해제한다.

방어구 흡수 비율은 캐릭터 Stats의 `ArmorAbsorptionRatio`로 설정한다(기본 0.5). `ResetForRespawn`도 마지막 스폰의 에셋 설정에 따른 HP·방어구를 복원한다. 원래 게임의 Pawn 재생성 경로는 동일하게 스폰 에셋으로 초기화한다.

## 복제와 경계

- 무기 액터, Fire의 CombatState/ADS는 소유 Pawn의 relevancy를 따른다.
- 탄약은 Ammo 컴포넌트에서 `COND_OwnerOnly`로 복제한다.
- Health 컴포넌트는 PlayerState에 부착하므로 Pawn 교체와 무관하게 유지되며 모든 클라이언트에 복제한다.
- HP·탄약 쓰기는 소유 액터의 서버 권한을 확인한다. 클라이언트는 조준 방향·발사 번호·생명 ID·환산한 발사 시각만 전달한다.
- 서버 승인 이후 effects/hit 응답을 보내는 기존 Baseline 동작을 유지한다. Lag Compensation은 이력·독립 판정 컴포넌트로 연결했다. 로컬 Prediction/Reconciliation은 후속 작업이다.
- 계측은 Pawn과 무기가 각각 도착하는 순서를 가정하지 않고, 실제 Fire 컴포넌트가 바뀌면 delegate를 재연결한다.
- PlayerController에는 입력 모드와 테스트 컴포넌트 구성만 남긴다. smoke 자동 조작은 `CombatSmokeComponent`, 정량 계측은 `CombatBenchmarkComponent`가 담당한다.
- 타이머·스폰 관찰 콜백은 이름 있는 함수와 UObject/raw delegate 바인딩을 사용한다. C++ 소스에 람다를 사용하지 않는다.
- 객체 포인터와 컨테이너 순회 변수는 명시 타입을 사용한다. `const`와 참조 여부를 유지하고 `auto`로 변수 타입을 숨기지 않는다.
- `.clang-format`을 기준으로 C++ 줄 배치와 들여쓰기를 통일한다. Unreal generated 헤더 순서를 유지하기 위해 include 정렬은 비활성화한다.

## API 이전

| 기존 접근 | 새 접근 |
|---|---|
| Character.CombatState | Character.GetCombatState() 또는 Weapon.Fire.GetCombatState() |
| Character.MagazineAmmo / ReserveAmmo | Weapon.Ammo.GetMagazineAmmo() / GetReserveAmmo() |
| Character.FireWeapon / StartReload / StopReload | Weapon.Fire.FireWeapon / Weapon.Ammo.StartReload / FinishReload |
| Character.CurrentWeapon | WeaponClass의 WeaponData 또는 CharacterData.DefaultWeapon |
| Character.IA_* / PlayerController.CombatMappingContext | Character.CombatInput.InputData의 액션과 MappingContext |
| Character.OnShotSubmitted / OnShotFeedback / OnHitConfirmed | Weapon.Fire의 동일 delegate |
| Character.ServerFire / ServerStartReload | Weapon.Fire.ServerFire(FCombatShotCommand) / Weapon.Ammo.ServerStartReload |
| PlayerState.CurrentHealth / CurrentArmor / MaxHealth / bIsDead | PlayerState.Health.GetHealth() / GetArmor() / GetMaxHealth() / IsDead() |
| Character.FirstPersonWeapon | Character.GetWeapon().WeaponMesh |
| GameMode.DefaultCharacterClass | GameMode.DefaultPawnClass |

CoreRedirects는 native 부모 클래스의 이름 변경을 처리한다. 제거된 필드를 직접 참조하는 사용자 Blueprint 노드는 위 getter나 컴포넌트 접근으로 이전해야 한다. 저장소의 기존 BP_Default는 Engine.Pawn 기반이며 그대로 로딩 가능한지 확인한다. 기존 native Character 경로가 새 클래스로 리다이렉트되는지도 별도 검사한다.

스폰 지점이 모두 점유되었을 때의 엔진 fallback, TeamSize의 인원 제한 미적용, 도착 시각 기준 연사 제한은 이번 구조 변경에서 정책을 바꾸지 않는다.

## 검증

기존 `LatencyCombat` 테스트는 컴포넌트 getter를 통해 동일 게임 동작을 검사한다. 구조 테스트는 별도 무기 액터의 소유권·부착·복제·수명, 클라이언트 탄약 변경 차단, 기존 Blueprint 로딩과 native 클래스 경로 리다이렉트, 기본 입력 방향·감도와 비기본 에셋의 HP/방어구/탄약/재장전 설정을 확인한다. 테스트는 런타임 탄약을 직접 덮어쓰지 않고 에셋 입력으로 조건을 구성한다.

2026-10-11 macOS / UE 5.7.4 검증:

- `Latency_MultiCombatEditor Mac Development` 빌드 성공.
- `LatencyCombat` 자동화 테스트 9/9 통과.
- 별도 서버와 클라이언트 2개: PktLag=0 smoke 양쪽 PASS.
- PktLag=50ms(왕복에 약 100ms 추가) benchmark: 24발, 양쪽 PASS. 이동·점프·HP/점수 복제·재장전·리스폰 및 관찰 클라이언트의 사망 후 발사 차단 확인.
- clang-format dry-run과 git diff --check 통과.

네트워크 로그는 `Saved/FPSBaseline/RefactorVerification/`에 저장한다. 위 검증은 구조 변경의 회귀 검사다. 기존 360표본 결과를 다시 실행하거나 이동 표적의 Lag Compensation 성능을 검증한 결과는 아니다. 입력 테스트는 액션 매핑과 modifier의 방향·감도를 검사하며 실제 화면 렌더링과 물리 입력 지연은 측정하지 않는다.

## 6주차 추가 경계

`DA_CombatNetwork → GameMode → GameState.LagCompensation`이 네트워크 수치의 설정·검증·복제 경로다. PlayerController의 `CombatTimeSyncComponent`가 클라이언트 시계를 서버 기준으로 추정한다. Character의 `RewindHistoryComponent`는 서버에서만 기록하고, 새 Pawn마다 생명 ID와 버퍼를 새로 만든다.

Fire는 명령 순서·시각·전투 상태를 검사하고 HitValidation에 판정을 맡긴다. Ammo의 실제 차감과 재장전, Character/Health/GameMode의 피해 권한과 점수 처리는 재사용한다. 실제 액터를 과거 위치로 옮기지 않는다. 승인·거절과 명중 확인은 Reliable `ClientShotResult`로 전달하며 시각 효과는 기존 multicast를 사용한다.

- 상세 시간 정책과 현재 월드 동적 차폐물 경계는 [Lag Compensation 문서](LagCompensationPlan.md), Win64 빌드·자동화·네트워크·렌더링 결과는 [6주차 검증](Week6Validation.md)에 있다.

## 외형 에셋(1P/3P 스켈레탈 메시) 및 우측 HUD 텔레메트리 경계

- **캐릭터·총기 외형 SSOT**: `/Game/Data/DA_DefaultCharacter`와 `/Game/Data/DA_DefaultWeapon`이 언리얼 5.7 기본 마네킹(`SKM_Manny_Simple` + `ABP_CombatRifle`)과 라이플(`SKM_Rifle` + 사격음·`MM_Rifle_Fire`)을 연결한다. `Tools/CreateCombatDefaultAssets.py`는 기본 AnimBP를 복제해 라이플 대기·이동·점프와 `DefaultSlot`을 구성한다. 메시의 상대 Z=-90, Yaw=-90은 생성 시점과 BeginPlay에 적용하고 `CacheInitialMeshOffset`도 갱신하여 클라이언트 이동 보정이 이를 유지하게 한다. Python `Rotator`는 축을 키워드 인자로 지정한다. `Character->GetMesh()`는 시각 전용(`NoCollision`, `OwnerNoSee`)으로 분리되어 서버 Hitscan·리와인드 캡슐 판정 경계를 침범하지 않는다.
- **1P/3P 무기 부착과 수명**: `ACombatWeapon`의 루트(`WeaponRoot`)는 `FirstPersonCamera`에 부착되고, 1인칭 메시(`FirstPersonMesh`, `OnlyOwnerSee`)와 3인칭 메시(`ThirdPersonMesh`, `OwnerNoSee`, 캐릭터 `GetMesh()` 오른손 소켓 부착)의 가시성·총구 소켓·사망 처리는 `UWeaponPresentationComponent`가 전담한다.
- **발사 표현**: 기존 승인 발사 `MulticastShot → PlayShot` 경로가 캐릭터의 `DefaultSlot`에서 사격 몽타주를 재생하고, 로컬 1인칭 무기에만 반동과 복귀 보간을 적용한다. 반동 컴포넌트는 복귀 완료 후 Tick을 끈다.
- **1인칭 표현 범위**: 현재 1인칭은 총기 메시와 반동이며 팔 메시는 없다. `MM_Rifle_Fire`는 상대가 보는 3인칭 캐릭터의 사격 애니메이션이다.
- **상하 조준·왼손 IK**: `ABP_CombatRifle`의 네이티브 부모 `UCombatAnimInstance`가 기존 그래프 평가 후 허리·척추·목·머리·쇄골에 조준 보정을 분배하고 왼팔 Two Bone IK를 적용한다. 로컬 `ControlRotation`과 `GetBaseAimRotation()`을 통한 엔진의 원격 `RemoteViewPitch16`을 읽고 ±80도로 제한·보간한다. 기본 `MF_Rifle_Idle_ADS`와 `HandGrip_R` 부착은 정면 시선에서도 총구가 캐릭터 기준 Pitch +11.886°, Yaw +8.224°를 향하므로, `ReferenceAimRotation`의 역회전으로 이 오차를 제거한 뒤 목표 Pitch를 적용한다. 이 quaternion 차이를 메시 컴포넌트 공간으로 옮겨 본 가중치만큼 분배하므로 캐릭터가 어느 방향을 바라봐도 같은 정렬을 유지한다. 원본 포즈·무기 부착을 교체하면 `ReferenceAimRotation`도 조정해야 한다. 무기의 `LeftHandGripSocketName`과 `LeftHandGripOffset`이 왼손 목표를 정의하며 기본 라이플은 `GripPoint_002`와 `HandGrip_L`의 역변환을 사용해 손바닥을 손잡이에 맞춘다. 목표는 이번 프레임의 오른손 포즈를 기준으로 계산하므로 반동·조준 중 이전 프레임의 무기 위치를 따라가는 지연을 피한다. 재장전 중에는 왼손 IK를 풀고, 소켓이 없는 무기는 IK를 생략한다. 본 보정은 시각 포즈에만 적용하며 캡슐·서버 명중 판정·RPC는 변경하지 않는다. `LatencyCombat.Animation.ViewDirectionAlignment`는 정면·상하 조준과 여러 캐릭터 Yaw에서 기본값 대비 변화량 대신 최종 총구와 시선 사이 절대 각도를 검사한다.
- **외형 변경 검증**: C++ 데이터 에셋 클래스나 기본 에셋을 변경한 뒤에는 에디터를 완전히 종료하고 다시 연다. 기존 세션에서 Hot Reload만 하면 이전 회전값이 남거나 `HOTRELOAD_WeaponDataAsset` 타입 충돌로 클라이언트가 무기 에셋을 거부할 수 있다. `-FPSBaselineBenchmark -FPSPresentationCheck`로 시작한 새 클라이언트는 상대의 기립 자세·Z=-90·무기 복제·실제 사격 슬롯 재생을 검사하고 `Saved/CombatPresentation/Network`에 화면을 저장한다.
- **실시간 HUD 그래프와 상태 분리**: `UCombatTelemetryComponent`가 로컬 `PlayerController`에서 RTT/프레임 시계열과 최근 발사별 `AckMs`·`ShotAgeMs`·`TargetAgeMs`·승인/거절 통계를 수집하며, 리스폰으로 새 Pawn·무기가 생성되면 이전 미응답 슬롯만 정리하고 델리게이트를 자동 재연결한다. `ACombatHUD`는 상태를 갖지 않고 우측 패널에 서버 상태 수치·실시간 RTT 선 그래프·발사별 리와인드 막대그래프를 렌더링한다.
