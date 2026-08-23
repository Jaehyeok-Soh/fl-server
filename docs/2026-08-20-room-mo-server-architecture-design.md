# [설계 문서] 상용급 C++ 룸 MO 게임 서버 아키텍처 및 단계별 전환 로드맵

- **작성일:** 2026-08-20
- **프로젝트:** fl-server (GameServer & ServerCore)
- **목표:** 현재 프로토타입 수준의 C++ IOCP 서버를 상용 출시(Production-Ready) 가능한 고성능 룸 기반 MO (Multiplayer Online) 게임 서버로 단계적 전환

---

## 1. 개요 및 전환 목표

### 1.1 현재 구조의 핵심 한계점 (As-Is)
1. **네트워크 I/O 위험성:** `ClientInfo` 내 단일 4KB 버퍼 사용 및 오버플로우 시 버퍼 위치 리셋(`mSendPos = 0`)으로 인한 패킷 손실 및 메모리 오염.
2. **동시성 모델 병목:** `PacketManager`의 단일 스레드 패킷 처리 루프(`sleep_for(1ms)`) 및 `Room::m_RoomLock` Mutex 경쟁.
3. **메모리 파편화:** 패킷 수발신마다 `shared_ptr<char[]>`를 `make_shared`로 동적 힙 할당하여 고빈도 패킷 발생 시 극심한 힙 경합 발생.
4. **수명주기 안전성 미비:** 소켓 연결 해제 도중 IOCP 워커 스레드가 원시 세션 포인터에 접근할 때 발생할 수 있는 Use-After-Free(UAF) 크래시 위험.

### 1.2 목표 아키텍처 (To-Be)
1. **완전 비동기 I/O & Scatter-Gather:** `SendQueue` 및 `WSABUF` 일괄 전송(Batching)으로 Syscall 최소화.
2. **Lock-Free JobQueue (Strand / Actor 모델):** 각 `Room`이 독립된 `JobQueue`를 보유하여 룸 내부 무잠금(Zero-Lock) 직렬 실행 및 멀티코어 확장성 확보.
3. **TLS 기반 제로 힙 할당 버퍼링:** `thread_local` 메모리 청크 기반 `SendBuffer`와 원형 `RecvBuffer` 구축.
4. **글로벌 타이머 스케줄러:** Min-Heap 기반 `JobTimer`로 고정밀 룸 틱(30Hz/60Hz) 및 지연 작업 처리.

---

## 2. 전체 시스템 아키텍처 다이어그램

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                                APPLICATION LAYER (GameServer)                           │
│                                                                                         │
│  ┌─────────────────────────────────┐           ┌──────────────────────────────────────┐ │
│  │         UserManager             │           │             RoomManager              │ │
│  │  - Session <-> User Mapping     │           │  - Active Rooms Pool & Registry      │ │
│  │  - User State Machine           │           │  - Matchmaking & Room Lifecycle      │ │
│  └────────────────┬────────────────┘           └──────────────────┬───────────────────┘ │
│                   │                                               │                     │
│  ┌────────────────▼────────────────┐           ┌──────────────────▼───────────────────┐ │
│  │              User               │           │           Room (JobQueue 보유)        │ │
│  │  - Player Transform (Pos, Rot)  │           │  - Room Tick (30Hz/60Hz)             │ │
│  │  - Input History Buffer         │◀──────────│  - Character Sync & Interpolation    │ │
│  │  - Dead Reckoning Context       │ (WeakRef) │  - Hit/Action Rule Validation        │ │
│  └─────────────────────────────────┘           │  - Broadcast Buffer Batching         │ │
│                                                └──────────────────▲───────────────────┘ │
├───────────────────────────────────────────────────────────────────┼─────────────────────┤
│                               CONCURRENCY & SCHEDULER LAYER       │ (Push Job)          │
│                                                                   │                     │
│  ┌────────────────────────────────────────────────────────────────┴──────────────────┐  │
│  │                              JobQueue (Strand / Actor)                            │  │
│  │   - Lock-Free / SpinLock Task Queue                                               │  │
│  │   - Guaranteed Sequential Execution per Room (Zero Mutex inside Room logic)       │  │
│  └────────────────────────┬──────────────────────────────────────────────────────────┘  │
│                           │                                                             │
│  ┌────────────────────────▼────────────────────┐  ┌──────────────────────────────────┐  │
│  │             Global ThreadPool               │  │       Global JobTimer Scheduler  │  │
│  │  - N Worker Threads (e.g. CPU Core * 2)     │  │  - Priority Queue (Min-Heap)     │  │
│  │  - Job Dispatch & Work Stealing             │◀─┼── Tick Registration (Every 33ms) │  │
│  └─────────────────────────────────────────────┘  └──────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────────────────────────────┤
│                                 NETWORK & MEMORY CORE                                   │
│                                                                                         │
│  ┌─────────────────────────────────┐           ┌──────────────────────────────────────┐ │
│  │     PacketSession (Ref-Count)   │           │      TLS Memory Pool & SendBuffer    │ │
│  │  - enable_shared_from_this      │           │  - thread_local SendBufferChunk      │ │
│  │  - Atomic Disconnect Exchange   │           │  - Zero-Copy Buffer Slicing          │ │
│  │  - RingBuffer Recv Framing      │           │  - Custom Memory Pool                │ │
│  └────────────────┬────────────────┘           └──────────────────┬───────────────────┘ │
│                   │                                               │                     │
│  ┌────────────────▼───────────────────────────────────────────────▼──────────────────┐  │
│  │               SendQueue & Scatter-Gather I/O (WSABUF Batching)                     │  │
│  │  - Multiple Packet Send coalesced into 1 WSASend call                             │  │
│  └────────────────────────────────┬──────────────────────────────────────────────────┘  │
│                                   │                                                     │
│  ┌────────────────────────────────▼──────────────────────────────────────────────────┐  │
│  │                       IocpCore / Listener / Socket Engine                         │  │
│  │  - AcceptEx Pre-posting Pool | GetQueuedCompletionStatus Overlapped Dispatch      │  │
│  └───────────────────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. 핵심 데이터 흐름 (Zero-Lock Data Flow)

```
[클라이언트 패킷 도착]
      │ (WSARecv 완료)
      ▼
1. IocpCore WorkerThread:
   - RecvBuffer(링버퍼)에서 헤더 검증 및 1개 패킷 조립
   - Session이 속한 Room 획득 (`shared_ptr<Room>`)
      │
      ▼
2. Room::PushJob (Strand Enqueue):
   - Room의 `JobQueue`에 `[PacketHandler(패킷 데이터)]` 작업 등록
   - 해당 룸이 현재 실행 중이 아니라면 스레드풀의 실행 큐에 Room 등록
      │
      ▼
3. WorkerThread (스레드풀):
   - Room의 `JobQueue`에서 Job들을 순차적으로 Pop하여 실행
   - **★ 핵심: Room 내부 로직(이동, 스킬, 판정)에는 어떤 Mutex 락도 필요 없음 (완전 직렬 실행 보장)**
      │
      ▼
4. Room::Update / Broadcast (30Hz/60Hz Tick):
   - 룸 내부 모든 유저 위치/상태 계산 (Dead Reckoning)
   - TLS `SendBuffer`에 `CHARACTER_SYNC_BROADCAST` 1회 직렬화
   - 룸 내 모든 `UserSession->Send(sendBuffer)` 호출
      │
      ▼
5. Session::SendQueue (Scatter-Gather I/O):
   - 전송 버퍼들을 `vector<WSABUF>`로 모아서 **단 1회의 `WSASend`로 일괄 커널 전송**
```

---

## 4. 글로벌 상용 게임 서버 아키텍처 및 오픈소스 레퍼런스

### 4.1 미국 / 서구권 (GDC & AAA 글로벌 표준)
1. **Gaffer on Games (Glenn Fiedler)**
   - *핵심 주제:* Client-Side Prediction, Server Reconciliation, Snapshot Interpolation.
   - *참고 URL:* [https://gafferongames.com/categories/game-networking/](https://gafferongames.com/categories/game-networking/)
2. **Valve Source Multiplayer Networking**
   - *핵심 주제:* 틱 기반 시뮬레이션, 지연 보상(Lag Compensation), 히트박스 되감기.
   - *참고 URL:* [Valve Source Multiplayer Networking](https://developer.valvesoftware.com/wiki/Source_Multiplayer_Networking)
3. **Photon Quantum (Exit Games)**
   - *핵심 주제:* 룸 기반 결정론적(Deterministic) 롤백 ECS 아키텍처. 룸을 독립 액터로 격리.
   - *참고 URL:* [Photon Quantum Architecture](https://doc.photonengine.com/quantum/current/quantum-101)
4. **TrinityCore / AzerothCore (C++ 오픈소스)**
   - *GitHub:* [TrinityCore/TrinityCore](https://github.com/TrinityCore/TrinityCore), [azerothcore/azerothcore-wotlk](https://github.com/azerothcore/azerothcore-wotlk)
   - *배울 점:* `WorldSocket`, `AsyncAcceptor`, `MessageBuffer` 버퍼 관리, 인스턴스 틱 스케줄러.
5. **Boost.Asio / Asio Strand**
   - *핵심 주제:* 뮤텍스 없는 비동기 핸들러 직렬화 패턴(Strand).

### 4.2 중국권 (Tencent / NetEase 분산 액터 프레임워크)
1. **NoahGameFrame (NF) & Squick (C++20/C++23)**
   - *GitHub:* [ketoo/NoahGameFrame](https://github.com/ketoo/NoahGameFrame), [i0gan/Squick](https://github.com/i0gan/Squick)
   - *배울 점:* 상용 MOBA/MMO 분산 서버 구조, 플러그인 아키텍처, 고성능 C++ Actor Model.
2. **KBEngine (C++ / Python)**
   - *GitHub:* [kbengine/kbengine](https://github.com/kbengine/kbengine)
   - *배울 점:* `Cellapp`(룸/인스턴스)과 `Baseapp`(세션/게이트)의 분리 설계.
3. **Skynet (Cloud Wu)**
   - *GitHub:* [cloudwu/skynet](https://github.com/cloudwu/skynet)
   - *배울 점:* C 기반 경량 액터 모델 및 메시지 디스패치 파이프라인.

### 4.3 일본권 (CEDEC / 캡콤 / 세가 / Softgear)
1. **CEDEC 룸 서버 아키텍처**
   - *핵심 주제:* 로비/매칭 서버와 격리된 룸 인스턴스 서버의 분리, GGPO 기반 롤백 넷코드.
2. **Softgear STRIX Engine**
   - *배울 점:* 일본 상용 C++ 고성능 룸/동기화 릴레이 서버 엔진 아키텍처.

### 4.4 한국권 (상용 IOCP 표준 패턴)
1. **Rookiss C++ Server Engine**
   - *핵심 패턴:* `IocpCore` - `Listener` - `Session` - `PacketSession` 구조, TLS Buffer, JobQueue/JobTimer.
2. **ProudNet**
   - *핵심 패턴:* RMI 자동 직렬화 및 고성능 Send Coalescing(패킷 뭉쳐 보내기) 기법.

---

## 5. 단계별(Phase 1 ~ Phase 4) 상세 전환 로드맵 및 체크리스트

### [Phase 1] 네트워크 I/O 및 메모리/세션 코어 혁신

#### Step 1.1: TLS 기반 SendBuffer 및 고정밀 RecvBuffer (링버퍼)
- [ ] `SendBufferChunk` 클래스 구현 (64KB 청크 단위 `thread_local` 할당기)
- [ ] `SendBuffer` (Zero-copy Slicing 및 `shared_ptr<SendBuffer>` 반환) 구현
- [ ] `RecvBuffer` 구현 (`mReadPos`, `mWritePos`, `mCapacity` 기반 순수 원형 링버퍼)
- [ ] **검증:** 1,000,000회 연속 패킷 버퍼 할당/해제 시 힙 메모리 할당 0회 확인

#### Step 1.2: 완전 비동기 SendQueue & Scatter-Gather I/O
- [ ] `Session` 내 `queue<SendBufferRef> mSendQueue` 및 `atomic<bool> mRegisteredSend` 구현
- [ ] `Send()` 시 큐에 적재 후 `exchange(true)`로 전환된 단 1개 스레드가 모든 대기 버퍼를 `vector<WSABUF>`로 일괄 `WSASend`
- [ ] `OnSendCompleted()` 콜백에서 잔여 큐 버퍼 자동 연쇄 전송(Send Chaining) 처리
- [ ] **검증:** 100개 스레드의 동시 `Send()` 난사 테스트 시 패킷 순서 보장 및 메모리 오염 0건 확인

#### Step 1.3: 세션 참조 카운팅 수명주기 관리
- [ ] `class Session : public enable_shared_from_this<Session>` 상속 구조 적용
- [ ] 비동기 Overlapped Context에 `shared_from_this()` 등록하여 I/O 완료 시점까지 세션 수명 보장
- [ ] `Disconnect()` 시 `atomic<bool> mIsConnected`의 CAS(`compare_exchange`)를 통해 `OnDisconnected()` 정확히 단 1회 호출 보장
- [ ] **검증:** 수천 개 소켓의 동시 접속/강제 끊김 시 크래시(Use-After-Free) 발생 0건 확인

#### Step 1.4: IOCP 엔진 분리 및 모듈화
- [ ] `IocpCore` (순수 IOCP 핸들 등록 및 `GetQueuedCompletionStatus` 디스패치 전담) 분리
- [ ] `Listener` (`AcceptEx` 다중 사전 등록 및 소켓 수락 전담) 분리
- [ ] `ServerService` (전체 세션 풀 관리 및 시작/종료 오케스트레이션) 분리

#### Step 1.5: 안전한 패킷 파싱 및 프레이밍 파이프라인
- [ ] `PacketHeader` 검증 (`PacketLength > sizeof(PACKET_HEADER)` 및 `PacketLength <= MAX_PACKET_SIZE`)
- [ ] TCP Fragmentation(쪼개짐) 및 Coalescing(뭉침)을 100% 정상 조립하는 파싱 루프 구현

---

### [Phase 2] JobQueue (Strand) 동시성 모델 및 타이머 스케줄러

#### Step 2.1: 범용 Functor/Lambda 래핑 Job 구조
- [ ] `IJob` 인터페이스 및 템플릿 기반 `Job(Func&&, Args&&...)` 구현

#### Step 2.2: Room별 JobQueue (Strand) 및 Global ThreadPool
- [ ] `class JobQueue : public enable_shared_from_this<JobQueue>` 구현
- [ ] `Push(job)` 시 큐가 비어있었으면 전역 `ThreadPool` 실행 큐에 `JobQueue` 자체를 등록
- [ ] 워커 스레드가 `JobQueue::Execute()`를 호출하여 내부 작업들을 순차 실행
- [ ] **★ 결과: Room 내부의 모든 Mutex 락 완전 제거**

#### Step 2.3: Min-Heap 기반 JobTimer (PriorityQueue) 스케줄러
- [ ] `TimerItem(ExecuteTick, JobRef)` 우선순위 큐 구현
- [ ] 현재 틱 기준으로 만료된 타이머 작업을 꺼내어 해당 `JobQueue`에 Push
- [ ] 고정밀 Tick 루프 (`Room::Update` 30Hz/60Hz) 등록 및 구동

---

### [Phase 3] 룸(Room) MO 도메인 고도화 및 동기화 파이프라인

#### Step 3.1: User 상태 머신 정립
- [ ] `NONE -> CONNECTED -> LOGIN -> ROOM -> INGAME` 명확한 상태 전이 정의 및 불법 패킷 차단

#### Step 3.2: Room 생명주기 관리
- [ ] `Room` 상태: `PREPARING -> RUNNING -> GAME_OVER -> DESTROYED`
- [ ] 유저 입장/퇴장 시의 안전한 브로드캐스트 및 룸 풀링(Room Pool) 재사용

#### Step 3.3: CharacterSync 브로드캐스트 최적화
- [ ] 동기화 패킷을 1회만 직렬화하여 동일한 `SendBufferRef`를 룸 내 모든 세션에 전달
- [ ] 클라이언트 핑(Ping/Pong) 측정 및 타임스탬프 기반 딜레이 보정 구조

---

### [Phase 4] 상용 출시 안정성 및 검증 (Production-Ready QA)

#### Step 4.1: Crash Dump 및 예외 처리
- [ ] Windows `SetUnhandledExceptionFilter`를 등록하여 서버 비정상 종료 시 Full MiniDump(`.dmp`) 자동 생성

#### Step 4.2: DummyClient 고도화 및 부하 테스트
- [ ] 100 ~ 500개 세션 동시 접속 및 랜덤 이동/채팅 패킷 30Hz 난사 봇 구현
- [ ] 서버 CPU 점유율, 메모리 누수, 패킷 처리 지연시간(RTT) 측정
- [ ] Graceful Shutdown(안전 종료) 검증

---

## 6. 결론 및 권장 시작 지점

가장 먼저 시작할 작업은 **`ServerCore` 프로젝트의 [Step 1.1 & 1.2: TLS SendBuffer / RecvBuffer / SendQueue]** 구현입니다. 하부 버퍼 및 I/O 시스템이 완벽히 자리 잡히면, 상위 룸 로직과 JobQueue는 흔들림 없이 안정적으로 결합할 수 있습니다.
