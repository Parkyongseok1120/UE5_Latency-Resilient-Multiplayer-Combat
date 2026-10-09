# FPS Baseline 실행 결과 — 2026-10-10

최신 `main` 커밋 `9f57b3f461e6f8d7571ef781c1008f7aeb2e5ce9`에 이번 계측과 팀 스폰 수정 변경을 적용한 상태에서 새로 실행했다. 이전 체크아웃의 수치와 로컬 LLM 변경은 사용하지 않았다. 실행 방법과 지표 정의는 [검증 문서](FPSBaselineValidation.md)에 있다.

## 환경과 표본

- Windows, UE 5.7.4, Editor Development 모듈. 같은 PC에서 독립 Dedicated Server 프로세스 1개 + 클라이언트 2개.
- 기본 FPSBaseline 맵, 기본 캐릭터/무기와 무작위 spread, 서버 hitscan. 공격 Prediction / Reconciliation / Lag Compensation 미적용.
- 양쪽 송신에 동일한 PktLag, 지터/손실 0, 클라이언트 최대 60fps. 정량 실행은 서버와 클라이언트 모두 NullRHI.
- 조건별 3회 × 24개 순차 발사 = 72개, 전체 15회 / 360개. 기능 검증용 추가 발사는 통계에서 제외.
- 최초와 리스폰 후 일반 이동 입력으로 상대를 중앙선에 정렬하고 수평 속도가 안정된 뒤 발사. 발사별 실제 거리도 기록.

## 정량 결과

시간 단위는 ms다. 사격 응답은 전체 발사, 명중 확인과 HP 반영은 실제 피해를 준 발사만 집계한다. HP 반영은 공격자 Tick의 관찰 시점이며 HUD 픽셀 표시 시간을 뜻하지 않는다.

| 패킷 지연 | 추가 RTT | 발사 / 명중 | 명중률 | 사격 응답 평균 / P95 | 명중 확인 평균 | HP 반영 평균 / P95 |
|---:|---:|---:|---:|---:|---:|---:|
| 0 | 0 | 72 / 51 | 70.83% | 16.574 / 16.634 | 16.553 | 16.700 / 16.779 |
| 50 | 100 | 72 / 43 | 59.72% | 133.704 / 149.942 | 133.603 | 133.765 / 150.065 |
| 100 | 200 | 72 / 54 | 75.00% | 231.164 / 249.929 | 230.750 | 230.901 / 250.067 |
| 150 | 300 | 72 / 52 | 72.22% | 330.944 / 349.952 | 332.910 | 333.067 / 350.113 |
| 200 | 400 | 72 / 49 | 68.06% | 433.018 / 449.941 | 432.548 | 432.703 / 450.026 |

360개 발사의 거리는 1800.000~1800.017cm였다. 서버 승인을 기다리는 Baseline에서 왕복 지연이 사격 응답과 HP 반영 시간에 나타났다. 전송 주기·프레임·프로세스 스케줄링의 추가 시간이 포함된다.

명중률은 정지 표적과 기본 spread에서 관찰한 결과다. 조건별 3회이며 무작위 seed를 고정하거나 이동 표적을 비교하지 않았으므로, 조건 간 명중률 차이를 지연의 인과 효과로 해석하지 않는다. 손실·지터와 Lag Compensation 성능도 이번 결과로 판단할 수 없다.

원시 자료는 다음 로컬 경로에 있다. Saved는 Git에서 제외되며 재현 도구와 이 결과 요약은 저장소에 포함된다.

- [RawSamples.csv](../Saved/FPSBaseline/Measurements/20261010-015833-024/RawSamples.csv): 발사 ID, 응답 시간, HP 전후, 거리, 실행 로그 경로.
- [Summary.csv](../Saved/FPSBaseline/Measurements/20261010-015833-024/Summary.csv): 평균, P50/P95, 거리 범위.
- [Runs.csv](../Saved/FPSBaseline/Measurements/20261010-015833-024/Runs.csv): 15회 실행 결과와 양쪽 로그 경로.
- [Metadata.json](../Saved/FPSBaseline/Measurements/20261010-015833-024/Metadata.json): 환경, 기준 커밋과 소스/도구 해시.
- [CSVVerification.json](../Saved/Codex/Week4Latest/CSVVerification.json): 별도 Python 통계 재계산·ID·HP·스폰 방향·거리·로그 설정 대조 결과.

## 기능과 화면 검증

15회 모두 양쪽 클라이언트 PASS다. 상대 이동, 점프, HP 감소, 사망, 새로운 Pawn의 리스폰을 관찰했고, 공격자의 재장전 중 발사 차단/탄약 보존, 피해자의 사망 중 발사 차단, 킬·사망·팀 점수 일치를 확인했다.

Editor / Game Development 빌드는 성공했다. Unreal Automation은 6개 성공, 경고 0, 실패 0이다: HealthAuthority, FireAndReload, SimultaneousRespawn, CoverAndFriendlyFire, InitialTeamSpawn, Measurement.Correlation. 스폰 테스트는 InitNewPlayer → PostLogin, 캐릭터 생성 시 팀, 반대 팀의 캐시된 StartSpot을 가진 재시작 경로를 확인한다.

별도의 100ms GPU 실행도 양쪽 PASS다. 로그는 `Saved/FPSBaseline/20261010-015727-156-100ms-benchmark`에 있다. [Red 화면](../Saved/Codex/Week4Latest/Shooter.png)과 [Blue 화면](../Saved/Codex/Week4Latest/Observer.png)에서 반대편 팀 색상 캐릭터와 두 플레이어의 HUD 목록을 확인했다. 이 실행의 표본은 위 NullRHI 통계에 포함하지 않았다.

물리 키보드·마우스 포커스와 ADS/연사 조작감은 수동 확인 항목으로 남겨 둔다. 자동 입력을 수동 플레이 완료로 보고하지 않는다. M2의 후속 예측/보정은 [설계](CombatPredictionDesign.md)까지 정리한 상태다.
