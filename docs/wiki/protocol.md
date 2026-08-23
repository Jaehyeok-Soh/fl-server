# 프로토콜

`fl-server` ↔ `fl-client` 사이의 와이어 규약. 양쪽 다 C++이므로 구조체를
공유하는 것이 가장 안전하다.

## 와이어 포맷

모든 패킷은 `PACKET_HEADER`로 시작하고, 헤더 뒤에 바디가 붙는다.
바이트 순서는 **리틀 엔디언**(x86 네이티브, 변환 없음).

```cpp
// ServerCore/PacketHeader.h
struct PACKET_HEADER
{
    UINT16 PacketLength;  // 헤더 포함 전체 길이
    UINT16 PacketId;      // PACKET_ID enum
    UINT8  Type;          // 미사용
};
```

`PacketLength`는 **헤더를 포함한** 전체 바이트 수다. `PacketBuffer::GetPacket`이
이 값만 보고 스트림을 자른다.

### 패딩에 관한 주의 — 가장 중요한 규약

`PACKET_HEADER`에는 `#pragma pack`이 **없다**. `UINT16 + UINT16 + UINT8` = 5바이트
선언이지만, 2바이트 정렬 때문에 실제 `sizeof(PACKET_HEADER)`는 **6**이다.
꼬리에 패딩 1바이트가 들어간다.

반면 모든 파생 패킷 구조체는 `#pragma pack(push,1)` 안에 선언되어 있다.
즉 **헤더는 6바이트로 패딩되고, 바디는 패딩 없이 붙는** 하이브리드 레이아웃이다.

이건 서버 내부적으로는 일관되지만 **암묵적**이다. 클라이언트가 헤더를 5바이트로
만들면 모든 패킷이 1바이트씩 밀려서 조용히 오동작한다. 두 가지 선택지가 있다.

1. `PACKET_HEADER`도 `pack(1)`로 감싸 5바이트로 통일 — 깔끔하지만 클라이언트도
   같이 고쳐야 한다.
2. 현 상태 유지 + 명시화 — 미사용 `Type` 뒤에 `UINT8 Reserved`를 넣어 6바이트를
   *의도적으로* 만든다. 와이어 호환이 깨지지 않는다.

어느 쪽이든 **크기를 컴파일 타임에 못 박는 것**이 핵심이다.

```cpp
static_assert(sizeof(PACKET_HEADER) == 6, "wire format changed");
static_assert(sizeof(LOGIN_REQUEST_PACKET) == 72, "wire format changed");
// ... 패킷마다 한 줄
```

한 줄짜리 방어지만, 컴파일러 설정이나 필드 추가로 레이아웃이 바뀌는 순간
빌드가 깨져서 알려준다. 이게 없으면 클라이언트에서 값이 이상하게 나오는
형태로 며칠 뒤에 발견된다.

> 아래 표의 크기는 위 규칙으로 **계산한 값**이다. 실제 컴파일 결과와 대조해
> `static_assert`로 고정하는 것을 권한다.

## 패킷 목록

`ServerCore/Enum_PacketId.h`

### 시스템 (11~30)

서버 내부에서 생성되며 클라이언트가 보내는 것이 아니다.

| ID | 이름 | 방향 | 바디 | 크기 |
|---|---|---|---|---|
| 11 | `SYS_USER_CONNECT` | 내부 | — | — |
| 12 | `SYS_USER_DISCONNECT` | 내부 | — | — |
| 13 | `SYS_USER_CONNECT_RESPONSE` | S→C | `UINT32 ClientId` | 10 |

접속하면 서버가 즉시 `SYS_USER_CONNECT_RESPONSE`로 자신의 `ClientId`(세션 인덱스)를
알려준다. 클라이언트는 이 값으로 이후 브로드캐스트에서 "나"를 구분한다.

### 로그인 (201~202)

| ID | 이름 | 방향 | 바디 | 크기 |
|---|---|---|---|---|
| 201 | `LOGIN_REQUEST` | C→S | `char UserID[33]`, `char UserPW[33]` | 72 |
| 202 | `LOGIN_RESPONSE` | S→C | `UINT16 Result` | 8 |

`Result`는 `ERROR_CODE`. `NONE(0)`이면 성공, `LOGIN_USER_ALREADY(31)` 중복 로그인,
`LOGIN_USER_USED_ALL_OBJ(32)` 정원 초과.

> `UserPW`는 현재 **검증되지 않는다.** 그리고 `LOGIN_RESPONSE`(202)가 서버의
> 수신 핸들러 테이블에도 등록되어 있어서, 클라이언트가 응답 패킷을 보내면
> 로그인 처리가 실행된다. (→ review-log R-004)

### 룸 (206~232)

| ID | 이름 | 방향 | 바디 | 크기 |
|---|---|---|---|---|
| 206 | `ROOM_ENTER_REQUEST` | C→S | `INT32 RoomNumber` | 10 |
| 207 | `ROOM_ENTER_RESPONSE` | S→C | `INT16 Result` | 8 |
| 215 | `ROOM_LEAVE_REQUEST` | C→S | — | 6 |
| 216 | `ROOM_LEAVE_RESPONSE` | S→C | `INT16 Result` | 8 |
| 221 | `ROOM_CHAT_REQUEST` | C→S | `char Message[257]` | 263 |
| 222 | `ROOM_CHAT_RESPONSE` | S→C | `INT16 Result` | 8 |
| 223 | `ROOM_CHAT_NOTIFY` | S→C | `char UserID[33]`, `char Msg[257]` | 296 |
| 231 | `ROOM_JOIN_NOTIFY` | S→C | `INT32 ClientIndex`, `char UserID[33]` | 43 |
| 232 | `ROOM_LEAVE_NOTIFY` | S→C | `INT32 ClientIndex` | 10 |

룸 번호는 **0~9**, 방당 정원 **4명** (`PacketManager::CreateComponent`에 하드코딩).

### 입장 시 시퀀스

`ROOM_JOIN_NOTIFY`가 두 가지 용도로 쓰인다. 클라이언트는 구분할 필요 없이
"이 ClientIndex의 캐릭터를 스폰하라"로 처리하면 된다.

```
신규 유저 N이 입장

  S→N : ROOM_ENTER_RESPONSE (Result)
  S→N : ROOM_JOIN_NOTIFY × 기존 유저 수   ← 기존 유저 정보 전달
  S→기존 : ROOM_JOIN_NOTIFY (N)           ← 신규 유저 통보
```

주의: 코드상 `ROOM_ENTER_RESPONSE`가 `EnterUser()` **완료 후** 전송되므로,
실제 도착 순서는 위 그림과 다를 수 있다 (JOIN_NOTIFY들이 먼저 도착 가능).
클라이언트는 순서에 의존하지 말아야 한다.

### 캐릭터 동기화 (1001~1002)

| ID | 이름 | 방향 | 크기 |
|---|---|---|---|
| 1001 | `CHARACTER_SYNC` | C→S | 42 |
| 1002 | `CHARACTER_SYNC_BROADCAST` | S→C | 42 |

```cpp
struct CHARACTER_SYNC_PACKET : public PACKET_HEADER
{
    INT32  ClientIndex;   // 누구의 상태인가
    UINT32 TimeStamp;     // 서버가 브로드캐스트 시점에 덮어씀 (ms)
    UINT32 Sequence;      // 클라이언트가 채움
    float  PosX, PosY, PosZ;
    float  RotY;
    INT32  StateFlag;
    INT32  AnimIndex;
};
```

서버는 수신한 패킷을 그대로 복사하고 `PacketId`를 1002로, `TimeStamp`를
`steady_clock` 기반 ms 값으로 덮어쓴 뒤 **본인 포함 룸 전원**에게 뿌린다.

> 서버는 `ClientIndex`가 실제 송신자인지 검증하지 않는다. 위치 유효성 검사도 없다.
> 지금은 신뢰 클라이언트 모델이다.
>
> `TimeStamp`의 기준점은 `steady_clock` 에폭(= 서버 부팅 시각 기준의 임의 값)이라
> 절대 시각이 아니다. 클라이언트는 **차이값**으로만 써야 한다.

## 클라이언트가 지켜야 할 것

1. **길이 선행 조립.** TCP는 경계를 안 준다. 4바이트(헤더 앞 두 필드)를 먼저
   확보해 `PacketLength`를 읽고, 그만큼 모일 때까지 기다렸다 자른다.
   서버의 1회 수신 버퍼는 256바이트라 `ROOM_CHAT_NOTIFY`(296)는 반드시 쪼개져
   도착한다. 클라이언트 쪽도 동일하다.
2. **자기 `ClientId` 기억.** `SYS_USER_CONNECT_RESPONSE`로 받은 값.
   `CHARACTER_SYNC_BROADCAST`는 본인 것도 되돌아온다.
3. **문자열은 NUL 종단 고정 길이.** `UserID`는 32자 + NUL = 33바이트 고정.
   짧아도 33바이트를 채워 보낸다.
4. **알 수 없는 `PacketId`는 버린다.** 단, `PacketLength`만큼 건너뛰어야
   스트림이 유지된다.

## 프로토콜 변경 절차

1. `Enum_PacketId.h`에 ID 추가 (기존 값 재사용/재배치 금지)
2. `Packet_*.h`에 `pack(1)` 구조체 정의 + `static_assert`
3. `PacketManager::Init`에 핸들러 등록
4. 이 문서의 표 갱신
5. 클라이언트 헤더 동기화

ID는 절대 재사용하지 않는다. 지운 ID는 주석으로 남겨 무덤을 만든다.
