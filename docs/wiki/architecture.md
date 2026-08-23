# 아키텍처

## 한눈에

```
       [ fl-client ]  TCP:9000
             │
   ┌─────────▼──────────────────────────────────────────┐
   │ ServerCore (정적 lib)                              │
   │                                                    │
   │  AccepterThread ×1   ──AcceptEx──▶ stClientInfo[]  │
   │  WorkerThread   ×4   ──GQCS─────▶ ACCEPT/RECV/SEND │
   │                                                    │
   └─────────┬──────────────────────────────▲───────────┘
             │ OnConnect/OnReceive/OnClose  │ SendMsg
   ┌─────────▼──────────────────────────────┴───────────┐
   │ GameServer (exe)                                   │
   │                                                    │
   │  GameServerService : IOCPServer                    │
   │        │                                           │
   │        ▼                                           │
   │  PacketManager ── ProcessThread ×1 ──▶ 핸들러 테이블│
   │        ├── UserManager  (User 풀, 고정 크기)        │
   │        └── RoomManager  (Room 풀, 고정 크기)        │
   └────────────────────────────────────────────────────┘
```

계층은 **ServerCore가 게임을 모르고, GameServer가 소켓을 모른다**는 원칙으로
나뉜다. 경계는 `IOCPServer`의 순수가상 3개(`OnConnect` / `OnReceive` /
`OnClose`)와 `SendMsg` 하나뿐이다. 이 경계는 지금 잘 지켜지고 있다.

## 스레드 모델

현재 살아있는 스레드는 셋이다.

| 스레드 | 개수 | 하는 일 |
|---|---|---|
| AccepterThread | 1 | 32ms마다 전 세션 순회, 비어있고 재사용 대기시간(3초)이 지난 슬롯에 `AcceptEx` 재등록 |
| WorkerThread | 4 (`MAX_IO_WORKER_THREAD`) | `GetQueuedCompletionStatus`로 ACCEPT/RECV/SEND 완료 처리 |
| ProcessThread | 1 (`PacketManager`) | 게임 로직 전부. 큐가 비면 1ms sleep |

`SendThread`는 코드에 있으나 `GameServerService::Run` 경로에서
`CreateSendThread()` 호출이 주석 처리되어 **꺼져 있다**. 현재 송신은
`SendMsg` 호출 스레드(= ProcessThread)에서 곧바로 `WSASend`를 건다.

### 가장 중요한 사실

불변식 전체 목록은 [`CLAUDE.md`](../../CLAUDE.md)에 있다. 그중 여기서
반드시 알아야 할 것:

**게임 로직은 전부 ProcessThread 한 개에서 돈다.** 룸이 10개든 1개든 마찬가지다.
그런데 `Room::m_RoomLock`과 `PacketManager::mLock`이라는 락이 존재한다.

- `PacketManager::mLock`은 **정당하다.** WorkerThread(생산자)와
  ProcessThread(소비자)가 `mInComingPacketUserIndex` / `mSystemPacketQueue`를
  공유하므로 반드시 필요하다.
- **그런데 이 규칙이 이미 깨져 있다.** `PacketManager::ReceivePacketData`는
  WorkerThread에서 실행되면서 `User::mPacketBuffer`에 직접 쓴다. 같은 버퍼를
  ProcessThread가 읽는데 동기화가 없다 (→ review-log **R-016**, 치명).
  아래 「수신」 절의 그림에서 점선을 넘는 지점이 그것이다.
- `Room::m_RoomLock`은 **현재로선 무의미하다.** 룸을 만지는 것은 ProcessThread
  하나뿐이라 경합 자체가 없다. 비용과 복잡도만 낸다. (룸별 병렬 처리로 가면
  이 락은 필요해지는 게 아니라 **더 확실히 사라진다** — 룸 액터 모델에서는
  한 룸을 한 워커만 잡으므로 락 대신 소유권으로 해결된다.)

## 세션 (`stClientInfo`)

- 서버 시작 시 `MAX_CLIENT`(100)개를 **미리 전부 생성**한다. 동적 할당 없음.
- 배열 인덱스가 곧 `clientIndex` = 세션 ID다. `User`, 패킷 큐, 브로드캐스트가
  모두 이 인덱스를 키로 쓴다.
- 연결이 끊기면 슬롯이 비고, 3초(`RE_USE_SESSION_WAIT_TIMESEC`) 후 재사용된다.

> **주의**: 인덱스에 세대(generation) 값이 없다. 인덱스 5번이 끊기고 새 클라이언트가
> 5번 슬롯을 잡았을 때, 큐에 남아있던 이전 유저의 패킷이 새 유저 것으로 처리될 수
> 있다. 3초 지연은 이 창을 좁힐 뿐 닫지 못한다.

## 패킷 수명주기

### 수신

```
WSARecv 완료
  └─ WorkerThread: OnReceive(clientIndex, size, mRecvBuf)
       └─ PacketManager::ReceivePacketData
            ├─ User::SetPacketData  → PacketBuffer에 바이트 누적 (64KB 링)
            └─ EnqueuePacketData    → mInComingPacketUserIndex.push_back  [락]
  └─ 곧바로 BindRecv()로 다음 수신 재등록

ProcessThread 루프
  └─ DequePacketData  [락]  → 해당 User의 PacketBuffer에서 패킷 1개 절단
       └─ ProcessRecvPacket → mRecvFunctionDictionary[packetId] 호출
            └─ 같은 유저 버퍼에 더 있으면 while 루프로 계속 소비
```

핵심은 **`mRecvBuf`(256B, 세션 소유)와 `PacketBuffer`(64KB, User 소유)가 분리**되어
있다는 것. TCP는 경계를 보장하지 않으므로 `PacketBuffer`가 조립을 담당한다.
`PacketBuffer::SetPacketData`는 쓰기 위치가 끝에 닿으면 미소비분을 앞으로 당기는
compaction을 한다.

> **여기가 R-016 지점이다.** `User::SetPacketData`는 WorkerThread에서 실행되고
> `User::GetPacket`은 ProcessThread에서 실행되는데, `PacketBuffer`에는 락이 없다.
> 특히 `SetPacketData`의 compaction이 `RPos`를 되돌리면서 메모리를 이동시킨다.

큐에는 **패킷이 아니라 유저 인덱스만** 들어간다. 실제 데이터는 유저별 버퍼에 있다.
덕분에 큐 원소가 작지만, 같은 유저가 여러 번 enqueue되어도 상관없도록
"버퍼가 빌 때까지 계속 읽는" 루프가 필요한 구조다.

### 송신

```
핸들러
  └─ SendPacketFunc(clientIndex, size, buffer)     (std::function)
       └─ GameServerService의 람다 → IOCPServer::SendMsg
            └─ stClientInfo::SendMsg   [mSendLock]
                 ├─ mSendBuf(4KB)에 복사, mSendPos 전진
                 └─ 전송중이 아니면 SendIO()
                      ├─ mSendBuf → mSendingBuf 복사
                      ├─ WSASend
                      └─ mSendPos = 0
WSASend 완료
  └─ WorkerThread: SendCompleted → mIsSending = false
```

**이중 버퍼(`mSendBuf` / `mSendingBuf`)** 구조다. 전송 중에 새 데이터가 들어와도
in-flight 버퍼를 건드리지 않으려는 의도.

> **알려진 결함** (→ [review-log.md](review-log.md) R-001, R-002)
> - `mSendPos + dataSize > MAX_SOCK_SENDBUF`이면 `mSendPos = 0`으로 되돌린다.
>   미전송 데이터를 조용히 덮어쓴다.
> - `SendCompleted`(WorkerThread)가 `mIsSending`을 락 없이 쓰고,
>   `SendMsg`(ProcessThread)가 락 안에서 그 값을 읽는다. 서로 다른 락 도메인이다.

## 컴포넌트 책임

| 클래스 | 소유하는 것 | 책임 |
|---|---|---|
| `IOCPServer` | 리슨 소켓, IOCP 핸들, `stClientInfo` 풀, 스레드들 | 소켓 I/O 전부. 게임 개념 없음 |
| `stClientInfo` | 소켓 1개, 수신/송신 버퍼, OVERLAPPED 3개 | 한 연결의 I/O 상태 |
| `PacketBuffer` | 64KB 바이트 버퍼 | TCP 스트림 → 패킷 경계 복원 |
| `GameServerService` | `PacketManager` | ServerCore ↔ GameServer 어댑터. 콜백을 큐잉으로 변환 |
| `PacketManager` | `UserManager`, `RoomManager`, 큐 2개, ProcessThread | 패킷 디스패치 + 핸들러 구현 |
| `UserManager` | `User` 풀(고정), ID→인덱스 맵 | 로그인 상태, 중복 로그인 판정 |
| `User` | `PacketBuffer`, 도메인 상태, 현재 룸 번호 | 한 유저의 세션 스코프 상태 |
| `RoomManager` | `Room` 풀(고정, 0~9) | 룸 번호 → 룸 조회, 입퇴장 위임 |
| `Room` | 유저 목록 | 브로드캐스트, 정원 관리 |

`Room`은 `User`를 `shared_ptr`로 잡지만 **소유하지 않는다** — 실제 소유자는
`UserManager`의 풀이다. `User` 객체는 서버 수명 내내 재사용되며 `Clear()`로
초기화될 뿐 해제되지 않는다.

## 유저 상태 기계

```
NONE ──LOGIN_REQUEST──▶ LOGIN ──ROOM_ENTER_REQUEST──▶ ROOM
  ▲                       │                            │
  └───────────────────────┴────────────────────────────┘
              연결 종료 / ROOM_LEAVE_REQUEST
```

`DOMAIN_STATE`는 `NONE / LOGIN / ROOM` 셋뿐이다. 연결 종료 시
`ClearConnectionInfo`가 상태를 보고 룸 퇴장과 유저 정보 삭제를 순서대로 한다.

> 현재 핸들러들은 이 상태를 **검사하지 않는다.** 로그인 없이
> `ROOM_ENTER_REQUEST`를 보내도 처리 경로를 탄다.

## 고정 상수 (`ServerCore/Define.h`)

| 상수 | 값 | 의미 |
|---|---|---|
| `SERVER_PORT` | 9000 | 리슨 포트 |
| `MAX_CLIENT` | 100 | 동시 접속 = 세션 풀 크기 = User 풀 크기 |
| `MAX_IO_WORKER_THREAD` | 4 | IOCP 워커 수 |
| `MAX_SOCKBUF` | 256 | 세션당 수신 버퍼 (1회 WSARecv 크기) |
| `MAX_SOCK_SENDBUF` | 4096 | 세션당 송신 버퍼 |
| `PACKET_DATA_BUFFER_SIZE` | 65536 | User당 패킷 조립 버퍼 |
| `RE_USE_SESSION_WAIT_TIMESEC` | 3 | 세션 슬롯 재사용 대기 |

룸 설정은 `Define.h`가 아니라 `PacketManager::CreateComponent`에 하드코딩되어
있다 (룸 0~9, 방당 4명). 코드에 `TODO : 하드코딩 제거` 주석이 달려 있다.

## 아직 없는 것

의도적으로든 아니든 현재 존재하지 않는 것들. 설계 논의의 출발점.

- **틱이 없다.** ProcessThread는 이벤트 구동 + 1ms sleep 폴링이다. 고정 주기가
  없어서 이동 보간·타임아웃·주기적 정리를 걸 자리가 없다.
- **인증이 없다.** 비밀번호를 받지만 검증하지 않는다. `User::mAuthToken`은
  선언만 되어 있다.
- **영속화가 없다.** Redis 코드는 주석 처리 상태. 서버가 죽으면 전부 사라진다.
- **테스트가 없다.**
- **메트릭이 없다.** 로그는 있으나 모든 recv/send마다 `info` 레벨로 찍는다.
