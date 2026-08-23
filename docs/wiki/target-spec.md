# 목표 사양서

fl-server가 **충족해야 할 것**을 요구사항 단위로 규정한다.

> [structure.md](structure.md)는 지금 무엇이 있는지, [target-structure.md](target-structure.md)는
> 목표가 어떻게 생겼는지를 그린다. **이 문서는 무엇을 만족해야 하는지를 규정한다.**
> 셋의 관계: 구성도(현재) → 청사진(목표 형태) → **사양서(목표 조건)**.

---

## 0. 이 문서 읽는 법

### 0.1 요구사항 등급

| 등급 | 뜻 |
|---|---|
| **MUST** | 충족하지 못하면 사양 미달. 근거가 되는 결함이나 불변식이 있다 |
| **SHOULD** | 강한 권고. 안 하기로 정했으면 [decisions.md](decisions.md)에 X-번호로 반려를 남긴다 |
| **MAY** | 선택. 안 해도 사양 충족 |
| **조건부** | 미결 질문(§14)의 답에 달렸다. 답이 나오기 전엔 구현하지 않는다 |

### 0.2 요구사항 ID 체계

| 접두 | 영역 | 절 |
|---|---|---|
| `SRV` | 서버 / 프로세스 생명주기 | §2 |
| `NET` | 네트워크 / 소켓 / 연결 | §3 |
| `PRO` | 프로토콜 / 프레이밍 / 검증 | §4 |
| `THR` | 스레드 / 동시성 / 소유권 | §5 |
| `BLD` | 빌드 / 프로그램 / 의존성 | §6 |
| `FIL` | 폴더 / 파일 / 명명 | §7 |
| `CLS` | 클래스 / 인터페이스 | §8 |
| `RES` | 자원 / 버퍼 / 메모리 | §9 |
| `OBS` | 로그 / 메트릭 / 관측 | §10 |
| `ERR` | 오류 / 절단 처리 | §11 |
| `TST` | 테스트 | §12 |

**번호는 재사용하지 않는다.** 폐기된 요구사항은 지우지 않고 `폐기`로 표시한다.

### 0.3 검증 방법 표기

모든 MUST 요구사항에는 검증 방법이 붙는다. **검증할 수 없는 요구사항은 요구사항이 아니다.**

| 표기 | 뜻 |
|---|---|
| `컴파일` | `static_assert` 또는 타입 시스템이 강제. 어기면 빌드가 깨진다 |
| `단위` | `Tests.exe`의 테스트 케이스가 검증 |
| `실행` | `DummyClient` 또는 `fl-client`로 시나리오 재현 |
| `부하` | 다중 접속 부하 테스트에서 측정 |
| `검토` | 사람이 코드를 읽고 확인 (마지막 수단) |

---

## 1. 범위

### 1.1 대상

Windows x64 단일 프로세스 룸형 MO 게임 서버. 클라이언트는 별도 저장소의
`fl-client`(DirectX11 자체 엔진).

### 1.2 목표 수준

**"단일 서버 데모를 출시 가능한 품질로."**
분산·샤딩·무중단 배포는 이 사양의 범위 밖이다 ([limits.md](limits.md)).

「출시 가능한 품질」을 이 사양서는 다음으로 정의한다.

| 조건 | 어디서 규정 |
|---|---|
| 악의적 입력으로 서버가 멈추거나 잘못 읽지 않는다 | §4 PRO |
| 데이터가 조용히 손상되지 않는다 — 손상 대신 명시적 절단 | §11 ERR |
| 동시성 규칙이 관례가 아니라 코드로 강제된다 | §5 THR |
| 상태를 로그가 아니라 숫자로 안다 | §10 OBS |
| 회귀를 사람 눈이 아니라 테스트가 잡는다 | §12 TST |

### 1.3 이 사양이 규정하지 않는 것

- 게임 규칙 (전투·아이템·레벨) — Q2 미정
- 클라이언트 구현
- 와이어 포맷 변경 — 현행 유지가 전제 (§4.1)

---

## 2. 서버 사양 (SRV)

### 2.1 프로세스

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| SRV-001 | MUST | 서버는 단일 프로세스로 동작한다 | D-001 | 검토 |
| SRV-002 | MUST | 실행 파일은 `GameServer.exe` 하나다 | — | 검토 |
| SRV-003 | MUST | 기동 실패 시 0이 아닌 종료 코드를 반환한다 | 신규 | 실행 |
| SRV-004 | MUST | 포트 바인딩 실패를 `error`로 로그하고 즉시 종료한다 | 신규 | 실행 |

> **SRV-003의 이유:** 현재 `main`은 `Init`/`BindandListen` 실패를 무시하고 계속 진행한다.
> 포트가 이미 사용 중이어도 "서버 시작" 로그가 찍힌다. CI나 스크립트에서 기동 성공을
> 판별할 수 없다.

### 2.2 설정

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| SRV-010 | MUST | 런타임 설정은 `server.json`에서 읽는다 | R-015 | 실행 |
| SRV-011 | MUST | 설정 파일이 없으면 내장 기본값으로 기동하고 `warn`을 남긴다 | 신규 | 실행 |
| SRV-012 | MUST | 설정 값은 기동 후 변경되지 않는다 (읽기 전용) | THR-020 | 검토 |
| SRV-013 | MUST | 범위를 벗어난 설정값은 거부하고 기동을 중단한다 | 신규 | 단위 |

**설정 항목과 허용 범위**

| 키 | 타입 | 기본 | 허용 범위 | 근거 |
|---|---|---:|---|---|
| `Port` | UINT16 | 9000 | 1024–65535 | — |
| `MaxSession` | UINT32 | 100 | 1–10000 | 조건부 Q4 |
| `IoWorkerCount` | UINT32 | 4 | 1–64 | — |
| `RoomCount` | UINT32 | 10 | 1–1000 | 조건부 Q3 |
| `RoomCapacity` | UINT32 | 4 | 2–64 | 조건부 Q3 |
| `TickHz` | UINT32 | 30 | 1–120 | — |
| `IdleTimeoutSec` | UINT32 | 60 | 5–3600 | NET-030 |
| `RecvAssemblerSize` | UINT32 | 8192 | 1024–1048576 | RES-002 |
| `SendRingSize` | UINT32 | 16384 | 4096–1048576 | RES-003 |
| `MaxRejectPerSession` | UINT32 | 16 | 1–1000 | ERR-021 |

> SRV-013이 MUST인 이유: `RoomCapacity = 0`이면 아무도 룸에 못 들어가고,
> `MaxSession = 0`이면 접속이 안 된다. 조용히 동작 불능이 되는 것보다
> 기동 실패가 낫다.

### 2.3 기동 순서

| ID | 등급 | 요구사항 | 검증 |
|---|---|---|---|
| SRV-020 | MUST | 설정 로드 → 리슨 → 워커 기동 → 로직 기동 순서를 지킨다 | 검토 |
| SRV-021 | MUST | 리슨 소켓이 열린 뒤에만 "기동 완료" 로그를 남긴다 | 실행 |

### 2.4 종료

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| SRV-030 | MUST | 종료는 다음 순서를 지킨다: **수용 중단 → 로직 배수 → 세션 정리 → 워커 조인 → IOCP 핸들 해제** | limits.md | 검토 |
| SRV-031 | MUST | `CloseHandle(IOCP)`는 모든 워커 스레드가 조인된 뒤에 호출한다 | limits.md | 검토 |
| SRV-032 | MUST | 종료 시 접속 중인 세션에 `ServerShutdown` 사유로 절단한다 | ERR-010 | 실행 |
| SRV-033 | SHOULD | 종료가 5초 안에 끝나지 않으면 강제 종료하고 `warn`을 남긴다 | 신규 | 실행 |

> **SRV-030/031이 MUST인 이유:** 현재 `DestroyThread()`는 워커가 도는 중에
> `CloseHandle(mIOCPHandle)`을 호출한다. `GetQueuedCompletionStatus`가 유효하지 않은
> 핸들을 보게 되는 미정의 동작이다.

### 2.5 콘솔 명령

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| SRV-040 | MUST | `quit` — 정상 종료를 시작한다 | 현행 | 실행 |
| SRV-041 | MUST | `stat` — 메트릭 스냅샷을 출력한다 | OBS-020 | 실행 |
| SRV-042 | SHOULD | `room <n>` — 해당 룸의 인원과 큐 길이를 출력한다 | 신규 | 실행 |
| SRV-043 | MAY | `level <lvl>` — 로그 레벨을 런타임에 변경한다 | 신규 | 실행 |

> **SRV-041이 MUST인 이유:** 메트릭을 수집해도 볼 방법이 없으면 없는 것과 같다.
> HTTP 엔드포인트를 여는 것은 이 규모에서 과하다. 콘솔 한 줄이면 충분하다.

---

## 3. 네트워크 사양 (NET)

### 3.1 리슨

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| NET-001 | MUST | TCP / IPv4 / `Config::Port`에서 수신한다 | — | 실행 |
| NET-002 | MUST | `WSA_FLAG_OVERLAPPED` 소켓을 쓴다 | D-001 | 검토 |
| NET-003 | SHOULD | `listen` backlog는 `SOMAXCONN`을 쓴다 | 신규 | 검토 |
| NET-004 | MUST | 동시 접속은 `Config::MaxSession`을 초과하지 않는다 | D-002 | 부하 |

### 3.2 소켓 옵션

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| NET-010 | MUST | 접속 소켓에 `TCP_NODELAY`를 **실제로 설정한다** | 신규 | 실행 |
| NET-011 | MUST | `SO_RCVBUF = 0`(제로 카피 수신)은 설정하지 않는다 | 신규 | 검토 |
| NET-012 | MUST | 소켓 옵션 설정 실패는 `warn`으로 남기고 접속은 계속한다 | 신규 | 검토 |

> **NET-010:** `stClientInfo::SetSocketOption()`이 작성되어 있으나 **아무 데서도
> 호출되지 않는다.** 캐릭터 동기화가 20~30Hz 소량 패킷이라 Nagle 지연이 그대로
> 체감된다. 호출하도록 배선한다.
>
> **NET-011:** 같은 함수가 `SO_RCVBUF = 0`도 설정한다. 제로 카피 수신은 커널 버퍼를
> 없애 페이지 잠금 비용과 수신 누락 위험을 만든다. 이 규모에서 이득보다 위험이 크다.
> **호출을 살릴 때 이 줄은 빼야 한다.**

### 3.3 세션 식별

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| NET-020 | MUST | 세션은 `SessionId {Index, Generation}`으로 식별한다 | R-010 | 컴파일 |
| NET-021 | MUST | 슬롯 재사용 시 `Generation`을 1 증가시킨다 | R-010 | 단위 |
| NET-022 | MUST | 세션 경계를 넘는 모든 API는 raw index가 아니라 `SessionId`를 받는다 | R-010 | 컴파일 |
| NET-023 | MUST | 세대가 다른 `SessionId`로 온 요청은 조용히 버린다 | R-010 | 단위 |
| NET-024 | MUST | 세대 도입 후 `RE_USE_SESSION_WAIT_TIMESEC` 지연을 제거한다 | R-010 | 검토 |

> **NET-022가 이 사양에서 가장 중요한 타입 요구다.** `UINT32 clientIndex`를 받는
> 함수가 하나라도 남으면 세대 검사를 우회할 수 있다. **타입으로 막는다.**

### 3.4 연결 수명

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| NET-030 | MUST | `Config::IdleTimeoutSec` 동안 수신이 없으면 `IdleTimeout`으로 절단한다 | 신규 | 실행 |
| NET-031 | MUST | 마지막 수신 시각은 매 수신 완료 시 갱신한다 | NET-030 | 검토 |
| NET-032 | MUST | 타임아웃 검사는 틱에서 수행한다 (별도 폴링 스레드를 두지 않는다) | THR-004 | 검토 |
| NET-033 | MUST | 세션 반환 시 `AcceptEx`를 **즉시** 재등록한다 (주기 폴링 금지) | 신규 | 검토 |

> **NET-033:** 현재 `AccepterThread`가 32ms마다 전 슬롯을 순회한다. 접속이 없어도
> 초당 31회 × 100슬롯을 훑는다. 세션이 반환되는 시점에 재등록하면 이 스레드가 통째로 사라진다.

---

## 4. 프로토콜 사양 (PRO)

### 4.1 와이어 포맷

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| PRO-001 | MUST | 와이어 포맷을 변경하지 않는다 | D-006 | 컴파일 |
| PRO-002 | MUST | `sizeof(PACKET_HEADER) == 6` | R-012, I-7 | 컴파일 |
| PRO-003 | MUST | 모든 와이어 구조체 크기를 `static_assert`로 고정한다 | R-012 | 컴파일 |
| PRO-004 | MUST | `PacketLength`는 **헤더를 포함한** 전체 바이트 수다 | I-6 | 단위 |
| PRO-005 | MUST | 바이트 순서는 리틀 엔디언(네이티브), 변환하지 않는다 | D-006 | 검토 |

```cpp
// PacketHeader.h — 규정된 형태
struct PACKET_HEADER
{
    UINT16 PacketLength = { 0 };   // 헤더 포함 전체 길이
    UINT16 PacketId     = { 0 };
    UINT8  Type         = { 0 };   // 미사용
    UINT8  Reserved     = { 0 };   // 패딩을 명시화. 항상 0
};
static_assert(sizeof(PACKET_HEADER) == 6, "wire format changed");
```

### 4.2 프레이밍 검증

**이 절이 「출시 가능」의 핵심이다.** 신뢰 경계에서는 게으르게 처리하지 않는다.

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| PRO-010 | MUST | `PacketLength < sizeof(PACKET_HEADER)`이면 프로토콜 위반으로 판정한다 | R-017 | 단위 |
| PRO-011 | MUST | `PacketLength > Config::RecvAssemblerSize`이면 프로토콜 위반으로 판정한다 | 신규 | 단위 |
| PRO-012 | MUST | 프로토콜 위반 시 조립 버퍼를 폐기하고 세션을 `ProtocolViolation`으로 절단한다 | R-017 | 실행 |
| PRO-013 | MUST | 어떤 입력으로도 조립 루프가 무한 반복하지 않는다 | R-017 | 단위 |
| PRO-014 | MUST | 조립 루프의 종료 판정은 `PacketId` 값이 아니라 **유효 패킷 여부**로 한다 | R-017 | 검토 |

> **PRO-013의 검증 방법:** "읽기 위치가 전진하지 않는 반복이 없다"를 단위 테스트로
> 고정한다. `GetPacket`을 3회 연속 호출해도 같은 패킷이 나오지 않아야 한다.

### 4.3 패킷 검증 게이트

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| PRO-020 | MUST | 모든 클라이언트 패킷은 핸들러 실행 **전에** 크기를 검증한다 | R-005 | 단위 |
| PRO-021 | MUST | 모든 클라이언트 패킷은 핸들러 실행 **전에** 요구 상태를 검증한다 | R-009 | 단위 |
| PRO-022 | MUST | 검증은 디스패처 **한 곳**에서 수행한다. 핸들러 안에 중복 검사를 두지 않는다 | R-005 | 검토 |
| PRO-023 | MUST | 핸들러 등록 시 기대 크기와 요구 상태를 **함께** 선언해야 한다 | R-005 R-009 | 컴파일 |
| PRO-024 | MUST | 등록되지 않은 `PacketId`는 버리고 카운트한다 | 신규 | 실행 |
| PRO-025 | MUST | 서버→클라 전용 패킷(`*_RESPONSE`, `*_NOTIFY`)을 수신 등록하지 않는다 | R-006 | 검토 |
| PRO-026 | MUST | `CHARACTER_SYNC`의 `ClientIndex`가 송신 세션과 일치하는지 확인한다 | 신규 | 실행 |

> **PRO-023이 「컴파일」 검증인 이유:** 등록 API가 크기와 상태를 필수 인자로 받으면,
> 빠뜨린 채로는 컴파일이 안 된다. **검증을 잊는 것이 불가능해진다.** 이게 이 사양서에서
> 반복되는 원칙이다 — 규칙은 문서가 아니라 타입으로 강제한다.
>
> **PRO-026:** 현재는 남의 `ClientIndex`를 넣어 보내면 그대로 브로드캐스트된다.
> 즉 다른 사람의 캐릭터를 조종할 수 있다. Q2의 답과 무관하게 이건 막아야 한다.

### 4.4 상태별 허용 패킷

| 패킷 | 요구 상태 | 크기 |
|---|---|---:|
| `LOGIN_REQUEST` | `NONE` | 72 |
| `ROOM_ENTER_REQUEST` | `LOGIN` | 10 |
| `ROOM_LEAVE_REQUEST` | `ROOM` | 6 |
| `ROOM_CHAT_REQUEST` | `ROOM` | 263 |
| `CHARACTER_SYNC` | `ROOM` | 42 |

시스템 패킷(11~30)은 서버가 스스로 생성하므로 검증 대상이 아니다.
크기는 `sizeof(T)`로 자동 유도한다 (PRO-023).

### 4.5 버전 협상 — 조건부

| ID | 등급 | 요구사항 | 조건 |
|---|---|---|---|
| PRO-030 | 조건부 | 접속 시 클라/서버 프로토콜 버전을 교환하고 불일치를 명시적으로 거부한다 | fl-client 동시 수정 가능 시 |

와이어를 건드리므로 fl-client 작업과 묶어서 판단한다. 자체 엔진 클라이언트를
같은 사람이 개발하므로 빌드 불일치는 실제로 자주 겪을 문제다.

---

## 5. 스레드 / 동시성 사양 (THR)

### 5.1 스레드 구성

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| THR-001 | MUST | I/O 완료 처리 스레드는 `Config::IoWorkerCount`개다 | — | 검토 |
| THR-002 | MUST | 별도 Accepter 폴링 스레드를 두지 않는다 | NET-033 | 검토 |
| THR-003 | MUST | 구현되어 있으나 비활성인 스레드를 남기지 않는다 | D-004 | 검토 |
| THR-004 | MUST | 게임 로직은 고정 틱(`Config::TickHz`)으로 구동한다 | 신규 | 실행 |
| THR-005 | 조건부 | 룸별 잡큐 + 워커 풀 (룸 액터) | Q2 |

> **THR-003:** `SendThread`는 구현되어 있고 호출만 주석 처리되어 있다.
> Q1의 답에 따라 **되살리거나 삭제한다.** "있는데 꺼져 있음"은 허용하지 않는다.

### 5.2 소유권 규칙

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| THR-010 | MUST | I/O 워커는 게임 상태(`User` / `Room` / `*Manager`)를 만지지 않는다 | I-2, R-016 | 컴파일 |
| THR-011 | MUST | 수신 조립 버퍼는 **세션 전용**이다. 두 스레드가 공유하지 않는다 | R-016 | 검토 |
| THR-012 | MUST | 계층 경계는 완성된 패킷(`PacketInfo`)만 전달한다. raw 버퍼를 넘기지 않는다 | R-016 | 컴파일 |
| THR-013 | MUST | 송신 상태(`링버퍼` / `pending` / `in-flight` 플래그)의 모든 접근은 **동일한 락** 안에서 한다 | R-002 | 검토 |
| THR-014 | MUST | 아무것도 보호하지 않는 mutex를 두지 않는다 | R-013 | 검토 |
| THR-015 | 조건부 | 한 룸은 동시에 한 워커만 소유한다 (CAS로 보장) | Q2 | 단위 |

> **THR-010이 「컴파일」 검증인 이유:** `OnPacket(SessionId, const PacketInfo&)` 경계에서
> 게임 객체에 대한 참조를 아예 넘기지 않으면, I/O 워커는 만질 수단이 없다.
> **관례가 아니라 도달 불가능성으로 보장한다.**

### 5.3 불변식과 강제 수단

**이 표의 「강제 수단」 칸을 전부 채우는 것이 이 사양의 목표다.**
문서에 적힌 불변식은 코드로 옮겨지기 전까지의 임시 거처다.

| # | 불변식 | 목표 강제 수단 | ID |
|---|---|---|---|
| I-1 | 게임 로직은 지정된 스레드에서만 실행된다 | `assert(mOwnerThreadId == GetCurrentThreadId())` | THR-020 |
| I-2 | I/O 스레드는 게임 상태를 만지지 않는다 | **타입** — 경계가 `PacketInfo`만 전달 | THR-010 |
| I-3 | 인덱스 = 세션 ID | **타입** — `SessionId` 값 타입 | NET-020 |
| I-4 | 객체는 해제되지 않고 재사용된다 | 문서 (강제 수단 없음, 허용) | — |
| I-5 | `Room`은 `User`를 소유하지 않는다 | 문서 (강제 수단 없음, 허용) | — |
| I-6 | `PacketLength`는 헤더 포함 전체 길이 | 단위 테스트 | PRO-004 |
| I-7 | 헤더 6바이트, 바디 pack(1) | `static_assert` | PRO-002 |
| I-8 | ServerCore는 게임을 모른다 | 검토 (지켜지고 있음) | — |
| I-9 | 핸들러는 크기·상태 조건을 만족할 때만 실행 | 디스패처 게이트 | PRO-020/021 |
| I-10 | 한 룸은 한 워커만 | `atomic` CAS + `assert` | THR-015 |

| ID | 등급 | 요구사항 | 검증 |
|---|---|---|---|
| THR-020 | MUST | 스레드 소속이 정해진 객체는 Debug 빌드에서 `assert`로 소속을 검사한다 | 검토 |
| THR-021 | MUST | I-4/I-5처럼 강제 수단이 없는 불변식은 **문서에 그렇다고 명시한다** | 검토 |

> **THR-021이 요구사항인 이유:** "강제 수단 없음"이 누락인지 판단인지 구분해야 한다.
> I-4(객체 미해제)는 강제할 방법이 마땅치 않고, 어겨도 즉시 드러난다. 판단으로 남긴다.

---

## 6. 빌드 / 프로그램 사양 (BLD)

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| BLD-001 | MUST | 플랫폼은 x64 전용, 툴셋 `v143` 이상 | — | 컴파일 |
| BLD-002 | MUST | 언어 표준은 C++20(`/std:c++20`)이며 모든 프로젝트가 동일하다 | — | 컴파일 |
| BLD-003 | MUST | Debug / Release 두 구성 모두 경고 없이 빌드된다 | 신규 | 컴파일 |
| BLD-004 | SHOULD | 경고 수준은 `/W4` | 신규 | 컴파일 |
| BLD-005 | MUST | 의존성은 `vcpkg.json` 매니페스트로 선언한다 | 신규 | 검토 |
| BLD-006 | MUST | 프로젝트 간 링크는 **프로젝트 참조**로 한다. `#pragma comment(lib, "상대경로")`를 쓰지 않는다 | 신규 | 검토 |
| BLD-007 | MUST | 빌드 산출물은 저장소에 커밋하지 않는다 | R-019 | 검토 |
| BLD-008 | MUST | `.gitignore`는 이 저장소에 존재하는 경로만 규정한다 | R-019 | 검토 |
| BLD-009 | SHOULD | CI에서 x64 Debug/Release 빌드 + `Tests.exe` 실행 | 신규 | 검토 |

**프로그램 목록**

| 프로그램 | 종류 | 산출물 | ID |
|---|---|---|---|
| `GameServer.exe` | Application | `Binary/$(Configuration)/` | SRV-002 |
| `DummyClient.exe` | Application | `Binary/$(Configuration)/` | TST-020 |
| `ServerCore.lib` | StaticLibrary | `Libraries/$(Configuration)/` | — |
| `Tests.exe` | Application | `Binary/$(Configuration)/` | TST-001 |

> **BLD-005의 이유:** 현재 의존성 기록은 `CorePch.h`의 주석 한 줄이 전부다.
> 새 환경에서 재현하려면 그 주석을 읽고 손으로 설치해야 한다. CI를 붙이려면 필수다.
>
> **BLD-006:** `DummyClient/pch.h`가 `#pragma comment(lib, "Debug\\ServerCore.lib")`로
> 구성 이름을 하드코딩한다. 구성이 추가되거나 출력 경로가 바뀌면 조용히 깨진다.

---

## 7. 폴더 / 파일 사양 (FIL)

### 7.1 배치 규칙

| ID | 등급 | 요구사항 | 검증 |
|---|---|---|---|
| FIL-001 | MUST | `ServerCore/`에는 게임 개념이 등장하는 파일을 두지 않는다 | 검토 |
| FIL-002 | MUST | 와이어 패킷 구조체는 `Packet_*.h`에만 정의한다 | 검토 |
| FIL-003 | MUST | enum은 `Enum_*.h`에 하나씩 정의하고 `Enums.h`가 묶는다 | 검토 |
| FIL-004 | MUST | 빌드 산출물은 `Binary/`(exe) 와 `Libraries/`(lib) 로만 나간다 | 검토 |
| FIL-005 | MUST | 문서는 `docs/wiki/`(위키) 와 `docs/superpowers/plans/`(계획) 에 둔다 | 검토 |
| FIL-006 | MUST | 리팩터링 이전 코드 사본을 저장소에 두지 않는다 | 검토 |

> **FIL-006:** `backup/` 폴더에 이전 단일 프로젝트 구조가 통째로 남아있다.
> git에 추적되지는 않지만 로컬에 존재하며, `EchoServer` / `ChatServer` 등
> 지금은 없는 코드가 들어있어 검색 시 혼란을 준다. 이력은 git이 보관한다.

### 7.2 명명 규칙

| ID | 등급 | 요구사항 | 검증 |
|---|---|---|---|
| FIL-010 | MUST | 클래스는 `PascalCase` | 검토 |
| FIL-011 | MUST | POD성 구조체는 `st` 접두 (`stOverlappedEx`) | 검토 |
| FIL-012 | MUST | 와이어 패킷 구조체는 `SCREAMING_SNAKE_PACKET` | 검토 |
| FIL-013 | MUST | 멤버 변수는 `m` 접두. **신규 코드에 `m_`를 쓰지 않는다** | 검토 |
| FIL-014 | MUST | 매개변수는 뒤에 `_` (`maxUserCount_`) | 검토 |
| FIL-015 | SHOULD | 포인터/스마트포인터 지역변수는 `p` 접두 | 검토 |
| FIL-016 | MUST | enum은 `struct`로 감싸 네임스페이스화한다 | 검토 |

> **FIL-013:** 현재 `m`(지배적)과 `m_`(`m_RoomLock`, `m_eOperation`, `m_pPacketManager`)가
> 섞여 있다. **일괄 개명은 하지 않는다** — 순수 이름 변경 커밋은 diff만 키우고 검토를
> 방해한다. 해당 파일을 어차피 수정하는 Phase에서 함께 정리한다.

### 7.3 파일 크기

| ID | 등급 | 요구사항 | 검증 |
|---|---|---|---|
| FIL-020 | SHOULD | 구현 파일(`.cpp`)은 500줄을 넘지 않는다 | 검토 |
| FIL-021 | MUST | 500줄을 넘기기로 했으면 그 판단을 `decisions.md`에 남긴다 | 검토 |

> `PacketManager.cpp`가 현재 336줄로 가장 크다. 디스패처 분리 후 실측한 줄 수를 보고
> 도메인별 분할(`Handlers_Login` / `Handlers_Room` / `Handlers_Sync`)을 판단한다.
> **미리 쪼개지 않는다** — 배선 비용을 필요해지기 전에 치르는 것이다.

### 7.4 삭제 대상

| ID | 등급 | 대상 | 이유 |
|---|---|---|---|
| FIL-030 | MUST | `ServerCore/Packet.h/.cpp` | `PacketData`를 아무도 쓰지 않는다 |
| FIL-031 | MUST | `ServerCore/Types.h` | `int32` 별칭을 아무도 쓰지 않는다 |
| FIL-032 | SHOULD | `ServerCore/Server_Defines.h` | 헤더 하나만 포함하는 경유 헤더 |
| FIL-033 | MUST | `backup/` | FIL-006 |
| FIL-034 | MUST | `UserManager::IncreaseUserCnt` / `DecreaseUserCnt` | 호출되지 않는다 (R-008) |
| FIL-035 | MUST | `RE_USE_SESSION_WAIT_TIMESEC` | 세대값이 대체 (NET-024) |
| FIL-036 | 조건부 | `IOCPServer::SendThread` / `CreateSendThread` | Q1 |

---

## 8. 클래스 사양 (CLS)

각 클래스는 **책임 한 줄 · 공개 인터페이스 · 불변식 · 스레드 소속**을 갖는다.

### 8.1 `SessionId` (값 타입)

**책임:** 세션을 세대와 함께 식별한다.
**스레드 소속:** 없음 (값 타입, 복사 안전)

```cpp
struct SessionId
{
    UINT32 Index      = { UINT32_MAX };
    UINT32 Generation = { 0 };

    bool IsValid() const { return Index != UINT32_MAX; }
    bool operator==(const SessionId&) const = default;
};
```

| ID | 등급 | 요구사항 | 검증 |
|---|---|---|---|
| CLS-001 | MUST | 기본 생성된 `SessionId`는 무효(`IsValid() == false`)다 | 단위 |
| CLS-002 | MUST | `Index`가 같고 `Generation`이 다르면 다른 세션이다 | 단위 |

### 8.2 `RecvAssembler`

**책임:** TCP 스트림에서 패킷 경계를 복원한다.
**스레드 소속:** 소유 `Session`을 처리하는 I/O 워커 **전용**. 락 없음. (THR-011)

```cpp
enum class PopResult { None, Ok, Corrupted };

class RecvAssembler
{
public:
    void       Init(UINT32 bufferSize_);
    void       Reset();
    bool       Append(UINT32 dataSize_, const char* pData_);   // false = 용량 초과
    PopResult  TryPop(PacketInfo& out_);
};
```

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| CLS-010 | MUST | `TryPop`은 완성된 패킷이 없으면 `None`을 반환하고 상태를 바꾸지 않는다 | — | 단위 |
| CLS-011 | MUST | 프레이밍 위반 시 `Corrupted`를 반환하고 버퍼를 비운다 | PRO-010/011 | 단위 |
| CLS-012 | MUST | `Corrupted` 반환 후 재호출해도 `Corrupted` 또는 `None`만 반환한다 | PRO-013 | 단위 |
| CLS-013 | MUST | `Append`가 용량을 넘기면 `false`를 반환한다. 조용히 덮어쓰지 않는다 | R-001 유형 | 단위 |
| CLS-014 | MUST | 1바이트씩 나눠 넣어도 마지막 바이트에서 정확히 완성된다 | — | 단위 |
| CLS-015 | MUST | 여러 패킷이 한 번에 들어오면 순서대로 전부 나온다 | — | 단위 |

> **CLS-012가 중요한 이유:** 손상 후 "아무것도 반환하지 않되 전진도 안 함"이면
> 그 세션은 영원히 막힌다. 반드시 버려야 한다 (R-017).

### 8.3 `SendBuffer`

**책임:** 송신 데이터를 축적하고 in-flight 상태를 관리한다.
**스레드 소속:** 다중 스레드 접근. **모든 공개 메서드가 동일한 내부 락을 잡는다.** (THR-013)

```cpp
class SendBuffer
{
public:
    void Init(UINT32 ringSize_);
    void Reset();

    bool Enqueue(UINT32 dataSize_, const char* pData_);  // false = 용량 초과 → 절단
    bool TryBeginSend(WSABUF& out_);                     // false = 보낼 것 없음 또는 전송 중
    void OnSendCompleted(UINT32 sentSize_);
};
```

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| CLS-020 | MUST | 용량 초과 시 `false`를 반환한다. **미전송 데이터를 절대 덮어쓰지 않는다** | R-001 | 단위 |
| CLS-021 | MUST | `TryBeginSend`는 이미 전송 중이면 `false`를 반환한다 | R-002 | 단위 |
| CLS-022 | MUST | in-flight 플래그의 읽기·쓰기가 모두 내부 락 안에서 일어난다 | R-002 | 검토 |
| CLS-023 | MUST | `OnSendCompleted` 후 대기 데이터가 있으면 이어서 보낼 수 있는 상태가 된다 | 신규 | 단위 |
| CLS-024 | MUST | 부분 전송(`sentSize_ < 요청 크기`)을 올바르게 처리한다 | 신규 | 단위 |

> **CLS-024:** `WSASend`는 요청보다 적게 보낼 수 있다. 현재 코드는 이 경우를 다루지
> 않고 `mSendPos = 0`으로 전부 비운다. 남은 바이트가 사라진다.

### 8.4 `Session`

**책임:** 연결 하나의 I/O 상태 전부를 소유한다.
**스레드 소속:** I/O 워커. 세션당 동시에 하나의 워커만 I/O를 처리한다.

```cpp
class Session
{
public:
    SessionId GetId() const;

    bool PostAccept(SOCKET listenSock_);
    bool OnAcceptCompleted();
    bool OnRecvCompleted(UINT32 size_);
    bool Send(UINT32 dataSize_, shared_ptr<char[]> pData_);
    void OnSendCompleted(UINT32 size_);

    void Disconnect(DisconnectReason reason_);
    bool IsTimedOut(steady_clock::time_point now_) const;
};
```

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| CLS-030 | MUST | `Session`이 `RecvAssembler`와 `SendBuffer`를 소유한다 | R-016 | 컴파일 |
| CLS-031 | MUST | 재사용 시 `Generation`을 증가시키고 두 버퍼를 초기화한다 | NET-021 | 단위 |
| CLS-032 | MUST | `Disconnect`는 사유를 기록하고 중복 호출에 안전하다 | ERR-001 | 검토 |
| CLS-033 | MUST | 수신 완료 시 마지막 수신 시각을 갱신한다 | NET-031 | 검토 |

### 8.5 `IOCPServer` (추상)

**책임:** 소켓 I/O 전부. 게임 개념을 모른다. (I-8)
**스레드 소속:** 자신이 I/O 워커를 소유한다.

```cpp
class IOCPServer
{
public:
    bool Init(const Config& config_);
    bool StartServer();
    void StopAccepting();
    void CloseAllSessions();
    void JoinWorkers();

    bool Send(SessionId id_, UINT32 dataSize_, shared_ptr<char[]> pData_);

protected:
    virtual void OnConnect(SessionId id_) = 0;
    virtual void OnClose(SessionId id_, DisconnectReason reason_) = 0;
    virtual void OnPacket(SessionId id_, const PacketInfo& packet_) = 0;
};
```

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| CLS-040 | MUST | 계층 경계는 위 3개 가상 함수 + `Send` **4개뿐**이다 | I-8 | 검토 |
| CLS-041 | MUST | `OnPacket`은 **완성된 패킷**만 전달한다. raw 버퍼를 넘기지 않는다 | THR-012 | 컴파일 |
| CLS-042 | MUST | `OnClose`는 절단 사유를 함께 전달한다 | ERR-001 | 컴파일 |
| CLS-043 | MUST | `Send`는 무효하거나 세대가 다른 `SessionId`에 대해 `false`를 반환한다 | NET-023 | 단위 |

### 8.6 `PacketDispatcher`

**책임:** `PacketId` → 핸들러 라우팅 + 크기·상태 게이트.
**스레드 소속:** 등록은 기동 시 단일 스레드. 조회는 읽기 전용이므로 다중 스레드 안전.

```cpp
class PacketDispatcher
{
public:
    using Handler = function<void(SessionId, const PacketInfo&)>;

    // 크기는 sizeof(TPacket)로 자동 유도. 요구 상태는 필수 인자.
    template<typename TPacket>
    void Register(PACKET_ID::Enum id_, Handler func_, DOMAIN_STATE::Enum requiredState_);

    void RegisterSystem(PACKET_ID::Enum id_, Handler func_);   // 검증 생략

    DispatchResult Dispatch(SessionId id_, const PacketInfo& packet_, DOMAIN_STATE::Enum state_);
};
```

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| CLS-050 | MUST | `Register`는 요구 상태를 **필수 인자**로 받는다 (기본값 금지) | PRO-023 | 컴파일 |
| CLS-051 | MUST | 판정 로직은 순수 함수로 분리되어 단위 테스트 가능하다 | PRO-020/021 | 단위 |
| CLS-052 | MUST | 미등록 ID / 크기 불일치 / 상태 불일치를 각각 구분해 반환한다 | OBS-011 | 단위 |
| CLS-053 | MUST | 기동 후 등록 테이블은 변경되지 않는다 | THR-020 | 검토 |

> **CLS-050에서 기본값을 금지하는 이유:** `= DOMAIN_STATE::NONE` 같은 기본값이 있으면
> 안 적어도 컴파일된다. 그 순간 PRO-023이 무력화된다. **필수 인자여야 강제된다.**

### 8.7 `Room`

**책임:** 룸 하나의 인원 관리와 브로드캐스트.
**스레드 소속:** 【조건부 Q2】 룸 액터면 소유 워커 전용(락 없음), 단일 로직 스레드면 그 스레드 전용.

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| CLS-060 | MUST | `Room`은 `User`를 소유하지 않는다 (참조만) | I-5 | 검토 |
| CLS-061 | MUST | `Room`에 mutex를 두지 않는다 | R-013 | 검토 |
| CLS-062 | MUST | 브로드캐스트 시 패킷 크기를 인자로 받지 않는다 (타입에서 유도) | R-004 R-018 | 컴파일 |
| CLS-063 | 조건부 | `TryAcquire()`가 CAS로 단독 소유를 보장한다 | Q2 | 단위 |

```cpp
// CLS-062 — 크기 인자를 없애는 헬퍼
template<typename TPacket>
void SendPacketTo(SessionId id_, TPacket& packet_)
{
    packet_.PacketLength = sizeof(TPacket);
    Send(id_, sizeof(TPacket), MakePacketBuffer(packet_));
}
```

> **CLS-062가 R-004를 구조적으로 없앤다.** 크기를 따로 넘길 수 없으면
> "10바이트 버퍼를 43바이트로 전송"이 성립할 수 없다.

### 8.8 `Config` / `Metrics`

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| CLS-070 | MUST | `Config`는 기동 후 불변이다 | SRV-012 | 컴파일 (`const`) |
| CLS-071 | MUST | `Config::Load` 실패 시 기본값 인스턴스를 반환한다 | SRV-011 | 단위 |
| CLS-072 | MUST | `Metrics` 카운터는 `atomic`이며 락을 쓰지 않는다 | OBS-010 | 검토 |

---

## 9. 자원 사양 (RES)

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| RES-001 | MUST | 세션·유저·룸은 기동 시 전부 사전 할당하고 런타임에 생성/해제하지 않는다 | D-002, I-4 | 검토 |
| RES-002 | MUST | 조립 버퍼 기본값은 8 KB | limits.md | 검토 |
| RES-003 | MUST | 송신 링버퍼 기본값은 16 KB | R-001 | 검토 |
| RES-004 | SHOULD | 수신 소켓 버퍼는 1 KB | 신규 | 검토 |
| RES-005 | MUST | 100세션 기준 상주 메모리가 4 MB를 넘지 않는다 | limits.md | 부하 |

### 9.1 버퍼 크기 근거

| 버퍼 | 현재 | 목표 | 근거 |
|---|---:|---:|---|
| 수신 소켓 (`MAX_SOCKBUF`) | 256 B | 1 KB | 최대 패킷 296 B. 1 KB면 대부분 1회 수신으로 끝나 조립 횟수가 준다 |
| 조립 (`PACKET_DATA_BUFFER_SIZE`) | 64 KB | 8 KB | 최대 패킷 296 B. 64 KB는 근거 없는 과다 배분 |
| 송신 (`MAX_SOCK_SENDBUF`) | 4 KB ×2 | 16 KB 링 | 4인 룸 30Hz 브로드캐스트 버스트를 견뎌야 함 |

**조립 버퍼를 줄인 절감분으로 송신을 늘린다.** [limits.md](limits.md)의
"큰 쪽이 필요 없고 작은 쪽이 모자란다"의 해소다.

### 9.2 메모리 목표

| 항목 | 현재 | 목표 |
|---|---:|---:|
| 조립 버퍼 (100세션) | 6,400 KB | 800 KB |
| 송신 버퍼 (100세션) | 800 KB | 1,600 KB |
| 수신 소켓 버퍼 | 25 KB | 100 KB |
| 나머지 | 64 KB | 52 KB |
| **합계** | **≈ 7.3 MB** | **≈ 2.5 MB** |

RES-005의 상한 4 MB는 목표치 2.5 MB에 여유를 둔 값이다.

> 이 숫자들은 **목표치이지 측정값이 아니다.** 부하 테스트(TST-030)에서 실측하고
> 갱신한다.

---

## 10. 관측 사양 (OBS)

### 10.1 로그

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| OBS-001 | MUST | 패킷당 1회 이상 발생하는 로그는 `debug` 이하로 둔다 | R-011 | 검토 |
| OBS-002 | MUST | 접속당 1회 발생하는 로그는 `info` 이하로 둔다 | R-011 | 검토 |
| OBS-003 | MUST | 바이너리 버퍼를 문자열 서식(`{}`)으로 출력하지 않는다 | R-011 | 검토 |
| OBS-004 | MUST | 초기화되지 않은 값을 로그로 출력하지 않는다 | R-014 | 검토 |
| OBS-005 | MUST | 기본 로그 레벨은 Debug 빌드 `debug`, Release 빌드 `info` | R-011 | 실행 |
| OBS-006 | MUST | 원격 입력으로 유발되는 로그는 세션당 횟수를 제한한다 | ERR-021 | 실행 |

> **OBS-003:** `spdlog::info("msg : {}", recvBuf.get())`는 `char*`를 NUL 종단 문자열로
> 취급한다. 수신 버퍼는 바이너리이고 매 수신마다 0으로 채워지지 않으므로
> 할당 범위 밖을 읽는다. **성능 문제 이전에 안전 문제다.**
>
> **OBS-006:** 거부를 로그로 남기는 것 자체가 새 공격면이다. 공격자가 초당 수천 개의
> 잘못된 패킷을 보내면 로그가 디스크를 채운다.

### 10.2 메트릭

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| OBS-010 | MUST | 아래 지표를 상시 수집한다 | limits.md | 실행 |
| OBS-011 | MUST | 거부 사유별로 카운터를 분리한다 | CLS-052 | 실행 |
| OBS-012 | MUST | `stat` 명령으로 스냅샷을 출력한다 | SRV-041 | 실행 |
| OBS-013 | SHOULD | 틱 소요 시간을 히스토그램(p50/p99)으로 수집한다 | limits.md | 부하 |

**필수 지표**

| 지표 | 종류 | 왜 필요한가 |
|---|---|---|
| `Sessions.Active` | Gauge | 현재 접속 수 |
| `Sessions.Accepted` / `Disconnected` | Counter | 접속 회전율 |
| `Disconnect.<Reason>` | Counter | 절단 사유 분포 (§11.1) |
| `Packets.In` / `Packets.Out` | Counter | 처리량 |
| `Packets.Rejected.<사유>` | Counter | 신뢰 경계가 실제로 일하는지 |
| `Queue.MaxLength` | Gauge | 백프레셔 필요 여부 판단 근거 |
| `Tick.DurationMs` | Histogram | 로직 여유 |
| `SendBuffer.Overflow` | Counter | RES-003 재산정 근거 |

> **`SendBuffer.Overflow`가 0이 아니면 버퍼가 모자란 것이다.** 이 지표 하나가
> §9의 목표치가 맞았는지 알려준다.

---

## 11. 오류 / 절단 사양 (ERR)

### 11.1 절단 사유

| ID | 등급 | 요구사항 | 검증 |
|---|---|---|---|
| ERR-001 | MUST | 모든 절단은 사유를 갖는다 | 컴파일 |
| ERR-002 | MUST | 사유별로 로그 레벨과 메트릭을 구분한다 | 실행 |

```cpp
enum class DisconnectReason
{
    PeerClosed,          // 수신 0바이트 — 정상
    SocketError,         // I/O 오류
    ProtocolViolation,   // 프레이밍 깨짐 (PRO-012)
    RejectThreshold,     // 거부 누적 초과 (ERR-021)
    SendOverflow,        // 송신 링버퍼 초과 (CLS-020)
    IdleTimeout,         // 하트비트 없음 (NET-030)
    ServerShutdown,      // 정상 종료 (SRV-032)
};
```

| 사유 | 로그 레벨 | 이유 |
|---|---|---|
| `PeerClosed` | debug | 정상 종료 |
| `SocketError` | info | 흔함 |
| `ProtocolViolation` | warn | 클라 버그 또는 공격 |
| `RejectThreshold` | warn | 동상 |
| **`SendOverflow`** | **error** | **서버 문제일 수 있음** |
| `IdleTimeout` | info | — |
| `ServerShutdown` | debug | — |

> **`SendOverflow`만 `error`인 이유:** 나머지는 클라이언트 사정이지만, 이건 서버가
> 보낼 것을 다 못 보내고 있다는 신호다. 버퍼 부족이거나 브로드캐스트 설계 문제다.

### 11.2 오류 처리 원칙

| ID | 등급 | 요구사항 | 근거 | 검증 |
|---|---|---|---|---|
| ERR-010 | MUST | 데이터를 조용히 손상시키느니 명시적으로 절단한다 | R-001 | 검토 |
| ERR-011 | MUST | 실패를 `true`로 반환하지 않는다 | R-001 | 검토 |
| ERR-012 | MUST | 한 세션의 오류가 다른 세션에 영향을 주지 않는다 | 신규 | 실행 |
| ERR-013 | MUST | 거부된 패킷은 버리되 연결을 즉시 끊지는 않는다 (임계까지) | 신규 | 실행 |

> **ERR-011:** `stClientInfo::SendMsg`는 미전송 데이터를 덮어쓴 뒤에도 `true`를
> 반환한다. 호출자는 성공했다고 믿는다. **조용한 실패가 가장 비싼 실패다.**

### 11.3 임계

| ID | 등급 | 요구사항 | 검증 |
|---|---|---|---|
| ERR-020 | MUST | 세션별 거부 카운터를 유지한다 | 실행 |
| ERR-021 | MUST | `Config::MaxRejectPerSession` 초과 시 `RejectThreshold`로 절단한다 | 실행 |
| ERR-022 | MUST | 카운터는 세션 재사용 시 초기화한다 | 단위 |

---

## 12. 테스트 사양 (TST)

### 12.1 단위 테스트

| ID | 등급 | 요구사항 | 검증 |
|---|---|---|---|
| TST-001 | MUST | `Tests.exe`가 존재하고 전체 통과가 기동 조건이다 | 실행 |
| TST-002 | MUST | 아래 대상은 **반드시** 단위 테스트를 갖는다 | 검토 |

**필수 테스트 대상**

| 대상 | 왜 필수인가 | 관련 ID |
|---|---|---|
| `RecvAssembler` 프레이밍 | 눈으로 볼 수 없고 깨지면 증상이 이해 불가능 | CLS-010~015 |
| `RecvAssembler` 손상 처리 | 원격 DoS 경로. 실클라이언트로는 못 잡음 | PRO-010~013 |
| `SendBuffer` 용량/부분전송 | 데이터 손실 경로 | CLS-020~024 |
| 디스패치 판정 규칙 | 신뢰 경계 | PRO-020/021 |
| `SessionId` 세대 판정 | 오배달 경로 | CLS-001/002 |
| `Config` 범위 검증 | 조용한 동작 불능 방지 | SRV-013 |

| ID | 등급 | 요구사항 | 검증 |
|---|---|---|---|
| TST-003 | MUST | 테스트는 소켓·스레드 없이 실행된다 (순수 로직만) | 검토 |
| TST-004 | MUST | 결함을 고칠 때는 **실패하는 테스트를 먼저 만든다** | 검토 |
| TST-005 | SHOULD | 테스트가 의도한 코드 경로를 실제로 밟는지 한 번은 확인한다 | 검토 |

> **TST-005:** 통과했다고 그 경로를 밟은 건 아니다. compaction처럼 드물게 실행되는
> 분기는 중단점으로 한 번 확인해야 테스트가 의미를 갖는다.

### 12.2 검증 도구

| ID | 등급 | 요구사항 | 검증 |
|---|---|---|---|
| TST-010 | MUST | `DummyClient`는 정상 흐름 전체를 수행할 수 있다 | 실행 |
| TST-011 | MUST | `DummyClient`는 **조작된 헤더**를 보낼 수 있다 (`raw <len> <id>`) | 실행 |
| TST-012 | MUST | `DummyClient`의 수신 파서도 프레이밍 하한을 검사한다 | 실행 |
| TST-020 | SHOULD | `DummyClient`는 N개 동시 접속 + 주기적 동기화 부하 모드를 갖는다 | 실행 |

> **TST-011이 MUST인 이유:** 정상 클라이언트가 만들지 않는 패킷은 정상 클라이언트로
> 테스트할 수 없다. 신뢰 경계 요구사항(PRO-010~026)의 **유일한 실증 수단**이다.

### 12.3 부하 테스트

| ID | 등급 | 요구사항 | 검증 |
|---|---|---|---|
| TST-030 | MUST | 목표 규모에서 아래를 실측하고 기록한다 | 부하 |

**측정 항목**

| 항목 | 판정 기준 |
|---|---|
| 로직 스레드 점유율 | 목표 규모에서 여유가 있어야 함 |
| 패킷 큐 최대 길이 | 백프레셔 필요 여부 판단 |
| 상주 메모리 | RES-005 (4 MB 이내) |
| `SendBuffer.Overflow` | **0이어야 함** |
| 동기화 왕복 지연 p50/p99 | 기록 (기준값 없음 — 첫 측정이 기준선) |

> **기준값을 미리 정하지 않는다.** 측정한 적이 없으므로 정할 근거가 없다.
> 첫 측정이 기준선이 되고, 이후 회귀를 그 값과 비교한다.

---

## 13. 적합성 검사

이 사양의 충족 여부를 판정하는 체크리스트. **Phase가 끝날 때마다 해당 항목을 확인한다.**

### 13.1 컴파일 타임 (빌드가 곧 검증)

- [ ] PRO-002/003 — 모든 와이어 구조체에 `static_assert`
- [ ] NET-020/022 — `SessionId`가 경계를 넘고, raw index API가 없다
- [ ] THR-010/012 — `OnPacket`이 `PacketInfo`만 전달
- [ ] CLS-050 — `Register`의 요구 상태가 필수 인자 (기본값 없음)
- [ ] CLS-062 — 브로드캐스트에 크기 인자가 없다
- [ ] BLD-002/003 — C++20, Debug/Release 경고 없음

### 13.2 단위 테스트

- [ ] CLS-010~015 — `RecvAssembler` 전체
- [ ] CLS-020~024 — `SendBuffer` 전체
- [ ] PRO-010~013 — 프레이밍 위반 처리
- [ ] PRO-020/021 — 크기·상태 게이트
- [ ] CLS-001/002 — `SessionId` 세대
- [ ] SRV-013 — 설정 범위 검증

### 13.3 실행 확인 (`DummyClient`)

- [ ] 정상 흐름: login → enter → chat → leave → 재입장
- [ ] `raw 0 1001` → 서버가 계속 응답 (PRO-013)
- [ ] 로그인 전 `enter` → 거부 (PRO-021)
- [ ] 잘못된 크기 → 거부, 크래시 없음 (PRO-020)
- [ ] 미등록 ID → 거부 (PRO-024)
- [ ] 남의 `ClientIndex`로 동기화 → 거부 (PRO-026)
- [ ] 거부 임계 초과 → 절단 (ERR-021)
- [ ] 유휴 방치 → 절단 (NET-030)
- [ ] 거부 후에도 그 연결이 정상 동작 (ERR-013)
- [ ] `stat` 출력에 필수 지표 전부 (OBS-010)
- [ ] Release에서 동기화 부하 중 로그 도배 없음 (OBS-001)

### 13.4 부하 확인

- [ ] 상주 메모리 4 MB 이내 (RES-005)
- [ ] `SendBuffer.Overflow == 0` (TST-030)
- [ ] 목표 규모에서 로직 스레드 여유 (TST-030)

### 13.5 코드 검토

- [ ] SRV-030/031 — 종료 순서
- [ ] THR-003 — 비활성 스레드 없음
- [ ] THR-014 — 아무것도 보호하지 않는 mutex 없음
- [ ] FIL-030~035 — 삭제 대상 전부 제거
- [ ] PRO-022 — 핸들러 안에 중복 검증 없음
- [ ] ERR-011 — 실패를 `true`로 반환하는 곳 없음

---

## 14. 미결 사양

**답이 나오기 전에는 구현하지 않는다.** 조건부 요구사항의 근거다.

| # | 질문 | 영향받는 요구사항 |
|---|---|---|
| **Q1** | `SendThread`를 왜 껐나? (D-004) | THR-003, FIL-036, RES-003 |
| **Q2** | 어떤 게임인가? 전투·아이템이 있나? | THR-005, THR-015, CLS-063, PRO-026 범위 |
| **Q3** | 룸 10 / 정원 4는 확정값인가? | SRV-010 기본값, RES-005 |
| **Q4** | 목표 동시 접속은? | SRV-010 기본값, RES-005, TST-030 |
| **Q5** | Redis/DB를 넣나? (D-008) | 인증 사양 전체 (현재 미규정) |

### 14.1 아직 규정하지 않은 영역

| 영역 | 왜 미규정인가 |
|---|---|
| 인증 / 비밀번호 검증 | Q5 미정. DB 방향이 정해져야 사양을 쓸 수 있다 |
| 서버 권위 (위치 검증) | Q2 미정. 게임 종류가 정해져야 판단 가능 |
| 게임 규칙 (전투·아이템) | Q2 미정 |
| 재접속 복구 | 인증이 없으면 정의할 수 없다 |

**이건 누락이 아니라 대기다.** 답이 나오면 해당 절을 추가한다.

---

## 15. 요구사항 요약

| 영역 | MUST | SHOULD | MAY | 조건부 | 계 |
|---|---:|---:|---:|---:|---:|
| SRV 서버 | 13 | 2 | 1 | 0 | 16 |
| NET 네트워크 | 12 | 1 | 0 | 0 | 13 |
| PRO 프로토콜 | 19 | 0 | 0 | 1 | 20 |
| THR 스레드 | 10 | 0 | 0 | 2 | 12 |
| BLD 빌드 | 6 | 3 | 0 | 0 | 9 |
| FIL 파일 | 16 | 3 | 0 | 1 | 20 |
| CLS 클래스 | 27 | 0 | 0 | 1 | 28 |
| RES 자원 | 4 | 1 | 0 | 0 | 5 |
| OBS 관측 | 10 | 2 | 0 | 0 | 12 |
| ERR 오류 | 9 | 0 | 0 | 0 | 9 |
| TST 테스트 | 8 | 2 | 0 | 0 | 10 |
| **계** | **134** | **14** | **1** | **5** | **154** |

### 15.1 근거 분포

| 근거 | 요구사항 수 |
|---|---:|
| review-log의 R-번호 | 62 |
| 불변식 I-1~I-10 | 14 |
| decisions.md의 D-번호 | 9 |
| 신규 (이 사양서에서 처음 규정) | 69 |

> **신규가 45%인 것은 정상이다.** review-log는 "지금 잘못된 것"을 담고,
> 사양서는 "충족해야 할 것"을 담는다. 종료 순서·타임아웃·메트릭처럼
> **없어서 결함으로도 안 잡히는 것**이 여기서 처음 규정된다.

---

## 이 문서 유지 규칙

- **요구사항 번호는 재사용하지 않는다.** 폐기해도 지우지 않고 `폐기`로 표시한다.
- 모든 MUST에는 검증 방법이 있어야 한다. **검증할 수 없으면 MUST가 아니다.**
- 조건부 요구사항은 답을 들으면 **확정 또는 삭제**로 바꾼다.
- Phase가 끝나면 §13 적합성 검사의 해당 항목을 채우고, 결과를
  [review-log.md](review-log.md)의 **결과** 칸에 커밋 해시와 함께 남긴다.
- 사양과 코드가 어긋나면 **둘 중 하나가 틀린 것이다.** 조용히 넘어가지 말고
  사양을 고치거나 코드를 고친다. 어느 쪽인지 판단은 사람이 한다.
