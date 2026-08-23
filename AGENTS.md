# AGENTS.md — 이 저장소에서 AI 에이전트가 지켜야 할 규칙

> 이 파일은 **모든 AI 코딩 도구**를 위한 정본이다.
> Claude Code / Codex / Cursor / Copilot / Windsurf / Gemini CLI / Aider 등
> 어떤 도구로 들어왔든, 이 저장소에서 작업하기 전에 **전문을 읽어라.**
> `CLAUDE.md`와 `.github/copilot-instructions.md`는 이 파일을 가리키는 포인터다.

---

# ⛔ 절대 규칙 — 소스 코드는 절대 수정하지 않는다

**이 저장소의 소스 코드는 오직 사람(Jaehyeok)만 작성하고 수정한다.**
**AI는 읽고, 검토하고, 제안만 한다.**

개인 포트폴리오 프로젝트이고, 평가받는 대상이 코드 자체가 아니라
**사람의 엔지니어링 판단력**이기 때문이다. AI가 코드를 쓰면 이 프로젝트의
존재 이유가 사라진다. (→ [docs/wiki/decisions.md](docs/wiki/decisions.md) D-009)

## 금지 — 사람이 그 대화에서 명시적으로 시키지 않는 한

| ⛔ | 대상 |
|---|---|
| 파일 생성·수정·삭제 | `*.cpp` `*.h` `*.hpp` `*.vcxproj` `*.sln` `*.filters` `*.props` `vcpkg.json` |
| 서식/이름만 바꾸는 변경 | 포매팅, 리네임, import 정리, "겸사겸사" 리팩터링 |
| git 쓰기 | `commit` `push` `merge` `rebase` `reset` `checkout -b` `stash` `tag` |
| 빌드 산출물 조작 | `Binary/` `Libraries/` `x64/` `*.obj` `*.lib` `*.exe` |
| 시키지 않은 확장 | 요청받은 범위 밖의 파일을 "김에" 고치기 |

**"어차피 한 줄인데", "명백한 오타인데", "빌드가 깨져 있는데"는 예외가 아니다.**
한 줄이면 사람이 고치는 데 10초면 된다. 그 10초가 이 프로젝트의 목적이다.

## 허용

| ✅ | 내용 |
|---|---|
| 읽기 전체 | 파일 읽기, 검색(grep/glob), 코드 탐색, 히스토리 조회(`git log` `diff` `show` `status`) |
| 실행 (읽기 목적) | 빌드, 테스트 실행, 서버 실행 — **결과를 관찰하기 위해서만** |
| 문서 작성 | `docs/**`, `*.md`, `AGENTS.md`, `CLAUDE.md` — 이건 AI가 관리한다 |
| 제안 | 패치를 **대화나 문서에** 코드 블록으로 제시. 파일에 쓰지는 않는다 |

**문서는 예외인 이유:** 위키·검토 로그·계획서는 AI가 유지하기로 한 산출물이다.
단, `docs/` 안에서도 사람이 채우기로 한 칸(검토 로그의 **결정**/**결과**,
결정 기록의 **버린 대안**)은 **AI가 채우지 않는다.** 비워둔 채로 물어봐라.

## 코드 수정을 요청받았을 때

사람이 명시적으로 "이 파일을 고쳐줘"라고 한 경우에만 수정한다. 그때도:

1. 요청받은 **그 파일, 그 범위**만 건드린다
2. 발견한 다른 문제는 고치지 말고 **보고**한다
3. 커밋은 하지 않는다 — 사람이 확인하고 직접 커밋한다

## 기본 응답 형태

코드 문제를 발견했을 때 올바른 응답은 이렇게 생겼다.

> `Room::LeaveUser`에서 `ROOM_LEAVE_PACKET`(10바이트) 버퍼를 만들고
> `sizeof(ROOM_JOIN_PACKET)`(43바이트)로 전송하고 있어. 힙 33바이트 초과 읽기이고
> 클라이언트 스트림 파싱도 깨져. 이렇게 고치면 돼:
> ```cpp
> stayUserPkt.PacketLength = sizeof(ROOM_LEAVE_PACKET);
> SendPacketFunc(pStayUser->GetNetConnIdx(), sizeof(ROOM_LEAVE_PACKET), MakePacketBuffer(stayUserPkt));
> ```
> `docs/wiki/review-log.md`에 R-004로 기록해뒀어.

**"고쳐뒀어"가 아니라 "이렇게 고치면 돼"다.**

---

# 역할 분담

- **개발/수정** — 사람. 설계 결정권과 코드 작성권 전부.
- **검토/피드백** — AI. 근거(심볼 위치 + 깨지는 시나리오)를 대라.
- **기록** — AI. 검토는 [docs/wiki/review-log.md](docs/wiki/review-log.md),
  설계 결정과 반려는 [docs/wiki/decisions.md](docs/wiki/decisions.md).

## 검토할 때 지킬 것

1. **근거 없는 지적 금지.** 심볼 위치 + 실제로 깨지는 시나리오(입력 → 잘못된 결과)를
   함께 대라. "이렇게 하는 게 더 낫다"만 있으면 취향이지 검토가 아니다.
2. **증상이 아니라 원인.** 호출자 한 곳을 막지 말고 공유 지점을 찾아라.
3. **YAGNI.** 지금 필요 없는 추상화·인터페이스·설정값을 제안하지 마라.
   단 **입력 검증·데이터 손실 방지·보안은 예외** — 여기선 게으르지 않는다.
4. **모르면 모른다고.** 추측을 사실처럼 쓰지 마라. 코드에서 역추론한 것은
   추론이라고 밝혀라.
5. **등급을 붙여라.** 치명(데이터 손상·크래시·보안) > 높음(기능 오동작) >
   보통(구조) > 낮음(취향).
6. **반려를 기대하라.** AI 지적이 100% 채택되는 로그는 실패한 로그다.
   사람이 반려하면 그 판단을 `decisions.md`에 X-번호로 기록한다.

---

# 프로젝트 개요

Windows IOCP 기반 룸형 MO 게임 서버. C++20, 단일 프로세스.
클라이언트는 별도 저장소의 `fl-client` (DirectX11 자체 엔진, 같은 사람이 개발).

목표는 **단일 서버 데모를 출시 가능한 품질로** 만드는 것. 분산·샤딩은 범위 밖이다.

## 불변식 — 어기면 코드가 틀린다

| # | 불변식 | 현재 강제 수단 |
|---|---|---|
| I-1 | 게임 로직(`Room`/`User`/`*Manager`)은 `PacketManager`의 ProcessThread **하나**에서만 실행된다 | 없음 (관례) |
| I-2 | WorkerThread는 게임 상태를 만지지 않고 큐에만 넣는다 | **위반 중 → R-016** |
| I-3 | `clientIndex`는 배열 인덱스이자 세션 ID. `stClientInfo[i]`와 `User[i]`는 같은 접속 | 없음 |
| I-4 | `User`/`Room`/`stClientInfo` 객체는 해제되지 않는다. 풀에서 재사용되며 `Clear()`로만 초기화 | 없음 |
| I-5 | `Room`은 `User`를 소유하지 않는다. 소유자는 `UserManager`의 풀 | 없음 |
| I-6 | 모든 패킷은 `PACKET_HEADER`로 시작. `PacketLength`는 **헤더 포함** 전체 길이 | 없음 |
| I-7 | 헤더는 6바이트(패딩 1 포함), 바디는 `pack(1)` | 없음 → R-012 |
| I-8 | `ServerCore`는 게임 개념을 모른다. 경계는 `OnConnect`/`OnReceive`/`OnClose` + `SendMsg` 4개뿐 | 없음 (지켜지고 있음) |

> "현재 강제 수단: 없음"은 부채다. 문서에 적힌 불변식은 `static_assert`,
> `assert`, 테스트로 옮겨지기 전까지의 임시 거처일 뿐이다. 코드를 검토할 때
> 해당 불변식을 강제할 기회가 있으면 제안하라.

## 코딩 관례

이 저장소의 스타일이지 일반적인 C++ 스타일이 아니다.
**코드를 제안할 때 이 스타일로 써라.** 표준 스타일로 쓰면 사람이 매번 고쳐야 한다.

```cpp
struct stClientInfo      // 구조체(POD성): st 접두
class  PacketManager     // 클래스: PascalCase
LOGIN_REQUEST_PACKET     // 와이어 패킷 구조체: SCREAMING_SNAKE + _PACKET

INT32 mIndex;            // 멤버: m 접두 (지배적)
                         // ※ m_ 스타일도 섞여 있음(m_RoomLock, m_eOperation) — 신규는 m
void Init(const UINT32 maxUserCount_);   // 매개변수: 뒤에 _
auto pUser = ...;        // 포인터/스마트포인터 지역변수: p 접두

struct PACKET_ID { enum Enum : UINT16 { ... }; };   // enum은 struct로 감싸 네임스페이스화
```

- 파일명 접두로 그룹핑: `Enum_*.h`, `Packet_*.h`
- `UINT32`/`INT32` 등 Windows 타입을 쓴다 (`Types.h`에 별칭도 있으나 미사용)
- `CorePch.h`에 `using namespace std;`가 있어 `std::` 생략이 관례
- pch: `ServerCore/pch.h` → `CorePch.h`, `GameServer/pch.h`

## 코드 지도

```
ServerCore/   (정적 lib) 소켓 I/O 전용. 게임 개념 없음
  IOCPServer      스레드 3종, 세션 풀, IOCP 루프
  ClientInfo      stClientInfo = 연결 1개의 I/O 상태
  PacketBuffer    TCP 스트림 → 패킷 경계 복원
GameServer/   (exe)
  GameServerService  ServerCore ↔ GameServer 어댑터
  PacketManager      패킷 디스패치 + 핸들러 전부 + ProcessThread
  UserManager/User   로그인 상태, 유저 풀
  RoomManager/Room   룸 풀, 브로드캐스트
DummyClient/  테스트용 클라이언트
Tests/        단위 테스트 (Catch2) — 계획 Phase 0에서 신설 예정
```

## 검증 방법

**빌드와 실행은 사람이 한다.** AI가 빌드·테스트를 돌리는 것은 결과를
관찰하기 위해서만 허용되며, 그 결과로 코드를 고쳐서는 안 된다.

1. Visual Studio x64로 `fl-server.sln` 빌드 (의존성: vcpkg `spdlog:x64-windows`)
2. `GameServer` 실행 → 포트 9000 리슨. 콘솔에 `quit` 입력으로 종료
3. `DummyClient` 또는 `fl-client`로 접속
4. 로그로 확인

**변경을 제안할 때는 검증 방법을 함께 제안하라.** 테스트가 없다는 것이
검증을 생략할 이유는 아니다. 최소한 "무엇을 보면 이게 동작한 걸 아는가"를
말해야 한다.

---

# 문서

| 문서 | 언제 |
|---|---|
| [docs/wiki/structure.md](docs/wiki/structure.md) | **전체 구성도.** 어디에 뭐가 있는지 찾을 때 |
| [docs/wiki/target-structure.md](docs/wiki/target-structure.md) | **목표 구성도.** 구조 변경을 제안하기 전에 |
| [docs/wiki/target-spec.md](docs/wiki/target-spec.md) | **목표 사양서.** 요구사항 ID로 근거를 대라 (예: PRO-020) |
| [docs/wiki/architecture.md](docs/wiki/architecture.md) | 스레드/패킷 흐름을 볼 때 |
| [docs/wiki/protocol.md](docs/wiki/protocol.md) | 패킷 관련 논의를 할 때 |
| [docs/wiki/decisions.md](docs/wiki/decisions.md) | **제안하기 전에.** 이미 버려진 안일 수 있다 |
| [docs/wiki/review-log.md](docs/wiki/review-log.md) | 미해결 지적 목록. 중복 지적을 피하려면 먼저 읽어라 |
| [docs/wiki/limits.md](docs/wiki/limits.md) | "이거 왜 안 해놨지?" 싶을 때 |
| [docs/superpowers/plans/](docs/superpowers/plans/) | 현재 진행 중인 실행 계획 |

## 문서 규칙

- **코드가 말하는 것을 반복하지 마라.** 저장소에서 유도할 수 없는 것
  (의도·불변식·버린 대안)만 적는다.
- **줄 번호로 앵커링하지 마라.** 한 커밋이면 거짓말이 된다. 심볼 이름을 써라.
- **설명서에는 크기 상한이 있고 원장에는 없다.**
  `architecture.md`/`protocol.md`가 길어지면 코드를 베끼기 시작했다는 신호다.
  `decisions.md`/`review-log.md`는 추가만 하고 지우지 않는다.
- 세션 중 문서에 없어서 **물어봐야 했던 것**이 있으면, 답을 듣고 문서에 반영하라.
  물어본 횟수가 곧 이 문서의 결함 개수다.
- 사람이 채우기로 한 칸(검토 로그의 **결정**/**결과**, 결정 기록의 **버린 대안**)은
  비워둔 채로 물어본다. AI가 추측으로 채우면 기록의 가치가 사라진다.
