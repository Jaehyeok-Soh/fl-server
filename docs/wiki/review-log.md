# 검토 로그

개발/수정은 사람이, 검토/피드백은 LLM이 한다.
여기엔 **지적 → 판단 → 결정 → 결과**가 한 줄기로 남는다.

## 기록 형식

```markdown
### R-000 — 한 줄 요약
| | |
|---|---|
| 등급 | 치명 / 높음 / 보통 / 낮음 |
| 위치 | `파일` › `심볼` — **줄 번호 금지**(한 커밋이면 거짓말이 된다) |
| 상태 | 제안됨 / 수정중 / 해결 / 보류 / 반려 |
| 갱신 | YYYY-MM-DD |

**증상** 어떤 입력·타이밍에서 어떻게 잘못되는가.
**원인** 코드 수준의 이유.
**제안** 무엇을 어떻게.
**결정** (사람이 작성) 수용/보류/반려 + 이유. 반려도 반드시 남긴다.
**결과** (사람이 작성) 커밋 해시, 실제로 확인한 방법.
```

**등급 기준** — 치명: 데이터 손상·크래시·보안 / 높음: 기능이 틀리게 동작 /
보통: 구조·유지보수 / 낮음: 취향·정리

번호는 재사용하지 않는다. 반려된 항목도 지우지 않는다.

**반려한 항목은 [decisions.md](decisions.md)의 「반려 기록」에 X-번호로도 옮긴다.**
반려는 결함이 아니라 판단의 증거이고, 같은 제안이 다시 올라올 때 가리킬 곳이
필요하기 때문이다. LLM 지적이 100% 채택되는 로그는 실패한 로그다 (→ D-009).

---

## 미해결 요약

| ID | 등급 | 요약 | 상태 |
|---|---|---|---|
| R-001 | 치명 | 송신 버퍼가 미전송 데이터를 덮어씀 | 제안됨 |
| R-002 | 치명 | `mIsSending` 데이터 레이스 → 중복 WSASend | 제안됨 |
| R-003 | 치명 | 로그인 시 UserID를 1바이트만 복사 → 힙 범위 밖 읽기 | 제안됨 |
| R-004 | 치명 | `Room::LeaveUser`가 10바이트 버퍼를 43바이트로 전송 | 제안됨 |
| R-005 | 치명 | 패킷 크기 검증 없이 `reinterpret_cast` | 제안됨 |
| R-006 | 높음 | `LOGIN_RESPONSE`가 수신 핸들러에 등록됨 | 제안됨 |
| R-007 | 높음 | `NotifyChat`이 짧은 문자열에서 33바이트 고정 복사 | 제안됨 |
| R-008 | 높음 | `mCurrentUserCnt` 증감 누락 → 정원 체크가 죽은 코드 | 제안됨 |
| R-009 | 높음 | 핸들러가 `DOMAIN_STATE`를 검사하지 않음 | 제안됨 |
| R-010 | 높음 | 세션 인덱스에 세대값 없음 → 패킷 오배달 | 제안됨 |
| R-011 | 보통 | 매 수신마다 바이너리를 문자열로 로깅 | 제안됨 |
| R-012 | 보통 | 헤더 크기가 암묵적, `static_assert` 없음 | 제안됨 |
| R-013 | 보통 | `Room::m_RoomLock`이 아무것도 지키지 않음 | 제안됨 |
| R-014 | 낮음 | `AcceptCompletion`이 초기화 안 된 주소를 로깅 | 제안됨 |
| R-015 | 낮음 | 룸 설정 하드코딩 | 제안됨 |
| R-016 | 치명 | `PacketBuffer`가 두 스레드에 무보호 공유 | 제안됨 |
| R-017 | 치명 | `PacketLength=0` 패킷 하나로 서버 전체 정지 (원격 DoS) | 제안됨 |
| R-018 | 낮음 | `SendPacketFunc` 시그니처가 계층마다 다름 (UINT16/UINT32) | 제안됨 |
| R-019 | 낮음 | `.gitignore`가 fl-client 저장소에서 복사됨 | 제안됨 |

---

## 2026-08-20 — 최초 전수 검토

전체 2,171줄 리딩. 이하 15건.

*(추가: 불변식 목록을 작성하다가 R-016을 발견. 문서 하단에 별도 기록.)*

### R-001 — 송신 버퍼가 미전송 데이터를 덮어씀
| | |
|---|---|
| 등급 | 치명 |
| 위치 | `ClientInfo.cpp` › `stClientInfo::SendMsg` |
| 상태 | 제안됨 |
| 갱신 | 2026-08-20 |

**증상** 4인 룸에서 `CHARACTER_SYNC`를 20Hz로 주고받으면 특정 클라이언트가
캐릭터 하나를 잃거나, 채팅이 중간에 잘려 도착한다. 재현이 산발적이다.

**원인**
```cpp
if ((mSendPos + dataSize_) > MAX_SOCK_SENDBUF)
    mSendPos = 0;
```
`mSendBuf`(4096B)가 가득 차면 쓰기 위치를 0으로 되돌린다. 아직 `WSASend`로
나가지 않은 데이터 위에 새 패킷을 덮어쓴다. 실패를 알리지도 않고 `true`를
반환한다. `ROOM_CHAT_NOTIFY`가 296바이트라 4KB는 채팅 14개면 찬다.

**제안** 링버퍼 + 대기 큐로 교체한다. 전송 중이면 새 데이터를 pending에 쌓고,
`SendCompleted`에서 pending이 있으면 이어서 보낸다. 버퍼가 진짜 넘치면
덮어쓰지 말고 **세션을 끊는다** — 조용한 데이터 손상보다 명시적 절단이 낫다.

**결정**
**결과**

---

### R-002 — `mIsSending` 데이터 레이스 → 중복 WSASend
| | |
|---|---|
| 등급 | 치명 |
| 위치 | `ClientInfo.cpp` › `stClientInfo::SendMsg` / `SendIO` / `SendCompleted` |
| 상태 | 제안됨 |
| 갱신 | 2026-08-20 |

**증상** 부하가 걸리면 간헐적으로 `WSASend` 실패 또는 잘못된 바이트가 나간다.

**원인** `SendCompleted`는 **WorkerThread**에서 `mIsSending = false`를 락 없이
쓴다. `SendMsg`는 **ProcessThread**에서 `mSendLock`을 잡고 `if (!mIsSending)`을
읽는다. 서로 다른 락 도메인이라 상호배제가 성립하지 않는다.

`mIsSending`이 `atomic<bool>`이라 찢어진 읽기는 없지만, 문제는 원자성이 아니라
**검사와 행동 사이의 간극**이다. `SendCompleted`가 false로 바꾼 직후,
아직 in-flight OVERLAPPED가 정리되기 전에 `SendMsg`가 `SendIO`를 재진입하면
같은 `mSendOverlappedEx`로 두 번째 `WSASend`가 걸린다.

**제안** `SendCompleted`도 `mSendLock`을 잡게 한다. R-001의 링버퍼 재작성과
같이 처리하는 게 자연스럽다 — 완료 시점에 락 안에서 pending을 확인하고
이어 보내는 구조면 이 레이스는 구조적으로 사라진다.

**결정**
**결과**

---

### R-003 — 로그인 시 UserID를 1바이트만 복사 → 힙 범위 밖 읽기
| | |
|---|---|
| 등급 | 치명 |
| 위치 | `PacketManager::ProcessLogin` → `UserManager::AddUser` → `User::SetLogin` |
| 상태 | 제안됨 |
| 갱신 | 2026-08-20 |

**증상** 로그인한 유저의 ID가 쓰레기 문자열이 된다. 중복 로그인 차단이
작동하지 않는다 (같은 ID로 두 번 로그인해도 둘 다 통과).

**원인**
```cpp
mUserManager->AddUser(make_shared<char>(*pLoginReqPacket->UserID), clientIndex_);
```
`UserID`는 `char[33]`이고 `*UserID`는 **첫 글자 하나**다. `make_shared<char>(c)`는
1바이트짜리 힙 블록을 만든다. NUL 종단이 없다.

이 포인터가 `User::SetLogin`의 `mUserID = userID_.get()`으로 넘어가
`std::string`을 만든다. `std::string(const char*)`은 NUL을 만날 때까지 읽으므로
**1바이트 할당 뒤쪽 힙을 계속 읽는다.** `UserManager::AddUser`의
`mUserIDDictionary.insert(...)`도 같은 포인터를 쓴다.

결정적으로 `FindUserIndexByID`는 패킷의 **정상 문자열**로 조회하는데
맵에는 쓰레기 문자열이 들어가 있어 절대 매칭되지 않는다.

**제안** 포인터를 넘기지 말고 값으로 넘긴다.
```cpp
// 패킷 필드가 NUL 종단인지 먼저 보장한 뒤
std::string userId(pLoginReqPacket->UserID,
                   strnlen(pLoginReqPacket->UserID, MAX_USER_ID_LEN));
mUserManager->AddUser(userId, clientIndex_);
```
`AddUser` / `SetLogin` 시그니처를 `const std::string&`로 바꾼다.
`shared_ptr<char>`를 쓸 이유가 없다.

**결정**
**결과**

---

### R-004 — `Room::LeaveUser`가 10바이트 버퍼를 43바이트로 전송
| | |
|---|---|
| 등급 | 치명 |
| 위치 | `Room.cpp` › `Room::LeaveUser` |
| 상태 | 제안됨 |
| 갱신 | 2026-08-20 |

**증상** 유저가 룸을 나가면 남은 유저들의 클라이언트가 이상한 패킷을 받는다.
`PacketLength`가 43으로 오는데 실제 `ROOM_LEAVE_PACKET`은 10바이트라,
클라이언트 파서가 다음 패킷 33바이트를 먹어치운다. 이후 스트림 전체가 깨진다.

**원인**
```cpp
ROOM_LEAVE_PACKET stayUserPkt = {};
stayUserPkt.PacketLength = sizeof(ROOM_JOIN_PACKET);        // ← 43
SendPacketFunc(..., sizeof(ROOM_JOIN_PACKET), MakePacketBuffer(stayUserPkt));
//                  ↑ 43                       ↑ 10바이트만 할당
```
`ROOM_JOIN_PACKET` 복붙 흔적. `MakePacketBuffer`는 `sizeof(T)` = 10바이트를
할당하는데 `SendMsg`는 43바이트를 `CopyMemory`한다. **힙 33바이트 초과 읽기**이자
와이어 포맷 오염이다.

**제안** 셋 다 `sizeof(ROOM_LEAVE_PACKET)`으로 고친다. 근본적으로는
`SendPacketFunc`가 크기를 따로 받는 것 자체가 이 실수를 부른다 —
템플릿 헬퍼 `SendPacket(idx, pkt)` 하나로 묶어 크기 인자를 없애면
이 부류가 구조적으로 불가능해진다. (R-012의 `static_assert`와 함께 보면 좋다.)

**결정**
**결과**

---

### R-005 — 패킷 크기 검증 없이 `reinterpret_cast`
| | |
|---|---|
| 등급 | 치명 |
| 위치 | `PacketManager` › `ProcessEnterRoom` / `ProcessRoomChatMessage` / `ProcessCharacterSync` |
| 상태 | 제안됨 |
| 갱신 | 2026-08-20 |

**증상** 클라이언트가 `PacketId=206`, `PacketLength=6`(헤더만)인 패킷을 보내면
서버가 힙 범위 밖을 읽는다. 운이 나쁘면 크래시, 좋으면 쓰레기 룸 번호.

**원인** `PacketBuffer::GetPacket`은 `PacketLength`만큼만 할당한다.
`ProcessLogin`만 `LOGIN_REQUEST_PACKET_SIZE != packetSize_` 검사를 하고,
`ProcessEnterRoom` / `ProcessRoomChatMessage` / `ProcessCharacterSync`는
검사 없이 바로 캐스팅한다.

**제안** 핸들러마다 검사를 흩뿌리지 말고 **디스패치 한 곳**에서 막는다.
`ProcessRecvPacket`에서 패킷 ID → 기대 크기 테이블을 조회해 불일치하면
버리고 로그를 남긴다. 등록 시점에 크기를 함께 등록하는 형태가 좋다.

```cpp
Register<ROOM_ENTER_REQUEST_PACKET>(PACKET_ID::ROOM_ENTER_REQUEST,
                                    &PacketManager::ProcessEnterRoom);
```
이러면 핸들러가 늘어날 때마다 검증을 잊을 수가 없다. 신뢰 경계는
게으르게 처리하지 않는다.

**결정**
**결과**

---

### R-006 — `LOGIN_RESPONSE`가 수신 핸들러에 등록됨
| | |
|---|---|
| 등급 | 높음 |
| 위치 | `PacketManager::Init` |
| 상태 | 제안됨 |
| 갱신 | 2026-08-20 |

**증상** 클라이언트가 서버→클라 전용 패킷(202)을 보내면 로그인 처리가 실행된다.

**원인**
```cpp
mRecvFunctionDictionary[PACKET_ID::LOGIN_REQUEST]  = &PacketManager::ProcessLogin;
mRecvFunctionDictionary[PACKET_ID::LOGIN_RESPONSE] = &PacketManager::ProcessLogin;  // ←
```
`LOGIN_RESPONSE_PACKET`은 8바이트라 `ProcessLogin`의 크기 검사(72)에 걸려
지금은 조용히 리턴한다. 즉 **현재는 우연히 막혀 있다.** 하지만 의도가 아니다.

**제안** 줄 삭제. 겸사겸사 ID 대역 규칙을 세워두면 좋다 — 요청/응답 대역을
나누고 디스패처가 응답 대역을 거부하게 하면 이 부류가 반복되지 않는다.

**결정**
**결과**

---

### R-007 — `NotifyChat`이 짧은 문자열에서 33바이트 고정 복사
| | |
|---|---|
| 등급 | 높음 |
| 위치 | `Room.cpp` › `Room::NotifyChat` |
| 상태 | 제안됨 |
| 갱신 | 2026-08-20 |

**증상** 채팅 알림의 `UserID` 뒤쪽에 쓰레기 바이트가 붙는다. ID가 15자를
넘는지 여부에 따라 증상이 달라진다(MSVC SSO 경계).

**원인**
```cpp
CopyMemory(roomChatNtfyPkt.UserID, userID_, sizeof(roomChatNtfyPkt.UserID)); // 33
```
`userID_`는 `PacketManager`에서 `reqUser->GetUserId().c_str()`로 넘어온 값이다.
`GetUserId()`는 `std::string`을 **값으로** 반환하므로 임시 객체이고, ID가
짧으면 그 문자열의 실제 크기는 33바이트가 아니다. 33바이트를 무조건 복사하면
범위 밖을 읽는다. (임시 객체 수명은 호출식 끝까지라 댕글링은 아니다.)

같은 함수의 `Msg` 복사는 원본도 `char[257]`이라 안전하다.

**제안** `strncpy_s(..., _TRUNCATE)`를 쓴다. 바로 위 `NotifyNewGuest`가 이미
그렇게 하고 있으니 그 패턴을 따르면 된다. 더불어 `GetUserId()`는
`const std::string&` 반환으로 바꿔 매 호출 복사를 없앤다.

**결정**
**결과**

---

### R-008 — `mCurrentUserCnt` 증감 누락
| | |
|---|---|
| 등급 | 높음 |
| 위치 | `UserManager::AddUser` / `UserManager::DeleteUserInfo` |
| 상태 | 제안됨 |
| 갱신 | 2026-08-20 |

**증상** 접속 정원이 무제한이 된다. `LOGIN_USER_USED_ALL_OBJ`는 절대 반환되지 않는다.

**원인** `IncreaseUserCnt` / `DecreaseUserCnt`가 정의만 되어 있고 **아무도
호출하지 않는다.** `mCurrentUserCnt`는 영원히 0이라
`ProcessLogin`의 `GetCurrentUserCnt() >= GetMaxUserCnt()`는 항상 거짓이다.

실제 접속 제한은 `stClientInfo` 풀 크기(100)로 걸리므로 지금은 사고가 안 나지만,
로그인 유저 수와 연결 수는 다른 개념이다.

**제안** 카운터를 지우고 `mUserIDDictionary.size()`를 쓴다. 같은 상태를
두 벌 관리하지 않는 쪽이 안 어긋난다.

**결정**
**결과**

---

### R-009 — 핸들러가 `DOMAIN_STATE`를 검사하지 않음
| | |
|---|---|
| 등급 | 높음 |
| 위치 | `PacketManager` › 핸들러 전반 |
| 상태 | 제안됨 |
| 갱신 | 2026-08-20 |

**증상** 로그인하지 않은 클라이언트가 `ROOM_ENTER_REQUEST`를 보내면 빈 UserID로
룸에 입장한다. 로그인 없이 채팅·캐릭터 동기화도 가능하다.

**원인** `DOMAIN_STATE`는 정의되어 있고 전이도 일어나지만, 어떤 핸들러도
"이 패킷을 받을 수 있는 상태인가"를 확인하지 않는다.

**제안** R-005의 등록 테이블에 **요구 상태**를 같이 넣는다.
크기 검증과 상태 검증을 디스패처 한 곳에서 처리하면 핸들러 본문은
게임 로직만 남는다.

| 패킷 | 요구 상태 |
|---|---|
| `LOGIN_REQUEST` | `NONE` |
| `ROOM_ENTER_REQUEST` | `LOGIN` |
| `ROOM_LEAVE_REQUEST`, `ROOM_CHAT_REQUEST`, `CHARACTER_SYNC` | `ROOM` |

**결정**
**결과**

---

### R-010 — 세션 인덱스에 세대값 없음 → 패킷 오배달
| | |
|---|---|
| 등급 | 높음 |
| 위치 | `stClientInfo` (풀 재사용) + `PacketManager::EnqueuePacketData` / `DequePacketData` |
| 상태 | 제안됨 |
| 갱신 | 2026-08-20 |

**증상** 드물게 A 유저가 보낸 패킷이 B 유저 것으로 처리된다. 재현이 매우 어렵다.

**원인** 큐에는 **세션 인덱스만** 들어간다. 인덱스 5번이 끊기고 슬롯이
재사용되면, 큐에 남아있던 "5번 처리해줘"가 새 클라이언트에게 적용된다.
`RE_USE_SESSION_WAIT_TIMESEC`(3초) 지연은 이 창을 좁힐 뿐 닫지 못한다.
`User::Clear()`가 `PacketBuffer`를 비워도, 이미 큐에 들어간 인덱스는 남아있다.

**제안** `stClientInfo`에 `uint32 mGeneration`을 두고 재사용할 때마다 증가시킨다.
큐 원소를 `{index, generation}`으로 만들고, 처리 직전에 현재 세대와 비교해
다르면 버린다. 작은 수정이고, 이 부류 버그를 통째로 없앤다.

**결정**
**결과**

---

### R-011 — 매 수신마다 바이너리를 문자열로 로깅
| | |
|---|---|
| 등급 | 보통 |
| 위치 | `IOCPServer::WorkerThread` (RECV 분기) |
| 상태 | 제안됨 |
| 갱신 | 2026-08-20 |

**증상** 로그가 깨진 문자로 도배되고, 20Hz 동기화가 시작되면 처리량이 급락한다.

**원인**
```cpp
spdlog::info("[RECEIVED] bytes : {} , msg : {}\n", dwIoSize, pClientInfo->RecvBuffer().get());
```
`char*`를 fmt에 넘기면 **NUL 종단 문자열로 취급**한다. 수신 버퍼는 바이너리이고
생성 시 한 번만 0으로 채워질 뿐 매 수신마다 초기화되지 않는다. 256바이트 안에
0이 없으면 할당 범위 밖을 읽는다.

성능 문제도 같이 있다. 4인 룸 × 20Hz면 초당 수백 줄, 전부 동기 I/O다.

**제안** 이 줄은 삭제한다. 디버깅이 필요하면 `SPDLOG_TRACE` + 16진 덤프로
바꾸고 기본 레벨을 `warn`으로 올린다. 상시로 나가야 할 정보는 로그가 아니라
**메트릭**(초당 패킷 수, 큐 길이, 처리 시간)이다.

같은 이유로 `ClientInfo.cpp`의 `PostAccept`/`AcceptCompletion`/`SendIO`/
`SendCompleted` 로그도 정리 대상이다. 특히 `SendIO`의 `getsockopt(SO_TYPE)`
호출은 디버깅 잔재로 보이며 매 송신마다 시스템 콜을 낸다.

**결정**
**결과**

---

### R-012 — 헤더 크기가 암묵적, `static_assert` 없음
| | |
|---|---|
| 등급 | 보통 |
| 위치 | `PacketHeader.h` › `PACKET_HEADER` |
| 상태 | 제안됨 |
| 갱신 | 2026-08-20 |

**증상** 지금은 정상. 하지만 컴파일러/설정/필드 추가로 레이아웃이 바뀌면
클라이언트에서 값이 밀려 나오는 형태로 며칠 뒤에 발견된다.

**원인** `PACKET_HEADER`에만 `#pragma pack`이 없어 `sizeof`가 6이다(선언은 5바이트).
파생 패킷들은 `pack(1)`이라 **헤더는 패딩, 바디는 무패딩**인 하이브리드다.
자체 엔진 클라이언트가 이 6바이트를 암묵적으로 맞춰야 한다.

**제안** 레이아웃을 바꿀 필요는 없다. 다만 **못 박는다.**
```cpp
static_assert(sizeof(PACKET_HEADER) == 6);
static_assert(sizeof(CHARACTER_SYNC_PACKET) == 42);
// 패킷마다 한 줄
```
그리고 `Type` 뒤에 `UINT8 Reserved`를 명시해 6바이트가 의도임을 코드로 남긴다.
자세한 배경은 [protocol.md](protocol.md).

**결정**
**결과**

---

### R-013 — `Room::m_RoomLock`이 아무것도 지키지 않음
| | |
|---|---|
| 등급 | 보통 |
| 위치 | `Room.cpp` › `Room` 전반 |
| 상태 | 제안됨 |
| 갱신 | 2026-08-20 |

**증상** 동작 문제는 없다. 비용과 오해를 만든다.

**원인** 게임 로직은 `PacketManager::ProcessThread` **하나**에서만 돈다.
룸을 만지는 스레드가 하나뿐이라 경합 자체가 존재하지 않는다.
게다가 `EnterUser`는 락을 놓은 뒤에 브로드캐스트하므로, 락이 있어도
"입장과 통지의 원자성"을 지켜주지도 못한다.

읽는 사람에게 "여기는 여러 스레드가 들어온다"는 잘못된 신호를 준다.

**제안** 지금 당장은 지워도 된다. 다만 룸별 병렬 처리(룸 액터 모델)로 갈
계획이라면, 그때도 답은 락이 아니라 **소유권**이다 — 한 룸은 한 워커만 잡고,
룸 내부는 단일 스레드를 보장한다. 어느 경로든 이 mutex는 사라진다.
구조 전환 시점에 함께 정리하는 것을 권한다.

**결정**
**결과**

---

### R-014 — `AcceptCompletion`이 초기화 안 된 주소를 로깅
| | |
|---|---|
| 등급 | 낮음 |
| 위치 | `stClientInfo::AcceptCompletion` |
| 상태 | 제안됨 |
| 갱신 | 2026-08-20 |

**증상** 접속 로그의 클라이언트 IP가 매번 무의미한 값이다.

**원인**
```cpp
SOCKADDR_IN stClientAddr;              // 초기화 없음
int nAddrLen = sizeof(SOCKADDR_IN);    // 쓰이지 않음
inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
```
스택 쓰레기를 읽는다.

**제안** `AcceptEx`는 주소를 `mAcceptBuf`에 담아준다. `GetAcceptExSockaddrs`로
꺼내거나, 간단히 `getpeername(mSocket, ...)`을 쓴다. IP가 필요 없다면
로그 줄과 변수를 통째로 지운다.

**결정**
**결과**

---

### R-015 — 룸 설정 하드코딩
| | |
|---|---|
| 등급 | 낮음 |
| 위치 | `PacketManager::CreateComponent` |
| 상태 | 제안됨 |
| 갱신 | 2026-08-20 |

**증상** 룸 수나 정원을 바꾸려면 재빌드해야 한다.
코드에 `// TODO : 하드코딩 제거` 주석이 이미 있다.

**제안** 지금 당장 설정 파일 시스템을 만들 필요는 없다. 최소한
`Define.h`의 다른 상수들 옆으로 옮겨 한곳에 모으는 것부터 한다.
운영 중 변경이 실제로 필요해지면 그때 파일로 뺀다.

**결정**
**결과**

---

## 2026-08-20 (추가) — 불변식 문서화 중 발견

`CLAUDE.md`에 불변식 I-1/I-2를 명문화하는 과정에서, 코드가 그 불변식을
이미 위반하고 있는 것을 발견했다. **문서화 자체가 검출 도구로 작동한 사례다.**

### R-016 — `PacketBuffer`가 두 스레드에 무보호 공유
| | |
|---|---|
| 등급 | 치명 |
| 위치 | `PacketBuffer` 전체 / `PacketManager::ReceivePacketData` ↔ `PacketManager::ProcessPacket` |
| 상태 | 제안됨 |
| 갱신 | 2026-08-20 |

**증상** 부하 상태(다수 접속 + 20Hz 캐릭터 동기화)에서 패킷이 간헐적으로
깨져 읽힌다. `PacketLength`가 쓰레기값으로 읽히면 그 세션의 스트림 파싱이
영구히 어긋난다. 저부하에서는 거의 재현되지 않는다.

**원인** `User::mPacketBuffer`를 **두 스레드가 동기화 없이** 만진다.

| | 스레드 | 하는 일 |
|---|---|---|
| 쓰기 | **WorkerThread** | `OnReceive` → `ReceivePacketData` → `User::SetPacketData` → `mPacketDataBufferWPos` 갱신 |
| 읽기 | **ProcessThread** | `ProcessPacket` → `User::GetPacket` → `WPos`/`RPos` 읽고 `RPos` 갱신 |

`PacketBuffer`에는 mutex가 없다. 특히 위험한 것은 `SetPacketData`의
**compaction 경로**다.

```cpp
// SetPacketData — WorkerThread에서 실행
CopyMemory(mPacketDataBuffer.get(), GetPtr(mPacketDataBufferRPos), remainDataSize);
mPacketDataBufferWPos = remainDataSize;
mPacketDataBufferRPos = 0;              // ← ProcessThread가 쓰는 값을 덮어씀
```

버퍼가 찰 때 미소비 데이터를 앞으로 **이동시키면서 `RPos`를 0으로 되돌린다.**
바로 그 순간 ProcessThread가 `GetPacket` 안에서 옛 `RPos` 기준으로
`CopyMemory` 중이면 이동 중인 메모리를 읽는다. 나아가 `RPos` 갱신이 서로를
덮어써서 같은 패킷을 두 번 처리하거나 통째로 건너뛴다.

`User::Clear()`(ProcessThread, `ProcessUserConnect` 경로)가 `PacketBuffer::Clear()`를
부르는 것도 같은 레이스다.

**왜 지금까지 안 터졌나** 단일 클라이언트 저부하에서는 WorkerThread의 쓰기와
ProcessThread의 읽기가 시간적으로 겹칠 확률이 낮고, 64KB 버퍼라 compaction이
거의 발생하지 않는다. **fl-client로 다인 테스트를 시작하면 나타날 부류다.**

**뿌리** 이건 단독 실수가 아니라 D-005(큐에 인덱스만 저장)의 대가다.
큐 원소를 작게 만든 대신 버퍼를 스레드 경계 위에 올려놓았다.

**제안** — 셋 중 하나. 아래로 갈수록 크고 근본적이다.

1. **최소** `PacketBuffer`에 mutex를 넣고 `SetPacketData`/`GetPacket`/`Clear`를
   모두 감싼다. 가장 게으르고 즉시 안전해진다. 세션당 락이라 경합도 낮다.
2. **중간** 조립을 **수신 측 전용**으로 옮긴다. `stClientInfo`가 자기
   `PacketBuffer`를 갖고 WorkerThread에서 완성된 패킷 단위로 잘라 큐에 넣는다.
   그러면 버퍼는 WorkerThread 전용이 되고 큐만 공유된다 — **I-2가 구조적으로
   성립한다.** 대신 큐 원소가 커진다(D-005 뒤집기).
3. **큰** 룸 액터 모델 전환 시 세션→룸 잡큐로 흡수.

**권장은 2번.** 1번은 증상을 막지만 "게임 상태는 ProcessThread만 만진다"는
불변식은 여전히 깨진 채로 남는다. 2번은 불변식을 코드 구조로 만들어서,
같은 부류의 다음 버그를 애초에 불가능하게 한다. 다만 R-001/R-002(송신 경로
재작성)와 동시에 건드리면 한 번에 너무 많은 게 움직이니 **순서를 나누는 것**을
권한다 — 송신 먼저, 수신 나중.

**검증 방법** `PacketBuffer`는 소켓과 무관한 순수 로직이다.
"1바이트씩 쪼개 넣기", "패킷 3개를 한 번에 넣기", "compaction이 일어나도록
경계 근처까지 채우기" 세 가지 시나리오로 단위 테스트를 붙일 수 있다.
**이 저장소에서 테스트를 처음 붙이기 가장 좋은 지점**이기도 하다.

**결정**
**결과**

---

## 2026-08-23 — 계획 수립 중 발견

Phase 0의 `PacketBuffer` 테스트 케이스를 설계하다가 발견.

### R-017 — `PacketLength=0` 패킷 하나로 서버 전체 정지
| | |
|---|---|
| 등급 | 치명 (원격 DoS) |
| 위치 | `PacketBuffer::GetPacket` + `PacketManager::ProcessPacket` |
| 상태 | 제안됨 |
| 갱신 | 2026-08-23 |

**증상** 악의적인(또는 버그 있는) 클라이언트가 **6바이트짜리 패킷 하나**를 보내면
ProcessThread가 무한 루프에 빠진다. 게임 로직 스레드가 하나뿐이므로
**서버 전체가 응답을 멈춘다.** 접속은 유지되고 CPU 코어 하나가 100%가 된다.

**재현** `PacketLength = 0`, `PacketId = 1001`(SYS_END 초과면 아무거나)로 헤더만 전송.

**원인** 두 곳의 누락이 맞물린다.

`PacketBuffer::GetPacket`은 `PacketLength`의 **하한을 검사하지 않는다.**
```cpp
if (pHeader->PacketLength > remainByte)   // 상한만 본다
    return PacketInfo();
...
mPacketDataBufferRPos += pHeader->PacketLength;   // 0을 더한다 → RPos가 안 움직인다
```
`PacketLength`가 0이면 읽기 위치가 전진하지 않은 채 유효해 보이는
`PacketInfo`(`PacketId=1001`)가 반환된다.

`PacketManager::ProcessPacket`의 배출 루프가 이걸 무한 반복한다.
```cpp
while (true)
{
    auto nextPacket = pUser->GetPacket();
    if (nextPacket.PacketId <= PACKET_ID::SYS_END)
        break;                                    // 1001이라 절대 못 빠져나감
    ProcessRecvPacket(...);                       // 0바이트 버퍼를 계속 캐스팅
}
```
덤으로 매 반복마다 0바이트 할당을 `CHARACTER_SYNC_PACKET*`로 캐스팅하므로
R-005(크기 미검증)와 겹쳐 힙 범위 밖 읽기도 일어난다.

**왜 지금까지 안 터졌나** 정상 클라이언트는 이런 패킷을 만들지 않는다.
이건 **신뢰 경계 문제**이지 정상 경로 버그가 아니다. 그래서 fl-client 테스트로는
영원히 안 나오고, 실제 서비스에서 처음 발견된다.

**제안** 두 층에서 각각 막는다. 어느 한쪽만으로도 정지는 막히지만 둘 다 해야 한다.

1. **`PacketBuffer::GetPacket`** — `PacketLength < PACKET_HEADER_LENGTH`이면
   프레이밍이 이미 복구 불가능하다. 버퍼를 `Clear()`하고 `warn`을 남긴 뒤
   빈 결과를 돌려준다. 전진하지 않고 그냥 빈 결과만 주면 그 세션은 영원히
   막히므로, **반드시 버려야 한다.**
2. **`ProcessPacket`의 배출 루프** — `PacketId`가 아니라 "유효한 패킷을
   받았는가"로 종료를 판정한다. `DataSize == 0`이면 루프를 벗어난다.

**더 옳은 처리** 프레이밍이 깨진 것은 프로토콜 위반이므로 **연결을 끊는** 것이
정답이다. 다만 `PacketBuffer`에서 세션 종료까지 배선하는 일이 붙으므로,
Phase 0에서는 정지를 막는 데까지만 하고 절단은 Phase 2의 디스패처 게이트에서
함께 처리한다. → [limits.md](limits.md)

**검증 방법** `PacketBuffer` 단위 테스트로 잡힌다. 소켓이 필요 없다.
이 항목이 Phase 0에서 테스트를 먼저 붙이는 이유다.

**결정**
**결과**

---

## 2026-08-23 (추가) — 구성도 작성 중 발견

[structure.md](structure.md)를 그리면서 나온 것들. **구조를 도면으로 옮기는 작업이
검출 도구로 작동한 두 번째 사례다** (첫 번째는 R-016).

### R-018 — `SendPacketFunc` 시그니처가 계층마다 다르다
| | |
|---|---|
| 등급 | 낮음 |
| 위치 | `PacketManager::SendPacketFunc` / `RoomManager::SendPacketFunc` / `Room::SendPacketFunc` |
| 상태 | 제안됨 |
| 갱신 | 2026-08-23 |

**증상** 현재 없다. 모든 패킷이 65,536바이트 미만이라 드러나지 않는다.

**원인** 같은 람다가 세 번 복사되며 내려가는데 두 번째 인자 타입이 중간에서만 다르다.

| 소유자 | 시그니처 |
|---|---|
| `PacketManager::SendPacketFunc` | `function<void(UINT32, UINT32, shared_ptr<char[]>)>` |
| `RoomManager::SendPacketFunc` | `function<void(UINT32, **UINT16**, shared_ptr<char[]>)>` ← |
| `Room::SendPacketFunc` | `function<void(UINT32, UINT32, shared_ptr<char[]>)>` |

`RoomManager::Init`에서 `mRoomList[i]->SendPacketFunc = SendPacketFunc;`로 대입할 때
`UINT16` → `UINT32` 암묵 변환이 일어난다. 반대 방향(`PacketManager` → `RoomManager`)에서는
`UINT32` → `UINT16` 축소가 일어난다.

**왜 지금 문제가 아닌가** 최대 패킷이 `ROOM_CHAT_NOTIFY`(296바이트)다. 65,536을 넘는
패킷이 생기기 전까지는 절대 드러나지 않는다.

**왜 그래도 기록하는가** 의도로 보이지 않는다. 그리고 이런 것은 나중에 가변 길이
패킷(예: 룸 유저 목록 일괄 전송)을 추가하는 순간 조용히 잘려서 나간다.
**드러나는 시점이 원인에서 몇 달 떨어져 있는 부류다.**

**제안** 셋 다 `UINT32`로 통일한다. 근본적으로는 R-004의 제안과 같은 방향 —
크기를 인자로 넘기는 것 자체를 없애면 이 불일치가 존재할 자리가 사라진다.

Phase 3(송신 경로 재작성)에서 `SendPacketTo<TPacket>` 템플릿을 도입할 때 함께 해소된다.
**단독으로 지금 고칠 필요는 없다.**

**결정**
**결과**

---

### R-019 — `.gitignore`가 다른 저장소에서 복사됨
| | |
|---|---|
| 등급 | 낮음 |
| 위치 | `.gitignore` |
| 상태 | 제안됨 |
| 갱신 | 2026-08-23 |

**증상** 동작 문제는 없다. 읽는 사람을 혼란시킨다.

**원인** 파일 상단에 이 저장소에 존재하지 않는 경로 규칙이 있다.

```
/Resources/**
/Client/Bin/**
/Editor/Bin/**
/Engine/Bin/**
/EngineSDK/Include/**
Resources.zip
```

`Client` / `Editor` / `Engine` / `EngineSDK`는 **fl-client(DirectX11 자체 엔진) 저장소의
폴더 구조**다. 거기서 복사해 온 것으로 보인다.

빌드 산출물(`Binary/`, `Libraries/`, `x64/`)은 파일 뒷부분의 표준 Visual Studio 규칙이
덮어주므로 **실제 추적에는 문제가 없다.** 추적 파일 57개를 확인했고 산출물은
하나도 포함되지 않았다.

**제안** 이 저장소에 없는 경로 규칙을 지운다. 대신 실제로 필요한 것을 명시적으로 추가한다.

```gitignore
# fl-server
/Binary/
/Libraries/
/backup/          # 리팩터링 이전 구조의 로컬 사본. 저장소에 넣지 않는다
/.vs/
```

`backup/`은 현재 추적되지 않지만 **명시적으로 제외되어 있지도 않다.** 표준 규칙에
우연히 걸리는 게 아니라 의도적으로 제외한다고 적어두는 편이 낫다.

계획 Task 0.2 Step 17에서 `Tests/` 산출물 제외를 확인할 때 함께 정리하면 된다.

**결정**
**결과**
