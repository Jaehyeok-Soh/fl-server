# 정확성 기반 다지기 (Phase 0–2) 실행 계획

**목표:** fl-client로 믿을 수 있는 테스트를 할 수 있는 상태를 만든다 — 테스트 안전망을 깔고, 스트림을 깨뜨리는 결함을 없애고, 신뢰 경계에 게이트를 세운다.

**접근:** 순수 로직(`PacketBuffer`)에 먼저 단위 테스트를 붙여 안전망을 만든 뒤, 확정적 버그를 정리하고, 마지막에 패킷 디스패처 한 곳에 크기·상태 검증을 모은다. 구조 변경(송신 재작성, 룸 액터)은 이 계획에 **포함하지 않는다**.

**기술 스택:** C++20 / MSVC x64 / Visual Studio 솔루션 / vcpkg (spdlog, Catch2) / Windows IOCP

**근거 문서:**
- [docs/wiki/review-log.md](../../wiki/review-log.md) — R-001 ~ R-017
- [docs/wiki/decisions.md](../../wiki/decisions.md) — D-001 ~ D-009
- [docs/wiki/limits.md](../../wiki/limits.md)
- [AGENTS.md](../../../AGENTS.md) — 불변식 I-1 ~ I-8, 코딩 관례

---

## ⛔ 실행 방식 — 코드는 사람이 쓴다

**이 계획의 모든 코드는 사람(Jaehyeok)이 직접 작성한다.**

계획서에 코드 블록이 들어있다고 해서 LLM이 그것을 파일에 써넣어도 된다는 뜻이 **아니다.** 코드 블록은 **사람이 보고 타이핑할 참고자료**다. LLM이 이 계획을 "실행"하는 일은 없다. (→ [AGENTS.md](../../../AGENTS.md), D-009)

이 계획은 작업지시서가 아니라 **너의 작업 목록 + 나의 검토 게이트**다.

각 Task 끝에 **[검토 게이트]**가 있다. 커밋하고 알려주면 내가 읽고 의도대로 됐는지, 놓친 호출자나 부작용이 없는지 확인한다. 통과하면 review-log의 해당 항목을 `해결`로 넘긴다. 동의 안 되는 지적은 반려해라 — `decisions.md`의 X-번호로 남긴다.

### 스텝 읽는 법

- **한 스텝 = 한 동작.** 2~5분. 여러 파일을 동시에 고치는 스텝은 없다.
- **⏸ 중단 조건**이 붙은 스텝은 결과가 예상과 다르면 **멈추고 알려라.**
- **커밋은 Task 끝에서 한 번.** Task 중간에 빌드가 깨지는 구간이 있는 Task는 머리말에 표시해뒀다.

---

## 전역 제약

- **코딩 관례를 따른다** — 멤버 `m` 접두, 매개변수 뒤 `_`, 구조체 `st` 접두, 패킷 구조체 `SCREAMING_SNAKE_PACKET`. 신규 코드는 `m_`이 아니라 `m`.
- **와이어 포맷을 바꾸지 않는다.** 이 계획의 어떤 Task도 fl-client 수정을 요구하지 않는다. (D-006, I-7)
- **커밋은 Task 단위.** 한 커밋에 두 Task를 섞지 않는다.
- **빌드는 x64 Debug / Release 둘 다** 통과해야 한다.
- 각 Task는 **독립적으로 `git revert` 가능**해야 한다.

---

## 순서를 이렇게 잡은 이유

이전 대화에서 내가 "송신 경로 먼저"라고 했는데 **바꿨다.**

송신 재작성(R-001/R-002)은 중간 규모 작업이고, 그 결과를 검증할 수단이 지금 없다. 반면 R-004/R-003/R-017은 **각각 몇 줄짜리 확정적 버그**이고, 고치는 즉시 fl-client 테스트가 의미를 갖는다. 검증 수단을 먼저 만들고, 싼 것부터 확정적으로 없애고, 그 다음에 구조를 건드린다.

또 하나 — R-017은 **fl-client로는 영원히 안 잡힌다.** 정상 클라이언트가 안 만드는 패킷이기 때문이다. 이런 부류는 단위 테스트로만 잡히고, 그래서 Phase 0이 테스트부터 시작한다.

| Phase | Task | 내용 | 다루는 항목 |
|---|---|---|---|
| **0** | 0.1 ~ 0.9 | 테스트 안전망 + 프레이밍 방어 + 크기 고정 | R-017, R-012 |
| **1** | 1.1 ~ 1.9 | 스트림을 깨뜨리는 확정적 버그 | R-004, R-003, R-007, R-011, R-014, R-006, R-008 |
| **2** | 2.1 ~ 2.4 | 신뢰 경계 게이트 + 마감 | R-005, R-009 |
| 이후 | — | 송신 재작성 / 수신 소유권 / 룸 액터 | R-001, R-002, R-016, R-010, R-013 — **별도 계획** |

---

## 파일 구조

**신규**

| 파일 | 책임 | Task |
|---|---|---|
| `Tests/Tests.vcxproj` | 테스트 실행 파일. `ServerCore`를 링크 | 0.2 |
| `Tests/pch.h`, `Tests/pch.cpp` | 테스트용 pch | 0.2 |
| `Tests/Test_Smoke.cpp` | 골격 확인용. 0.2에서 만들고 0.3에서 삭제 | 0.2 |
| `Tests/Test_PacketBuffer.cpp` | `PacketBuffer` 프레이밍 전부 | 0.3 |
| `GameServer/PacketDispatchRule.h` | 디스패처 판정 규칙 (의존성 없는 헤더) | 2.1 |
| `Tests/Test_PacketDispatch.cpp` | 판정 규칙 테스트 | 2.1 |

**수정**

| 파일 | 무엇을 | Task |
|---|---|---|
| `ServerCore/PacketBuffer.cpp` | 프레이밍 하한 검증 | 0.5 |
| `ServerCore/PacketHeader.h` | `Reserved` + `static_assert` | 0.7 |
| `ServerCore/Sys_ConnectResponsePacket.h`, `GameServer/Packet_*.h` | `static_assert` | 0.8 |
| `GameServer/PacketManager.cpp` | 배출 루프 / 로그인 문자열 / 등록 테이블 | 0.6, 1.4, 1.9, 2.2 |
| `GameServer/Room.cpp` | `LeaveUser` 크기, `NotifyChat` 복사 | 1.1, 1.5 |
| `GameServer/User.h/.cpp` | `SetLogin` 시그니처, `GetUserId` 반환형 | 1.4, 1.5 |
| `GameServer/UserManager.h/.cpp` | `AddUser` 시그니처, 유저 수 | 1.4, 1.9 |
| `ServerCore/IOCPServer.cpp` | 수신 로그 제거 | 1.6 |
| `ServerCore/ClientInfo.cpp` | `getsockopt`·IP 로그 제거, 레벨 | 1.6, 1.7 |
| `GameServer/GameServer.cpp` | 로그 레벨 설정 | 1.7 |

`PacketManager.cpp`는 336줄로 가장 큰 파일이지만 **이 계획에서는 쪼개지 않는다** — Phase 2에서 등록 테이블이 들어가면 핸들러 본문이 짧아지므로, 분할 판단은 그 이후가 정확하다.

---

# Phase 0 — 테스트 안전망

## Task 0.1: Catch2 설치 — ✅ 완료 (2026-08-23)

**목적:** 테스트 프레임워크 확보.
**커밋:** 없음 (환경 설정)
**결과:** Catch2 3.15.0 설치됨. 설치 결과를 보고 **Task 0.2 Step 5/11을 정정했다** —
`Catch2Main.lib`은 `manual-link/`에 있어 자동 링크되지 않고 구성별 파일명도 다르다.
자체 `main`을 쓰는 쪽으로 바꿨다.

---

- [ ] **Step 1: vcpkg 통합 확인**

```
vcpkg integrate install
```
Expected: `Applied user-wide integration...` 또는 이미 적용됨

- [ ] **Step 2: Catch2 설치**

```
vcpkg install catch2:x64-windows
```
소요: 몇 분

- [x] **Step 3: 설치 확인** — 완료 2026-08-23

```
vcpkg list catch2
```
결과: `catch2:x64-windows    3.15.0` ✅ (3.x이므로 include 경로 그대로)

- [x] **Step 4: 헤더 존재 확인** — 완료 2026-08-23

`C:/vcpkg/installed/x64-windows/include/catch2/catch_test_macros.hpp` 존재 ✅

- [x] **Step 5 (추가): 라이브러리 배치 확인** — 완료 2026-08-23

계획에 없던 확인인데, 결과가 Task 0.2를 바꿔서 기록한다.

```
installed/x64-windows/lib/Catch2.lib                     ← 자동 링크 ✅
installed/x64-windows/lib/manual-link/Catch2Main.lib     ← 자동 링크 ❌
installed/x64-windows/debug/lib/Catch2d.lib              ← 이름이 다름
installed/x64-windows/debug/lib/manual-link/Catch2Maind.lib
```

→ Task 0.2 Step 5/11 정정.

---

## Task 0.2: Tests 프로젝트 골격

**목적:** 테스트 하나가 실제로 실행되는 것까지 확인. **테스트 내용은 아직 없다.**
**커밋:** 있음 · **되돌리기:** 안전 (프로젝트 파일만)
**⚠ 중간에 빌드가 깨진다.** Step 13까지는 커밋하지 않는다.

**Files:** Create `Tests/Tests.vcxproj`, `Tests/pch.h`, `Tests/pch.cpp`, `Tests/Test_Smoke.cpp` · Modify `fl-server.sln`

---

- [ ] **Step 1: 프로젝트 생성**

솔루션 우클릭 → 추가 → 새 프로젝트 → **콘솔 앱 (C++)** / 이름 `Tests` / 위치 `D:\Source\fl-server`

- [ ] **Step 2: 플랫폼 정리**

구성 관리자에서 `Tests`가 x64로 빌드되는지 확인. Win32 구성은 무시하거나 제거.

- [ ] **Step 3: 언어 표준**

속성 → **모든 구성 / x64** → C/C++ → 언어 → C++ 언어 표준 → `ISO C++20 (/std:c++20)`

`ServerCore`와 같아야 한다. 다르면 링크에서 이상한 오류가 난다.

- [ ] **Step 4: 포함 디렉터리**

속성 → C/C++ → 일반 → 추가 포함 디렉터리:
```
$(SolutionDir)ServerCore
```

- [ ] **Step 5: 링커 설정 — 아무것도 하지 않는다**

**추가 종속성을 건드리지 마라.** vcpkg 통합이 `Catch2.lib`(Release) / `Catch2d.lib`(Debug)를
구성에 맞게 자동으로 링크한다.

**`Catch2Main.lib`을 추가 종속성에 적으면 안 된다.** 두 가지 이유다.

1. `manual-link/`는 라이브러리 검색 경로에 **포함되지 않는다.** 그게 이 폴더의 존재 이유다
   — 자동 링크에서 빼려고 분리해 둔 것이다. 이름만 적으면 못 찾는다.
2. Debug 구성의 파일명은 `Catch2Maind.lib`다. 하드코딩하면 **Debug 빌드가 깨진다.**

그래서 Catch2가 제공하는 `main` 대신 **직접 `main`을 쓴다** (Step 11).
3줄이면 되고 프로젝트 설정을 하나도 안 건드려도 된다.

- [ ] **Step 6: ServerCore 참조 추가**

`Tests` → 참조 → 참조 추가 → 프로젝트 → `ServerCore` 체크

- [ ] **Step 7: `Tests/pch.h` 작성**

```cpp
#pragma once

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <cstring>
#include <string>
```

- [ ] **Step 8: `Tests/pch.cpp` 작성**

```cpp
#include "pch.h"
```

- [ ] **Step 9: pch 옵션 (프로젝트 전체)**

속성 → 모든 구성 → C/C++ → 미리 컴파일된 헤더
- 미리 컴파일된 헤더: **사용 (/Yu)**
- 헤더 파일: `pch.h`

- [ ] **Step 10: pch 옵션 (`pch.cpp`만)**

`pch.cpp` 우클릭 → 속성 → C/C++ → 미리 컴파일된 헤더 → **만들기 (/Yc)**

- [ ] **Step 11: 자동 생성 `Tests.cpp`를 테스트 러너로 교체**

Visual Studio가 만든 `Tests.cpp`의 내용을 지우고 아래로 바꾼다. **파일은 남긴다.**

```cpp
#include "pch.h"

#include <catch2/catch_session.hpp>

int main(int argc, char* argv[])
{
	return Catch::Session().run(argc, argv);
}
```

> Step 5에서 설명한 대로 `Catch2Main.lib`은 자동 링크되지 않는다. 이 3줄이 그 라이브러리가
> 하는 일 전부다. **가장 게으른 해법이 가장 견고하기도 하다** — 구성별 파일명
> (`Catch2d` / `Catch2Maind`)에 영향받지 않고, 나중에 명령줄 인자를 다루고 싶어지면
> 여기서 바로 손댈 수 있다.

- [ ] **Step 12: `Tests/Test_Smoke.cpp` 작성**

```cpp
#include "pch.h"

TEST_CASE("테스트 골격이 동작한다", "[smoke]")
{
	REQUIRE(1 + 1 == 2);
}
```

- [ ] **Step 13: 빌드 (Debug x64)**

⏸ **중단 조건:** 링크 오류면 멈춰라. 원인별 증상:

| 증상 | 원인 |
|---|---|
| `main` 중복 정의 | Step 11을 안 했다 |
| `Catch2Main.lib` 못 찾음 | 추가 종속성에 적었다 → **지워라** (Step 5) |
| `unresolved external __std_*` 계열 | **툴셋 불일치.** 아래 참조 |
| pch 관련 오류 | Step 9/10 설정 불일치 |

**툴셋 불일치에 관하여** — 이 PC에는 MSVC가 셋 설치되어 있다 (2026-08-23 확인).

| 위치 | 버전 | 용도 |
|---|---|---|
| VS 2022 Community | 14.29 (v142), 14.43 (**v143**) | 이 솔루션이 쓰는 것 |
| VS 18 BuildTools | 14.50 | **vcpkg가 Catch2를 빌드할 때 쓴 것** |

MSVC는 **오래된 라이브러리를 새 프로젝트에서** 쓰는 방향은 보장하지만 반대는 아니다.
14.50으로 빌드된 `Catch2.lib`이 v143(14.43)에 없는 런타임 심볼을 참조할 수 있다.
`spdlog`/`fmt`는 더 이전에 빌드되어 이 문제가 없다 — **Catch2만 해당된다.**

**이 오류가 나면** 둘 중 하나:
1. VS 2022 개발자 명령 프롬프트에서 Catch2 재빌드
   (`vcpkg remove catch2:x64-windows` 후 재설치)
2. 솔루션 툴셋을 BuildTools 쪽으로 변경 — **비추천.** 다른 프로젝트에 영향이 간다

**나면 알려줘. 안 날 수도 있으니 미리 손대지 마라.**

- [ ] **Step 14: 실행**

Run: `Tests.exe`
Expected:
```
All tests passed (1 assertion in 1 test case)
```

- [ ] **Step 15: 출력 경로 확인**

`Tests.exe`가 어디 생겼는지 확인해 적어둬라. 이후 계속 쓴다.
다른 프로젝트가 `Binary\`로 나가므로 통일하고 싶으면 지금 맞춰라.

- [ ] **Step 16: Release 빌드 확인**

Expected: 성공

- [ ] **Step 17: `.gitignore` 확인**

`Tests/x64/`, `*.vcxproj.user` 등 빌드 산출물이 제외되는지 확인. 안 되어 있으면 추가.

- [ ] **Step 18: 커밋**

```bash
git add Tests/ fl-server.sln .gitignore
git commit -m "build: Catch2 기반 Tests 프로젝트 추가

ServerCore를 링크하는 테스트 실행 파일. 아직 스모크 테스트만 있다."
```

**[검토 게이트]** 확인할 것: Release 빌드 결과, `.gitignore` 커버리지, 출력 경로 일관성.

---

## Task 0.3: `PacketBuffer` 정상 경로 테스트

**목적:** 지금 **잘 동작하는 것**을 못 박는다. 이후 수신 경로 변경의 안전망.
**커밋:** 있음 · **되돌리기:** 안전 (테스트만)

**Files:** Create `Tests/Test_PacketBuffer.cpp` · Delete `Tests/Test_Smoke.cpp`

**Interfaces:**
- Consumes: `PacketBuffer::Init/SetPacketData/GetPacket/Clear`, `PACKET_HEADER`, `PacketInfo`, `PACKET_DATA_BUFFER_SIZE`, `PACKET_HEADER_LENGTH`
- Produces: `MakeRaw(length_, id_, alloc_)`, `Slice(src_, offset_, size_)` — Task 0.4, 0.5, 0.7이 그대로 쓴다

---

- [ ] **Step 1: 파일 생성 + 헬퍼만 먼저**

`Tests/Test_PacketBuffer.cpp`:
```cpp
#include "pch.h"

#include "PacketBuffer.h"
#include "PacketInfo.h"
#include "PacketHeader.h"
#include "Define.h"

namespace
{
	// 헤더만 채운 원시 패킷 바이트를 만든다.
	// length_ : 헤더의 PacketLength 필드에 넣을 값 (일부러 틀린 값을 넣을 수 있게 분리)
	// alloc_  : 실제로 할당할 바이트 수
	std::shared_ptr<char[]> MakeRaw(UINT16 length_, UINT16 id_, UINT32 alloc_)
	{
		auto buf = std::make_shared<char[]>(alloc_);
		std::memset(buf.get(), 0, alloc_);

		PACKET_HEADER header = {};
		header.PacketLength = length_;
		header.PacketId = id_;
		std::memcpy(buf.get(), &header, sizeof(PACKET_HEADER));

		return buf;
	}

	// 원시 버퍼의 일부만 잘라낸다. TCP 분할 도착을 흉내낸다.
	std::shared_ptr<char[]> Slice(const std::shared_ptr<char[]>& src_, UINT32 offset_, UINT32 size_)
	{
		auto buf = std::make_shared<char[]>(size_);
		std::memcpy(buf.get(), src_.get() + offset_, size_);
		return buf;
	}
}
```

- [ ] **Step 2: 빌드만 해서 헤더 포함 확인**

테스트는 아직 없다. 컴파일이 되는지만 본다.

⏸ **중단 조건:** `PacketBuffer.h`나 `Define.h`를 못 찾으면 Step 4의 포함 디렉터리 문제다. 이 헤더들이 `ServerCore/pch.h`에 암묵 의존해 컴파일이 깨질 수도 있다 — 그 경우 멈추고 알려줘. 헤더 자립성 문제라 별도 판단이 필요하다.

- [ ] **Step 3: 테스트 1 — 완전한 패킷 하나**

```cpp
TEST_CASE("완전한 패킷 하나를 그대로 돌려준다", "[PacketBuffer]")
{
	PacketBuffer pb;
	pb.Init(PACKET_DATA_BUFFER_SIZE);

	const UINT16 len = 20;
	pb.SetPacketData(len, MakeRaw(len, 1001, len));

	auto packet = pb.GetPacket();
	REQUIRE(packet.PacketId == 1001);
	REQUIRE(packet.DataSize == len);
}
```

- [ ] **Step 4: 실행**

Run: `Tests.exe "[PacketBuffer]"` → Expected: PASS

⏸ **중단 조건:** 실패하면 멈춰라. 정상 경로다. 실패한다면 아직 못 찾은 결함이 있다.

- [ ] **Step 5: 테스트 2 — 소비 후 빈 결과**

```cpp
TEST_CASE("소비한 뒤에는 빈 결과가 나온다", "[PacketBuffer]")
{
	PacketBuffer pb;
	pb.Init(PACKET_DATA_BUFFER_SIZE);

	const UINT16 len = 20;
	pb.SetPacketData(len, MakeRaw(len, 1001, len));

	REQUIRE(pb.GetPacket().PacketId == 1001);
	REQUIRE(pb.GetPacket().PacketId == 0);
	REQUIRE(pb.GetPacket().PacketId == 0);
}
```

- [ ] **Step 6: 실행** → Expected: 2 cases PASS

- [ ] **Step 7: 테스트 3 — Clear**

```cpp
TEST_CASE("Clear 후에는 남아있던 데이터가 나오지 않는다", "[PacketBuffer]")
{
	PacketBuffer pb;
	pb.Init(PACKET_DATA_BUFFER_SIZE);

	const UINT16 len = 20;
	pb.SetPacketData(len, MakeRaw(len, 1001, len));
	pb.Clear();

	REQUIRE(pb.GetPacket().PacketId == 0);
}
```

- [ ] **Step 8: 실행** → Expected: 3 cases PASS

- [ ] **Step 9: 스모크 테스트 제거**

`Tests/Test_Smoke.cpp`를 프로젝트에서 제외하고 삭제. 역할이 끝났다.

- [ ] **Step 10: 전체 실행**

Run: `Tests.exe` → Expected: 3 cases, 전부 PASS

- [ ] **Step 11: 커밋**

```bash
git add Tests/
git commit -m "test: PacketBuffer 정상 경로 특성 테스트

현재 동작을 고정한다. 이후 수신 경로 변경의 안전망."
```

**[검토 게이트]** 확인할 것: 테스트가 진짜 `ServerCore` 코드를 부르는지, `MakeRaw`가 만드는 바이트가 실제 와이어 형식과 같은지.

---

## Task 0.4: `PacketBuffer` 경계 테스트

**목적:** TCP 분할·병합·compaction을 고정한다. **실서비스에서 항상 일어나지만 로컬 테스트에서는 거의 안 밟히는** 경로다.
**커밋:** 있음 · **되돌리기:** 안전

**Files:** Modify `Tests/Test_PacketBuffer.cpp`

---

- [ ] **Step 1: 테스트 4 — 두 조각으로 분할 도착**

```cpp
TEST_CASE("쪼개져 도착한 패킷은 다 모일 때까지 나오지 않는다", "[PacketBuffer]")
{
	PacketBuffer pb;
	pb.Init(PACKET_DATA_BUFFER_SIZE);

	const UINT16 len = 20;
	auto raw = MakeRaw(len, 1001, len);

	pb.SetPacketData(8, Slice(raw, 0, 8));
	REQUIRE(pb.GetPacket().PacketId == 0);

	pb.SetPacketData(len - 8, Slice(raw, 8, len - 8));

	auto packet = pb.GetPacket();
	REQUIRE(packet.PacketId == 1001);
	REQUIRE(packet.DataSize == len);
}
```

- [ ] **Step 2: 실행** → Expected: PASS

- [ ] **Step 3: 테스트 5 — 헤더보다 적게 도착**

```cpp
TEST_CASE("헤더보다 적게 도착하면 아무것도 나오지 않는다", "[PacketBuffer]")
{
	PacketBuffer pb;
	pb.Init(PACKET_DATA_BUFFER_SIZE);

	auto raw = MakeRaw(20, 1001, 20);
	pb.SetPacketData(3, Slice(raw, 0, 3));

	REQUIRE(pb.GetPacket().PacketId == 0);
}
```

- [ ] **Step 4: 실행** → Expected: PASS

- [ ] **Step 5: 테스트 6 — 1바이트씩 도착**

가장 가혹한 분할. 실제로 일어난다.

```cpp
TEST_CASE("1바이트씩 도착해도 마지막 바이트에서 정확히 완성된다", "[PacketBuffer]")
{
	PacketBuffer pb;
	pb.Init(PACKET_DATA_BUFFER_SIZE);

	const UINT16 len = 20;
	auto raw = MakeRaw(len, 1001, len);

	for (UINT32 i = 0; i < len - 1u; ++i)
	{
		pb.SetPacketData(1, Slice(raw, i, 1));
		REQUIRE(pb.GetPacket().PacketId == 0);
	}

	pb.SetPacketData(1, Slice(raw, len - 1u, 1));
	REQUIRE(pb.GetPacket().PacketId == 1001);
}
```

- [ ] **Step 6: 실행** → Expected: PASS

- [ ] **Step 7: 테스트 7 — 한 번에 여러 개 도착**

```cpp
TEST_CASE("한 번에 여러 패킷이 도착하면 순서대로 다 나온다", "[PacketBuffer]")
{
	PacketBuffer pb;
	pb.Init(PACKET_DATA_BUFFER_SIZE);

	const UINT16 len = 12;
	auto merged = std::make_shared<char[]>(len * 3);

	for (UINT16 i = 0; i < 3; ++i)
	{
		auto one = MakeRaw(len, static_cast<UINT16>(1001 + i), len);
		std::memcpy(merged.get() + (len * i), one.get(), len);
	}

	pb.SetPacketData(len * 3, merged);

	REQUIRE(pb.GetPacket().PacketId == 1001);
	REQUIRE(pb.GetPacket().PacketId == 1002);
	REQUIRE(pb.GetPacket().PacketId == 1003);
	REQUIRE(pb.GetPacket().PacketId == 0);
}
```

- [ ] **Step 8: 실행** → Expected: PASS

- [ ] **Step 9: 테스트 8 — 병합 + 분할 동시 발생**

```cpp
TEST_CASE("완전한 패킷 뒤에 잘린 패킷이 붙어 와도 앞의 것만 나온다", "[PacketBuffer]")
{
	PacketBuffer pb;
	pb.Init(PACKET_DATA_BUFFER_SIZE);

	const UINT16 len = 12;
	auto first = MakeRaw(len, 1001, len);
	auto second = MakeRaw(len, 1002, len);

	// 첫 패킷 전체 + 두 번째 패킷의 앞 5바이트
	auto chunk = std::make_shared<char[]>(len + 5);
	std::memcpy(chunk.get(), first.get(), len);
	std::memcpy(chunk.get() + len, second.get(), 5);

	pb.SetPacketData(len + 5, chunk);

	REQUIRE(pb.GetPacket().PacketId == 1001);
	REQUIRE(pb.GetPacket().PacketId == 0);   // 두 번째는 아직 미완성

	pb.SetPacketData(len - 5, Slice(second, 5, len - 5));
	REQUIRE(pb.GetPacket().PacketId == 1002);
}
```

- [ ] **Step 10: 실행** → Expected: PASS

⏸ **중단 조건:** 실패하면 멈춰라. compaction 직전 상태와 얽힌 결함일 수 있다.

- [ ] **Step 11: 테스트 9 — compaction 경로**

```cpp
TEST_CASE("버퍼 끝에 닿아도 계속 정상 동작한다 (compaction)", "[PacketBuffer]")
{
	PacketBuffer pb;
	pb.Init(PACKET_DATA_BUFFER_SIZE);

	const UINT16 len = 256;   // MAX_SOCKBUF와 같은 크기
	const UINT32 rounds = (PACKET_DATA_BUFFER_SIZE / len) + 10;   // 경계를 확실히 넘긴다

	for (UINT32 i = 0; i < rounds; ++i)
	{
		pb.SetPacketData(len, MakeRaw(len, 1001, len));

		auto packet = pb.GetPacket();
		REQUIRE(packet.PacketId == 1001);
		REQUIRE(packet.DataSize == len);
	}

	REQUIRE(pb.GetPacket().PacketId == 0);
}
```

- [ ] **Step 12: 실행** → Expected: PASS

- [ ] **Step 13: compaction이 진짜 밟혔는지 눈으로 확인**

`PacketBuffer::SetPacketData`의 compaction 분기
(`if ((mPacketDataBufferWPos + dataSize_) >= PACKET_DATA_BUFFER_SIZE)`)에
중단점을 걸고 Step 11 테스트만 실행해서 **실제로 멈추는지** 확인.

> 통과했다고 그 경로를 밟은 건 아니다. 테스트가 의도한 코드를 실행하는지 한 번은 봐야 한다.

⏸ **중단 조건:** 중단점에 안 걸리면 이 테스트는 compaction을 테스트하지 않고 있다. 알려줘.

- [ ] **Step 14: 전체 실행** → Expected: 9 cases PASS

- [ ] **Step 15: 커밋**

```bash
git add Tests/Test_PacketBuffer.cpp
git commit -m "test: PacketBuffer 분할/병합/compaction 경계 테스트

1바이트 단위 분할, 다중 패킷 병합, 버퍼 끝 compaction 경로를 고정한다."
```

**[검토 게이트]** 확인할 것: Step 13의 실측 결과, 빠진 경계 케이스.

---

## Task 0.5: R-017 — 프레이밍 하한 검증

**목적:** `PacketLength=0` 패킷 하나로 서버 전체가 멈추는 것을 막는다. **원격 DoS 차단.**
**커밋:** 있음 · **되돌리기:** 취약한 동작으로 회귀

**Files:** Modify `ServerCore/PacketBuffer.cpp` (`GetPacket`), `Tests/Test_PacketBuffer.cpp`

---

- [ ] **Step 1: 실패하는 테스트 — `PacketLength = 0`**

```cpp
TEST_CASE("PacketLength가 0이면 유효한 패킷으로 취급하지 않는다", "[PacketBuffer][R-017]")
{
	PacketBuffer pb;
	pb.Init(PACKET_DATA_BUFFER_SIZE);

	pb.SetPacketData(PACKET_HEADER_LENGTH, MakeRaw(0, 1001, PACKET_HEADER_LENGTH));

	REQUIRE(pb.GetPacket().PacketId == 0);
}
```

- [ ] **Step 2: 실행해서 실패 확인**

Run: `Tests.exe "[R-017]"`
Expected: **FAIL** — `PacketId == 1001`을 돌려주며 깨진다.

⏸ **중단 조건:** 실패하지 않으면 재현이 안 된 것이다. 멈추고 알려줘.

- [ ] **Step 3: 무한 반복도 고정하는 테스트**

실제로 서버가 멈추는 이유는 "읽기 위치가 전진하지 않아서"다.

```cpp
TEST_CASE("PacketLength가 0이면 버퍼를 버려서 무한 반복을 막는다", "[PacketBuffer][R-017]")
{
	PacketBuffer pb;
	pb.Init(PACKET_DATA_BUFFER_SIZE);

	pb.SetPacketData(PACKET_HEADER_LENGTH, MakeRaw(0, 1001, PACKET_HEADER_LENGTH));

	// 전진하지 않고 빈 결과만 주면 그 세션은 영원히 막힌다. 반드시 버려야 한다.
	REQUIRE(pb.GetPacket().PacketId == 0);
	REQUIRE(pb.GetPacket().PacketId == 0);
	REQUIRE(pb.GetPacket().PacketId == 0);
}
```

- [ ] **Step 4: 실행해서 실패 확인** → Expected: FAIL

- [ ] **Step 5: 1~5도 막는 테스트**

```cpp
TEST_CASE("PacketLength가 헤더 크기보다 작으면 전부 거부한다", "[PacketBuffer][R-017]")
{
	for (UINT16 bad = 1; bad < PACKET_HEADER_LENGTH; ++bad)
	{
		PacketBuffer pb;
		pb.Init(PACKET_DATA_BUFFER_SIZE);

		pb.SetPacketData(PACKET_HEADER_LENGTH, MakeRaw(bad, 1001, PACKET_HEADER_LENGTH));

		REQUIRE(pb.GetPacket().PacketId == 0);
		REQUIRE(pb.GetPacket().PacketId == 0);
	}
}
```

- [ ] **Step 6: 실행** → Expected: 3개 모두 FAIL

- [ ] **Step 7: `GetPacket`에 하한 검증 추가**

`ServerCore/PacketBuffer.cpp`, `auto pHeader = (PACKET_HEADER*)GetPtr(mPacketDataBufferRPos);` **바로 다음**:

```cpp
	// 프레이밍이 깨진 경우. PacketLength가 0이면 읽기 위치가 전진하지 않아
	// 상위 배출 루프가 무한 반복한다. 유효해 보이는 패킷을 만들어선 안 된다.
	// 스트림 신뢰가 이미 끝났으므로 통째로 버린다.
	if (pHeader->PacketLength < PACKET_HEADER_LENGTH)
	{
		spdlog::warn("[PacketBuffer] invalid PacketLength({}), PacketId({}). discarding buffer",
			pHeader->PacketLength, pHeader->PacketId);
		Clear();
		return PacketInfo();
	}
```

- [ ] **Step 8: 실행** → Expected: `[R-017]` 3개 모두 PASS

- [ ] **Step 9: 기존 테스트 회귀 확인**

Run: `Tests.exe`
Expected: 12 cases 전부 PASS

⏸ **중단 조건:** 기존 테스트가 깨지면 하한 검증이 정상 패킷까지 막고 있다. 멈춰라.

- [ ] **Step 10: 죽은 코드 정리**

`GetPacket` 상단의 쓰이지 않는 변수 3개를 지운다:
```cpp
	const int PACKET_SIZE_LENGTH = { 2 };
	const int PACKET_TYPE_LENGTH = { 2 };
	short packetSize = { 0 };
```

- [ ] **Step 11: 전체 재실행** → Expected: 전부 PASS

- [ ] **Step 12: 커밋**

```bash
git add ServerCore/PacketBuffer.cpp Tests/Test_PacketBuffer.cpp
git commit -m "fix: PacketLength 하한 미검증으로 인한 무한 루프 차단 (R-017)

PacketLength=0이면 읽기 위치가 전진하지 않아 상위 배출 루프가 무한 반복하고
서버 전체가 멈춘다. 프레이밍이 깨진 시점에 버퍼를 폐기한다."
```

**[검토 게이트]** 확인할 것: `Clear()`가 맞는 선택인지, `GetPacket`의 다른 호출자, `spdlog::warn` 로그 폭주 가능성.

---

## Task 0.6: R-017 — 배출 루프 두 번째 방어선

**목적:** 루프 종료를 `PacketId` 값으로 판정하는 것 자체가 위태롭다. 구조적으로 막는다.
**커밋:** 있음 · **되돌리기:** 안전

**Files:** Modify `GameServer/PacketManager.cpp` (`ProcessPacket`)

---

- [ ] **Step 1: 현재 종료 조건 확인**

```cpp
			while (true)
			{
				auto nextPacket = pUser->GetPacket();
				if (nextPacket.PacketId <= PACKET_ID::SYS_END)
					break;
				ProcessRecvPacket(...);
			}
```
빈 결과의 `PacketId`가 0이라 **우연히** 빠져나가고 있다. 의도가 아니다.

- [ ] **Step 2: 종료 조건 추가**

```cpp
			auto pUser = mUserManager->GetUserByConnIdx(packetData.ClientIndex);
			while (true)
			{
				auto nextPacket = pUser->GetPacket();

				// 유효한 패킷을 받았는가로 판정한다. PacketId 값에 의존하지 않는다.
				if (nextPacket.DataSize == 0)
					break;

				if (nextPacket.PacketId <= PACKET_ID::SYS_END)
					break;

				ProcessRecvPacket(packetData.ClientIndex, nextPacket.PacketId, nextPacket.DataSize, nextPacket.pDataPtr);
			}
```

- [ ] **Step 3: 빌드** → Expected: 성공

- [ ] **Step 4: 정상 흐름 확인**

fl-client 또는 DummyClient: 접속 → 로그인 → 룸 입장 → 채팅 → 종료
Expected: 이전과 동일

- [ ] **Step 5: 커밋**

```bash
git add GameServer/PacketManager.cpp
git commit -m "fix: 패킷 배출 루프 종료 조건을 DataSize 기준으로 변경 (R-017)

PacketId 값에 의존하던 종료 판정을 유효 패킷 여부로 바꿔 두 번째 방어선을 둔다."
```

**[검토 게이트]** 확인할 것: `DataSize == 0`인 유효 패킷이 존재할 수 있는지 (시스템 패킷 경로).

---

## Task 0.7: R-012 — `PACKET_HEADER` 크기 고정

**목적:** 헤더 레이아웃이 바뀌면 **빌드가 깨져서** 알려주게 한다.
**커밋:** 있음 · **되돌리기:** 안전

**Files:** Modify `ServerCore/PacketHeader.h`

---

- [ ] **Step 1: 현재 크기를 실측**

`Tests/Test_PacketBuffer.cpp`에 **임시로** 추가:
```cpp
TEST_CASE("TEMP: 헤더 크기 실측", "[temp]")
{
	REQUIRE(sizeof(PACKET_HEADER) == 999);   // 일부러 틀린 값
}
```
Run: `Tests.exe "[temp]"`
Expected: FAIL하면서 실제 값을 출력. **그 숫자를 적어둬라.**

⏸ **중단 조건:** 6이 아니면 멈추고 알려줘. 분석 전제가 틀린 것이고 protocol.md도 다시 써야 한다.

- [ ] **Step 2: 임시 테스트 제거**

- [ ] **Step 3: `Reserved` 필드 + 주석 추가**

`ServerCore/PacketHeader.h`:
```cpp
#pragma once
#include <windows.h>

// 주의: 이 구조체에는 의도적으로 #pragma pack을 적용하지 않는다.
// 선언된 필드는 5바이트지만 2바이트 정렬로 sizeof는 6이 되고,
// fl-client도 이 6바이트를 그대로 맞춰야 한다. 아래 Reserved는
// 그 패딩이 우연이 아니라 계약임을 코드로 남기기 위한 것이다.
// 파생 패킷 구조체들은 모두 #pragma pack(1)이다. (→ docs/wiki/protocol.md)
struct PACKET_HEADER
{
	UINT16 PacketLength = { 0 };   // 헤더 포함 전체 길이
	UINT16 PacketId = { 0 };
	UINT8 Type = { 0 };            // 미사용
	UINT8 Reserved = { 0 };        // 패딩을 명시화. 값은 항상 0
};

const UINT32 PACKET_HEADER_LENGTH = sizeof(PACKET_HEADER);
```

- [ ] **Step 4: 빌드 + 전체 테스트**

Run: `Tests.exe` → Expected: 전부 PASS

⏸ **중단 조건:** `Reserved` 추가로 테스트가 깨지면(= `sizeof`가 바뀌면) 멈춰라. 와이어 호환이 깨진 것이다.

- [ ] **Step 5: `static_assert` 추가**

`PacketHeader.h` 맨 아래:
```cpp
static_assert(sizeof(PACKET_HEADER) == 6, "wire format changed - fl-client도 함께 고쳐야 한다");
```

- [ ] **Step 6: 일부러 깨뜨려 확인**

`6`을 `7`로 바꾸고 빌드.
Expected: **컴파일 오류**, 메시지가 보인다.

이걸 안 해보면 `static_assert`가 실제로 걸리는지 모른다.

- [ ] **Step 7: 되돌린다**

`7` → `6`. 빌드 성공 확인.

- [ ] **Step 8: fl-client 통신 실증**

서버 실행 → 접속 → 로그인 → 채팅
Expected: 정상. **와이어가 안 바뀌었다는 증거다.**

⏸ **중단 조건:** 통신이 깨지면 즉시 되돌리고 알려줘.

- [ ] **Step 9: 커밋**

```bash
git add ServerCore/PacketHeader.h
git commit -m "chore: PACKET_HEADER 패딩을 Reserved로 명시하고 크기 고정 (R-012)

sizeof는 6으로 불변이라 와이어 호환에 영향 없음. 레이아웃이 바뀌면
클라이언트에서 값이 밀리는 대신 빌드가 깨진다."
```

**[검토 게이트]** 확인할 것: Step 8의 통신 확인 결과, `PACKET_HEADER_LENGTH` 사용처가 여전히 맞는지.

---

## Task 0.8: R-012 — 나머지 패킷 크기 고정

**목적:** 모든 와이어 구조체 크기를 컴파일 타임에 못 박는다.
**커밋:** 있음 · **되돌리기:** 안전

**Files:** Modify `ServerCore/Sys_ConnectResponsePacket.h`, `GameServer/Packet_Login.h`, `Packet_Room.h`, `Packet_RoomChat.h`, `Packet_CharacterSync.h`

> 각 스텝의 숫자는 **계산값**이다. 빌드가 깨지면 컴파일러가 실제 값을 알려준다. 그 값으로 고치고 Step 11에 기록해라.

---

- [ ] **Step 1: `Sys_ConnectResponsePacket.h`**

`#pragma pack(pop)` 바로 다음:
```cpp
static_assert(sizeof(SYS_CONNECT_RESPONSE_PACKET) == 10, "wire format changed");
```

- [ ] **Step 2: 빌드** → Expected: 성공

- [ ] **Step 3: `Packet_Login.h`**

```cpp
static_assert(sizeof(LOGIN_REQUEST_PACKET) == 72, "wire format changed");
static_assert(sizeof(LOGIN_RESPONSE_PACKET) == 8, "wire format changed");
```

- [ ] **Step 4: 빌드** → Expected: 성공

- [ ] **Step 5: `Packet_Room.h`**

```cpp
static_assert(sizeof(ROOM_ENTER_REQUEST_PACKET) == 10, "wire format changed");
static_assert(sizeof(ROOM_ENTER_RESPONSE_PACKET) == 8, "wire format changed");
static_assert(sizeof(ROOM_LEAVE_REQUEST_PACKET) == 6, "wire format changed");
static_assert(sizeof(ROOM_LEAVE_RESPONSE_PACKET) == 8, "wire format changed");
static_assert(sizeof(ROOM_JOIN_PACKET) == 43, "wire format changed");
static_assert(sizeof(ROOM_LEAVE_PACKET) == 10, "wire format changed");
```

- [ ] **Step 6: 빌드** → Expected: 성공

- [ ] **Step 7: `Packet_RoomChat.h`**

```cpp
static_assert(sizeof(ROOM_CHAT_REQUEST_PACKET) == 263, "wire format changed");
static_assert(sizeof(ROOM_CHAT_RESPONSE_PACKET) == 8, "wire format changed");
static_assert(sizeof(ROOM_CHAT_NOTIFY_PACKET) == 296, "wire format changed");
```

- [ ] **Step 8: 빌드** → Expected: 성공

- [ ] **Step 9: `Packet_CharacterSync.h`**

```cpp
static_assert(sizeof(CHARACTER_SYNC_PACKET) == 42, "wire format changed");
```

- [ ] **Step 10: 빌드 (Debug + Release)** → Expected: 둘 다 성공

- [ ] **Step 11: 불일치 목록 정리**

Step 2~10에서 실제 값이 달랐던 것을 적는다. 검토 게이트에서 같이 본다.

| 구조체 | 계산값 | 실제값 |
|---|---|---|
| | | |

- [ ] **Step 12: fl-client 통신 확인**

전체 흐름 한 번. Expected: 정상 (아무것도 안 바뀌었으므로 당연히 정상이어야 한다)

- [ ] **Step 13: 커밋**

```bash
git add ServerCore/Sys_ConnectResponsePacket.h GameServer/Packet_Login.h GameServer/Packet_Room.h GameServer/Packet_RoomChat.h GameServer/Packet_CharacterSync.h
git commit -m "chore: 전체 와이어 패킷 크기를 static_assert로 고정 (R-012)"
```

**[검토 게이트]** 확인할 것: Step 11의 불일치 목록. 하나라도 달랐다면 protocol.md와 fl-client 양쪽을 다시 봐야 한다.

---

## Task 0.9: 문서에 확정값 반영 + Phase 0 마감

**목적:** `protocol.md`의 크기 표를 "계산값"에서 "확인된 값"으로 승격한다.
**커밋:** 있음 · **되돌리기:** 문서만

**Files:** Modify `docs/wiki/protocol.md`, `AGENTS.md`

---

- [ ] **Step 1: `protocol.md` 단서 문장 수정**

찾을 문장:
> 아래 표의 크기는 위 규칙으로 **계산한 값**이다. 실제 컴파일 결과와 대조해 `static_assert`로 고정하는 것을 권한다.

바꿀 문장:
> 아래 표의 크기는 **컴파일로 확인되어 `static_assert`로 고정된 값**이다.
> 값이 바뀌면 빌드가 깨진다.

- [ ] **Step 2: 크기 표 값 대조**

Task 0.8 Step 11에서 다른 값이 나왔으면 표를 고친다.

- [ ] **Step 3: `PACKET_HEADER` 코드 블록에 `Reserved` 추가**

- [ ] **Step 4: 「패딩에 관한 주의」 절을 확정 서술로**

두 가지 선택지를 제시했던 부분을, **2번(현 상태 유지 + 명시화)을 택했다**고 확정 서술로 바꾼다.

- [ ] **Step 5: `AGENTS.md` 불변식 표 갱신**

I-7 행의 「현재 강제 수단」: `없음 → R-012` → `static_assert (PacketHeader.h 외 5개 파일)`

- [ ] **Step 6: 커밋**

```bash
git add docs/wiki/protocol.md AGENTS.md
git commit -m "docs: 와이어 크기를 확정값으로 승격, 불변식 I-7 강제 수단 갱신"
```

---

### ✅ Phase 0 완료 기준

- [ ] `Tests.exe` 12개 테스트 전부 통과
- [ ] Debug / Release 둘 다 빌드
- [ ] fl-client 접속 → 로그인 → 룸 입장 → 채팅 정상

> **`PacketLength=0`을 실제 소켓으로 보내는 실증은 Task 1.3 이후에 한다.**
> 현재 `DummyClient`는 `main`에서 `ServerCore()` 스텁만 호출하는 빈 껍데기라
> 악성 패킷을 보낼 수단이 없다. Phase 0의 R-017 방어는 단위 테스트로만
> 검증되며, 소켓 경로 실증은 Task 1.3 Step 13으로 미룬다.

**[Phase 게이트]** 여기서 내가 Phase 0 전체를 다시 읽는다.

---

# Phase 1 — 스트림을 깨뜨리는 확정적 버그

여기서부터는 실제 통신으로 검증된다. **각 Task마다 눈으로 확인하고 넘어가라.**

## Task 1.1: R-004 — `Room::LeaveUser` 크기 불일치

**목적:** 룸 퇴장 시 클라이언트 스트림 전체가 깨지는 것을 막는다. **이 계획에서 가장 눈에 띄는 증상이다.**
**커밋:** 있음 · **되돌리기:** 안전

**Files:** Modify `GameServer/Room.cpp` (`Room::LeaveUser`)

---

- [ ] **Step 1: 현재 코드를 읽고 문제를 눈으로 확인**

`GameServer/Room.cpp`의 `LeaveUser` 안:
```cpp
ROOM_LEAVE_PACKET stayUserPkt = {};
stayUserPkt.PacketLength = sizeof(ROOM_JOIN_PACKET);        // ← 43 (틀림)
SendPacketFunc(..., sizeof(ROOM_JOIN_PACKET), MakePacketBuffer(stayUserPkt));
//                  ↑ 43 (틀림)              ↑ 10바이트만 할당
```
`MakePacketBuffer`는 `sizeof(T)` = **10바이트**를 할당하는데 `SendMsg`는 **43바이트**를 `CopyMemory`한다.

- [ ] **Step 2: 증상을 실제로 재현**

fl-client 2개를 같은 룸에 넣고 하나를 퇴장시킨다.
Expected:
- 남은 클라이언트에서 `ROOM_LEAVE_NOTIFY`의 `PacketLength`가 **43**으로 온다
- 이후 그 클라이언트의 패킷 파싱이 어긋난다

**고치기 전에 증상을 본다. 못 봤으면 고쳤는지 알 수 없다.**

⏸ **중단 조건:** 재현이 안 되면 멈추고 알려줘. 클라이언트가 이미 방어하고 있거나 내 분석이 틀린 것이다.

- [ ] **Step 3: `PacketLength` 수정**

```cpp
			stayUserPkt.PacketLength = sizeof(ROOM_LEAVE_PACKET);
```

- [ ] **Step 4: 전송 크기 수정**

```cpp
			SendPacketFunc(pStayUser->GetNetConnIdx(), sizeof(ROOM_LEAVE_PACKET), MakePacketBuffer(stayUserPkt));
```

수정 후 블록 전체:
```cpp
		for (auto& pStayUser : mUserList)
		{
			ROOM_LEAVE_PACKET stayUserPkt = {};
			stayUserPkt.PacketId = PACKET_ID::ROOM_LEAVE_NOTIFY;
			stayUserPkt.PacketLength = sizeof(ROOM_LEAVE_PACKET);
			stayUserPkt.ClientIndex = leaveUser_->GetNetConnIdx();

			SendPacketFunc(pStayUser->GetNetConnIdx(), sizeof(ROOM_LEAVE_PACKET), MakePacketBuffer(stayUserPkt));
		}
```

- [ ] **Step 5: 빌드** → Expected: 성공

- [ ] **Step 6: Step 2와 같은 시나리오로 재확인**

Expected:
- `PacketLength`가 **10**으로 온다
- 이후 패킷 파싱이 정상

- [ ] **Step 7: 퇴장 후 다시 입장까지 확인**

퇴장 → 재입장 → 채팅. Expected: 정상

- [ ] **Step 8: 커밋**

```bash
git add GameServer/Room.cpp
git commit -m "fix: Room::LeaveUser가 ROOM_JOIN_PACKET 크기로 전송하던 문제 (R-004)"
```

**[검토 게이트]** 확인할 것: Step 2/6의 실제 관측값, `LeaveUser`의 다른 부분에 남은 문제.

---

## Task 1.2: R-004 부류 전수 점검

**목적:** 증상이 아니라 **부류**를 잡는다. 같은 실수가 다른 곳에도 있는지 확인한다.
**커밋:** 있음 (문제를 찾으면) / 없음 (문제가 없으면 검토 게이트만)

**Files:** 점검 대상 — `GameServer/PacketManager.cpp`, `GameServer/Room.cpp`

---

- [ ] **Step 1: `MakePacketBuffer` 호출을 전부 나열**

```bash
grep -n "MakePacketBuffer" GameServer/*.cpp
```

- [ ] **Step 2: 각 줄을 표로 대조**

각 호출에서 `sizeof(X)`의 `X`와 `MakePacketBuffer(y)`의 `y` **타입**이 같은지 확인한다.

| 파일 › 함수 | 크기 인자의 타입 | 버퍼 인자 | 일치? |
|---|---|---|---|
| `PacketManager::ProcessSysUserConnectResponse` | `SYS_CONNECT_RESPONSE_PACKET` | `connectResPacket` | |
| `PacketManager::ProcessLogin` (×3) | `LOGIN_RESPONSE_PACKET` | `loginResPacket` | |
| `PacketManager::ProcessEnterRoom` | `ROOM_ENTER_RESPONSE_PACKET` | `roomEnterResPacket` | |
| `PacketManager::ProcessLeaveRoom` | `ROOM_LEAVE_RESPONSE_PACKET` | `roomLeaveResPacket` | |
| `PacketManager::ProcessRoomChatMessage` (×2) | `ROOM_CHAT_RESPONSE_PACKET` | `roomChatResPacket` | |
| `Room::EnterUser` | `ROOM_JOIN_PACKET` | `oldUserPkt` | |
| `Room::LeaveUser` | `ROOM_LEAVE_PACKET` | `stayUserPkt` | ✅ Task 1.1에서 수정 |
| `Room::NotifyChat` | `roomChatNtfyPkt` (변수 sizeof) | `roomChatNtfyPkt` | |
| `Room::NotifyNewGuest` | `ROOM_JOIN_PACKET` | `newGuestPkt` | |

- [ ] **Step 3: `PacketLength` 필드도 같이 대조**

각 패킷을 만드는 곳에서 `PacketLength`에 넣은 값이 그 패킷의 `sizeof`와 같은지 확인한다.
Task 1.1의 버그는 **크기 인자와 `PacketLength` 둘 다** 틀렸었다.

- [ ] **Step 4: 발견한 것을 정리**

| 위치 | 무엇이 틀렸나 | 수정 |
|---|---|---|
| | | |

- [ ] **Step 5: `Room::CharacterSync`도 확인**

```cpp
void Room::CharacterSync(INT32 clientIndex_, shared_ptr<char[]> pData)
{
	SendToAllUser(sizeof(CHARACTER_SYNC_PACKET), pData, clientIndex_, false);
}
```
여기는 `pData`를 **밖에서 받는다.** 호출자(`PacketManager::ProcessCharacterSync`)가
`MakePacketBuffer(characterSyncBroadCastPacket)`로 만든 것이 맞는지 확인한다.
타입이 어긋나면 같은 부류의 버그다.

- [ ] **Step 6: 구조적 대안을 검토만 한다 (구현하지 않는다)**

`SendPacketFunc`가 크기를 **따로** 받는 것이 이 실수를 부른다. 템플릿 헬퍼로 묶으면
크기 인자가 사라져 이 부류가 구조적으로 불가능해진다:

```cpp
// 참고용 스케치 — 이 Task에서는 구현하지 않는다
template<typename TPacket>
void SendPacketTo(UINT32 clientIndex_, TPacket& packet_)
{
	packet_.PacketLength = sizeof(TPacket);
	SendPacketFunc(clientIndex_, sizeof(TPacket), MakePacketBuffer(packet_));
}
```

**지금 하지 않는 이유:** 호출부 12곳을 동시에 바꾸는 변경이라 Phase 1의
"확정적 버그만 잡는다" 범위를 넘는다. Phase 3(송신 재작성)에서 송신 경로를
어차피 건드리므로 그때 같이 하는 것이 총 변경량이 적다.

**동의하지 않으면 지금 하자고 말해라.** 판단은 네 몫이다.

- [ ] **Step 7: 커밋 (Step 4에서 수정한 게 있을 때만)**

```bash
git add GameServer/
git commit -m "fix: MakePacketBuffer 크기 인자 불일치 전수 점검 (R-004 부류)"
```

**[검토 게이트]** 확인할 것: Step 2/3/4의 대조 결과 표. 내가 같은 대조를 독립적으로 해서 맞춰본다.

---

## Task 1.3: DummyClient 구현

**목적:** 검증 도구를 만든다. **악성 패킷을 보낼 수단이 지금 전혀 없다.**
**커밋:** 있음 · **되돌리기:** 안전 (테스트 도구만)
**⚠ 중간에 빌드가 깨진다.** 마지막에 커밋.

**Files:** Modify `DummyClient/DummyClient.cpp`, `DummyClient/DummyClient.vcxproj` · Create `DummyClient/README.md`

**현재 상태:** `main()`이 `ServerCore()` 스텁만 호출하는 빈 껍데기다. **이름만 DummyClient다.**

**왜 지금인가:** Phase 2의 게이트 검증(잘못된 크기·상태·미등록 ID 전송)과 Phase 0의
R-017 실증이 전부 이 도구를 필요로 한다. Phase 2에 가서 만들면 그때 Phase 2가 두 배로 커진다.

**설계 방침:** IOCP를 쓰지 않는다. **블로킹 소켓 + 수신 스레드 1개**면 충분하다.
이건 테스트 도구지 서버가 아니다.

---

- [ ] **Step 1: GameServer 헤더 포함 경로 추가**

`DummyClient` 속성 → 모든 구성 → C/C++ → 추가 포함 디렉터리:
```
$(SolutionDir)ServerCore;$(SolutionDir)GameServer
```
패킷 구조체는 헤더 전용이라 링크는 필요 없다.

- [ ] **Step 2: 언어 표준 확인**

`ServerCore`와 같은 `/std:c++20`인지 확인.

- [ ] **Step 3: 접속 + 종료 골격만 먼저**

`DummyClient/DummyClient.cpp`:
```cpp
#include "pch.h"

#include <iostream>
#include <string>
#include <thread>

#include "Packet_GamesServer.h"
#include "Sys_ConnectResponsePacket.h"

#pragma comment(lib, "ws2_32")

namespace
{
	SOCKET gSock = INVALID_SOCKET;

	bool ConnectToServer(const char* ip_, UINT16 port_)
	{
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
			return false;

		gSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (gSock == INVALID_SOCKET)
			return false;

		SOCKADDR_IN addr = {};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port_);
		inet_pton(AF_INET, ip_, &addr.sin_addr);

		if (connect(gSock, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
		{
			std::cout << "connect failed: " << WSAGetLastError() << "\n";
			return false;
		}

		std::cout << "connected\n";
		return true;
	}
}

int main()
{
	if (ConnectToServer("127.0.0.1", SERVER_PORT) == false)
		return 1;

	std::cout << "enter 'quit' to exit\n";

	std::string line;
	while (std::getline(std::cin, line))
	{
		if (line == "quit")
			break;
	}

	closesocket(gSock);
	WSACleanup();
	return 0;
}
```

- [ ] **Step 4: 빌드 + 접속 확인**

서버를 띄우고 `DummyClient` 실행.
Expected: `connected` 출력. 서버 로그에 `AcceptCompletion` / `OnConnect`가 보인다.

⏸ **중단 조건:** 접속이 안 되면 멈춰라. 포트나 방화벽 문제일 수 있다.

- [ ] **Step 5: 수신 스레드 추가**

`namespace` 안에:
```cpp
	volatile bool gRunning = true;

	void RecvLoop()
	{
		char buf[8192] = {};
		UINT32 pos = 0;

		while (gRunning)
		{
			int received = recv(gSock, buf + pos, static_cast<int>(sizeof(buf) - pos), 0);
			if (received <= 0)
			{
				std::cout << "[recv] closed (" << received << ")\n";
				return;
			}

			pos += received;

			while (pos >= PACKET_HEADER_LENGTH)
			{
				auto pHeader = reinterpret_cast<PACKET_HEADER*>(buf);

				// 클라이언트도 서버와 같은 프레이밍 방어가 필요하다
				if (pHeader->PacketLength < PACKET_HEADER_LENGTH)
				{
					std::cout << "[recv] BROKEN FRAMING len=" << pHeader->PacketLength
						<< " id=" << pHeader->PacketId << "\n";
					pos = 0;
					break;
				}

				if (pos < pHeader->PacketLength)
					break;

				std::cout << "[recv] id=" << pHeader->PacketId
					<< " len=" << pHeader->PacketLength << "\n";

				memmove(buf, buf + pHeader->PacketLength, pos - pHeader->PacketLength);
				pos -= pHeader->PacketLength;
			}
		}
	}
```

`main`의 접속 성공 직후:
```cpp
	std::thread recvThread(RecvLoop);
```
`main` 끝, `closesocket` 뒤:
```cpp
	gRunning = false;
	if (recvThread.joinable())
		recvThread.join();
```

> 이 파서가 `PacketLength` 하한을 검사하는 것에 주목해라.
> **fl-client에도 같은 검사가 있는지 확인해봐라** — 없으면 review-log 항목이 하나 더 생긴다.

- [ ] **Step 6: 빌드 + 접속 시 수신 확인**

Expected: 접속하면 서버가 `SYS_USER_CONNECT_RESPONSE`를 보내므로
```
[recv] id=13 len=10
```

⏸ **중단 조건:** 이게 안 나오면 서버 송신 경로에 문제가 있다. 멈추고 알려줘.

- [ ] **Step 7: 패킷 전송 헬퍼 추가**

```cpp
	void SendRaw(const void* data_, int size_)
	{
		int sent = send(gSock, static_cast<const char*>(data_), size_, 0);
		std::cout << "[send] " << sent << " bytes\n";
	}

	template<typename TPacket>
	void SendPacket(TPacket& packet_, PACKET_ID::Enum id_)
	{
		packet_.PacketId = id_;
		packet_.PacketLength = sizeof(TPacket);
		SendRaw(&packet_, sizeof(TPacket));
	}
```

- [ ] **Step 8: 명령어 — login**

`main`의 while 루프 안:
```cpp
		if (line.rfind("login ", 0) == 0)
		{
			LOGIN_REQUEST_PACKET pkt = {};
			strncpy_s(pkt.UserID, sizeof(pkt.UserID), line.substr(6).c_str(), _TRUNCATE);
			strncpy_s(pkt.UserPW, sizeof(pkt.UserPW), "pw", _TRUNCATE);
			SendPacket(pkt, PACKET_ID::LOGIN_REQUEST);
			continue;
		}
```

- [ ] **Step 9: 확인 — login**

Run: `login testuser`
Expected: `[send] 72 bytes` → `[recv] id=202 len=8`

- [ ] **Step 10: 명령어 — enter / chat / leave**

```cpp
		if (line.rfind("enter ", 0) == 0)
		{
			ROOM_ENTER_REQUEST_PACKET pkt = {};
			pkt.RoomNumber = std::stoi(line.substr(6));
			SendPacket(pkt, PACKET_ID::ROOM_ENTER_REQUEST);
			continue;
		}

		if (line.rfind("chat ", 0) == 0)
		{
			ROOM_CHAT_REQUEST_PACKET pkt = {};
			strncpy_s(pkt.Message, sizeof(pkt.Message), line.substr(5).c_str(), _TRUNCATE);
			SendPacket(pkt, PACKET_ID::ROOM_CHAT_REQUEST);
			continue;
		}

		if (line == "leave")
		{
			ROOM_LEAVE_REQUEST_PACKET pkt = {};
			SendPacket(pkt, PACKET_ID::ROOM_LEAVE_REQUEST);
			continue;
		}
```

- [ ] **Step 11: 확인 — 정상 흐름 전체**

DummyClient 2개를 띄우고:
```
(A) login alice
(A) enter 0
(B) login bob
(B) enter 0
(A) chat hello
(B) leave
```
Expected: A가 `id=231`(JOIN), `id=223`(CHAT), `id=232`(LEAVE)를 순서대로 받는다.
**Task 1.1 수정 덕에 `id=232 len=10`으로 와야 한다.**

- [ ] **Step 12: 악성 패킷 명령 추가**

```cpp
		// raw <PacketLength> <PacketId> : 헤더만 보내되 길이 필드를 임의로 조작
		if (line.rfind("raw ", 0) == 0)
		{
			int len = 0;
			int id = 0;
			sscanf_s(line.c_str(), "raw %d %d", &len, &id);

			PACKET_HEADER header = {};
			header.PacketLength = static_cast<UINT16>(len);
			header.PacketId = static_cast<UINT16>(id);
			SendRaw(&header, sizeof(PACKET_HEADER));
			continue;
		}
```

- [ ] **Step 13: R-017이 실제 소켓 경로에서 막히는지 실증**

```
login attacker
raw 0 1001
```
Expected:
- 서버 로그에 `[PacketBuffer] invalid PacketLength(0)` warn
- **서버가 계속 응답한다.** 다른 DummyClient에서 `chat test`가 정상 동작

⏸ **중단 조건:** 서버가 멈추면 Task 0.5의 수정이 실제 경로에서 안 먹은 것이다. 멈추고 알려줘.

- [ ] **Step 14: 취약점이 실재했음을 확인 (선택)**

```bash
git stash
git checkout <Task 0.5 이전 커밋> -- ServerCore/PacketBuffer.cpp
```
빌드 후 `raw 0 1001` → **서버가 멈춘다.** CPU 코어 하나가 100%.
확인했으면 복구:
```bash
git checkout HEAD -- ServerCore/PacketBuffer.cpp
git stash pop
```

> 포트폴리오에서 설명할 때 "고쳤다"보다 "이렇게 터지는 걸 확인하고 고쳤다"가 훨씬 강하다.
> 부담되면 건너뛰어도 된다.

- [ ] **Step 15: 사용법 문서**

`DummyClient/README.md`:
```markdown
# DummyClient

서버 검증용 최소 클라이언트. 블로킹 소켓 + 수신 스레드 1개.

## 명령어

| 명령 | 동작 |
|---|---|
| `login <id>` | LOGIN_REQUEST 전송 |
| `enter <roomNo>` | ROOM_ENTER_REQUEST 전송 |
| `chat <msg>` | ROOM_CHAT_REQUEST 전송 |
| `leave` | ROOM_LEAVE_REQUEST 전송 |
| `raw <len> <id>` | 헤더만 전송. 길이 필드를 임의로 조작해 신뢰 경계를 시험한다 |
| `quit` | 종료 |

수신 패킷은 `[recv] id=<id> len=<len>` 형태로 출력된다.
```

- [ ] **Step 16: 커밋**

```bash
git add DummyClient/
git commit -m "test: DummyClient를 실제 검증 도구로 구현

블로킹 소켓 + 수신 스레드. login/enter/chat/leave 정상 흐름과,
헤더 길이를 임의 조작하는 raw 명령으로 신뢰 경계를 시험할 수 있다."
```

**[검토 게이트]** 확인할 것: Step 13의 실증 결과, 수신 파서가 서버와 같은 프레이밍 규칙을 쓰는지, fl-client에도 `PacketLength` 하한 검사가 있는지.

---

## Task 1.4: R-003 — 로그인 UserID 손상

**목적:** 유저 ID가 쓰레기 문자열이 되고 중복 로그인 차단이 죽어 있는 문제를 고친다.
**커밋:** 있음 · **되돌리기:** 안전
**⚠ Step 3~9 사이에는 빌드가 깨진다.** 시그니처를 바꾸고 호출부를 고치는 순서라 쪼갤 수 없다.

**Files:** Modify `GameServer/User.h`, `User.cpp`, `UserManager.h`, `UserManager.cpp`, `PacketManager.cpp`

**Interfaces:**
- Produces: `UserManager::AddUser(const string& userID_, int clientIndex_)`, `User::SetLogin(const string& userID_)` — Task 1.9가 `UserManager`를 다시 건드린다

---

- [ ] **Step 1: 증상 재현 — ID 손상**

DummyClient 2개를 같은 룸에 넣고 한쪽에서 `chat hi`.
Expected: `ROOM_CHAT_NOTIFY`의 `UserID`가 `t` 또는 쓰레기 문자열.

> 지금 DummyClient는 `id`/`len`만 출력한다. `UserID`를 보려면 수신 루프에서
> `id == 223`일 때 `ROOM_CHAT_NOTIFY_PACKET`으로 캐스팅해 `UserID`를 함께
> 출력하도록 고쳐라. (이건 계속 쓸 수단이니 남겨두는 걸 권한다)

- [ ] **Step 2: 증상 재현 — 중복 로그인이 뚫린다**

```
(A) login samename
(B) login samename
```
Expected: 둘 다 `Result = 0`(성공). **`LOGIN_USER_ALREADY(31)`이 안 나온다.**

⏸ **중단 조건:** B가 거부되면 멈추고 알려줘. 내 분석이 틀린 것이다.

- [ ] **Step 3: `User::SetLogin` 선언 변경**

`GameServer/User.h`:
```cpp
	int SetLogin(const string& userID_);
```

- [ ] **Step 4: `User::SetLogin` 정의 변경**

`GameServer/User.cpp`:
```cpp
int User::SetLogin(const string& userID_)
{
	mCurDomainState = DOMAIN_STATE::LOGIN;
	mUserID = userID_;

	return 0;
}
```

- [ ] **Step 5: `UserManager::AddUser` 선언 변경**

`GameServer/UserManager.h`:
```cpp
	ERROR_CODE::Enum AddUser(const string& userID_, int clientIndex_);
```

- [ ] **Step 6: `UserManager::AddUser` 정의 변경**

`GameServer/UserManager.cpp`:
```cpp
ERROR_CODE::Enum UserManager::AddUser(const string& userID_, int clientIndex_)
{
	mUserObjPool[clientIndex_]->SetLogin(userID_);
	mUserIDDictionary.insert({ userID_, clientIndex_ });

	return ERROR_CODE::NONE;
}
```
쓰이지 않던 `auto user_idx = clientIndex_;`는 지운다.

- [ ] **Step 7: `ProcessLogin`에서 안전하게 문자열 생성**

`GameServer/PacketManager.cpp`. 크기 검사 직후:
```cpp
	auto pLoginReqPacket = reinterpret_cast<LOGIN_REQUEST_PACKET*>(pPacket_.get());

	// 패킷의 char 배열이 NUL 종단이라고 믿지 않는다. 길이를 직접 제한해 문자열을 만든다.
	const string userId(pLoginReqPacket->UserID,
		strnlen(pLoginReqPacket->UserID, MAX_USER_ID_LEN));

	spdlog::info("requested user id = {}", userId);
```

- [ ] **Step 8: 빈 ID 거부 추가**

응답 패킷 초기화 **뒤**, 정원 체크 **앞**:
```cpp
	if (userId.empty())
	{
		loginResPacket.Result = ERROR_CODE::LOGIN_USER_INVALID_PW;
		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), MakePacketBuffer(loginResPacket));
		return;
	}
```

> `ERROR_CODE`에 "잘못된 ID"에 딱 맞는 값이 없어 `LOGIN_USER_INVALID_PW`를 쓴다.
> 새 코드를 추가하려면 클라이언트도 함께 고쳐야 하므로 「와이어 무변경」 제약에 걸린다.
> 판단은 네 몫이다.

- [ ] **Step 9: 나머지 호출부 교체**

```cpp
	if (mUserManager->FindUserIndexByID(userId.c_str()) == -1)
	{
		mUserManager->AddUser(userId, clientIndex_);
		...
	}
```
`make_shared<char>(*pLoginReqPacket->UserID)`가 남아있으면 안 된다.

- [ ] **Step 10: 빌드** → Expected: 성공

⏸ **중단 조건:** `AddUser`/`SetLogin`의 다른 호출자가 있어 컴파일이 깨지면 그 목록을 알려줘. 내가 놓친 곳이다.

- [ ] **Step 11: Step 1 재확인 — ID가 정확한가**

Expected: `UserID`가 `testuser` 그대로

- [ ] **Step 12: Step 2 재확인 — 중복 로그인이 막히는가**

Expected: B가 `Result = 31`로 거부

- [ ] **Step 13: 퇴장 후 재로그인 확인**

```
(A) login alice → 연결 끊기
(A') login alice
```
Expected: 재로그인 성공.

**여기가 이 Task에서 가장 놓치기 쉬운 지점이다.** `DeleteUserInfo`가
`mUserIDDictionary.erase(user_->GetUserId())`로 지우는데, ID가 제대로 들어가야
제대로 지워진다. 이전에는 쓰레기 키로 들어가서 **지워지지도 않았다.**

⏸ **중단 조건:** 재로그인이 `LOGIN_USER_ALREADY`로 거부되면 사전에서 안 지워진 것이다. 멈추고 알려줘.

- [ ] **Step 14: 32자 ID 확인**

```
login abcdefghijklmnopqrstuvwxyz012345
```
Expected: 정확히 32자로 저장·전달. 잘리거나 늘어나지 않는다.

- [ ] **Step 15: 커밋**

```bash
git add GameServer/User.h GameServer/User.cpp GameServer/UserManager.h GameServer/UserManager.cpp GameServer/PacketManager.cpp DummyClient/
git commit -m "fix: 로그인 시 UserID가 1바이트만 복사되던 문제 (R-003)

make_shared<char>(*UserID)가 첫 글자 하나만 담은 비종단 힙 블록을 만들어,
std::string 생성 시 범위 밖을 읽고 중복 로그인 판정도 항상 실패했다."
```

**[검토 게이트]** 확인할 것: Step 13의 재로그인 결과, `mUserIDDictionary` 누수, `FindUserIndexByID`가 `const char*`를 받는 게 여전히 적절한지.

---

## Task 1.5: R-007 — `NotifyChat` 고정 길이 복사

**목적:** 채팅 알림의 `UserID` 뒤에 쓰레기가 붙는 범위 밖 읽기를 없앤다.
**커밋:** 있음 · **되돌리기:** 안전

**Files:** Modify `GameServer/User.h` (`GetUserId`), `GameServer/Room.cpp` (`NotifyChat`)

---

- [ ] **Step 1: 증상 재현**

```
login ab
enter 0
chat hello
```
Expected: `ROOM_CHAT_NOTIFY`의 `UserID`가 `ab` 뒤에 쓰레기 바이트를 달고 온다.

> ID가 15자를 넘는지에 따라 증상이 달라진다 (MSVC `std::string` SSO 경계).
> **짧은 ID에서 더 잘 보인다.**

- [ ] **Step 2: `GetUserId`를 참조 반환으로**

`GameServer/User.h`:
```cpp
	const string& GetUserId() const { return mUserID; }
```
호출부는 `.c_str()`을 쓰고 있어 그대로 컴파일된다. 매 호출 복사가 사라지는 것은 덤이다.

- [ ] **Step 3: 빌드** → Expected: 성공

⏸ **중단 조건:** 컴파일이 깨지면 `GetUserId()` 반환값을 값으로 받아 저장하는 곳이 있는 것이다. 위치를 알려줘.

- [ ] **Step 4: `UserID` 복사 교체**

`GameServer/Room.cpp`:
```cpp
	strncpy_s(roomChatNtfyPkt.UserID, sizeof(roomChatNtfyPkt.UserID), userID_, _TRUNCATE);
```
바로 위 `NotifyNewGuest`가 이미 같은 방식을 쓴다. 그 패턴을 따른다.

- [ ] **Step 5: `Msg` 복사도 교체**

```cpp
	strncpy_s(roomChatNtfyPkt.Msg, sizeof(roomChatNtfyPkt.Msg), msg_, _TRUNCATE);
```
원본이 `char[257]`이라 지금은 안전하지만, 클라이언트가 NUL 없이 257바이트를 꽉 채워
보내면 알림 패킷의 `Msg`가 비종단이 되어 **클라이언트 쪽에서** 문제가 된다.

- [ ] **Step 6: 빌드 + 짧은 ID 확인**

Expected: `UserID`가 정확히 `ab` + NUL, 뒤는 전부 0

- [ ] **Step 7: 긴 ID 확인 (32자)** → Expected: 32자 그대로

- [ ] **Step 8: 긴 메시지 확인 (256자)**

Expected: 잘리더라도 NUL 종단이 보장되고 크래시가 없다

- [ ] **Step 9: 커밋**

```bash
git add GameServer/Room.cpp GameServer/User.h
git commit -m "fix: NotifyChat의 고정 길이 CopyMemory로 인한 범위 밖 읽기 (R-007)"
```

**[검토 게이트]** 확인할 것: `GetUserId()` 반환형 변경의 다른 영향, `NotifyNewGuest`와 스타일 일치.

---

## Task 1.6: R-011 — 범위 밖 읽기를 유발하는 로그 제거

**목적:** 로그 두 곳이 할당 범위 밖을 읽는다. **성능이 아니라 안전 문제라 먼저 처리한다.**
**커밋:** 있음 · **되돌리기:** 안전

**Files:** Modify `ServerCore/IOCPServer.cpp` (`WorkerThread`), `ServerCore/ClientInfo.cpp` (`SendIO`)

---

- [ ] **Step 1: 문제를 확인**

```cpp
spdlog::info("[RECEIVED] bytes : {} , msg : {}", dwIoSize, pClientInfo->RecvBuffer().get());
```
`char*`를 fmt에 넘기면 **NUL 종단 문자열로 취급**한다. 수신 버퍼는 바이너리고
생성 시 한 번만 0으로 채워질 뿐 매 수신마다 초기화되지 않는다.
256바이트 안에 0이 없으면 할당 범위 밖을 읽는다.

- [ ] **Step 2: 해당 로그 줄 삭제**

수정 후:
```cpp
		else if (IOOperation::RECV == pOverlappedEx->m_eOperation)
		{
			OnReceive(pClientInfo->GetIndex(), dwIoSize, pClientInfo->RecvBuffer());
			pClientInfo->BindRecv();
		}
```

- [ ] **Step 3: `SendIO`의 디버깅 잔재 삭제**

`ServerCore/ClientInfo.cpp`, `SendIO()`에서 다음 5줄을 지운다:
```cpp
	spdlog::info("SendIO socket handle: {}", (int)mSocket);

	int optVal = { 0 };
	int optLen = sizeof(optVal);
	int ret = getsockopt(mSocket, SOL_SOCKET, SO_TYPE, (char*)&optVal, &optLen);
	spdlog::info("getsockopt SO_TYPE ret: {} optVal: {} err: {}", ret, optVal, WSAGetLastError());
```
**매 송신마다 시스템 콜을 하나 더 하고 있었다.**

- [ ] **Step 4: 빌드** → Expected: 성공

- [ ] **Step 5: 정상 흐름 확인**

DummyClient 2개로 login → enter → chat.
Expected: 정상 동작. 콘솔에서 깨진 문자가 사라진다.

- [ ] **Step 6: 커밋**

```bash
git add ServerCore/IOCPServer.cpp ServerCore/ClientInfo.cpp
git commit -m "fix: 수신 버퍼를 문자열로 찍던 로그 제거 (R-011)

바이너리 char*를 fmt에 넘겨 버퍼 범위 밖을 읽던 로그를 삭제.
SendIO의 디버깅용 getsockopt 호출도 함께 제거."
```

**[검토 게이트]** 확인할 것: 진단에 꼭 필요한 정보를 지우지 않았는지.

---

## Task 1.7: R-011 / R-014 — 로그 레벨 정리

**목적:** 부하 테스트가 가능한 로그 레벨로 만들고, 초기화 안 된 값을 찍는 로그를 없앤다.
**커밋:** 있음 · **되돌리기:** 안전

**Files:** Modify `ServerCore/ClientInfo.cpp`, `GameServer/GameServer.cpp`

---

- [ ] **Step 1: R-014 — 초기화 안 된 주소 로깅 제거**

`ServerCore/ClientInfo.cpp`, `AcceptCompletion()`에서:
```cpp
	SOCKADDR_IN stClientAddr;              // 초기화 없음 → 스택 쓰레기
	int nAddrLen = sizeof(SOCKADDR_IN);    // 쓰이지 않음
	char clientIP[32] = { 0, };
	inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
	spdlog::info("Client Connect : IP({}) SOCKET({})", clientIP, (int)mSocket);
```
전부 지운다.

> IP가 실제로 필요하면 `getpeername(mSocket, ...)`으로 제대로 가져와라.
> 지금은 필요 없으므로 지운다. 필요하다고 판단하면 말해줘.

- [ ] **Step 2: 매 접속 시도마다 나오는 로그를 debug로**

`spdlog::info` → `spdlog::debug`:
- `PostAccept`의 `"PostAccept. client Index: {}"`
- `AcceptCompletion`의 `"AcceptCompletion : SessionIndex({})"`
- `BindIOCompletionPort`의 `"BindIOCP result: {} socket: {}"`

`AccepterThread`가 32ms마다 빈 슬롯 전부에 `PostAccept`를 시도하므로 **초당 수백 줄**이 나온다.

- [ ] **Step 3: 매 송신마다 나오는 로그를 debug로**

`SendCompleted`의 `"[Send Completed] bytes : {}"` → `spdlog::debug`

- [ ] **Step 4: 남길 로그를 정한다**

`info`로 남겨도 되는 것 — 드물게 일어나고 운영에 의미 있는 사건:
- `IOCPServer::Init` / `BindandListen` / `StartServer`의 시작 로그
- `GameServerService::OnConnect` / `OnClose`
- `PacketManager`의 로그인 로그

**판단 기준은 "초당 몇 번 나오는가"다.** 접속당 1회면 `info`, 패킷당 1회면 `debug` 이하.

- [ ] **Step 5: `main`에 로그 레벨 설정**

`GameServer/GameServer.cpp`:
```cpp
int main()
{
#ifdef _DEBUG
	spdlog::set_level(spdlog::level::debug);
#else
	spdlog::set_level(spdlog::level::info);
#endif

	GameServerService server;
	...
```

- [ ] **Step 6: Debug 빌드 확인**

Expected: 기존과 비슷한 양의 로그 (debug가 켜져 있으므로)

- [ ] **Step 7: Release 빌드 확인**

DummyClient 2개를 붙이고 채팅을 여러 번.
Expected: 콘솔이 도배되지 않는다. 접속/로그인/종료 정도만.

- [ ] **Step 8: 캐릭터 동기화 부하 확인**

fl-client로 캐릭터를 계속 움직여 `CHARACTER_SYNC`를 20Hz로 흘린다.
Expected: Release에서 로그가 거의 안 늘어난다.

⏸ **중단 조건:** 여전히 도배되면 놓친 상시 로그가 있다. 어느 것인지 알려줘.

- [ ] **Step 9: 커밋**

```bash
git add ServerCore/ClientInfo.cpp GameServer/GameServer.cpp
git commit -m "fix: 초기화 안 된 주소 로깅 제거 및 로그 레벨 정리 (R-014, R-011)"
```

**[검토 게이트]** 확인할 것: Step 8의 결과, 디버그 빌드에서 필요한 정보가 여전히 보이는지.

---

## Task 1.8: R-006 — `LOGIN_RESPONSE` 수신 등록 제거

**목적:** 클라이언트가 서버→클라 전용 패킷을 보내 로그인 경로를 타는 것을 막는다.
**커밋:** 있음 · **되돌리기:** 안전

**Files:** Modify `GameServer/PacketManager.cpp` (`Init`)

---

- [ ] **Step 1: 현재 상태 확인**

```cpp
	mRecvFunctionDictionary[PACKET_ID::LOGIN_REQUEST] = &PacketManager::ProcessLogin;
	mRecvFunctionDictionary[PACKET_ID::LOGIN_RESPONSE] = &PacketManager::ProcessLogin;  // ←
```

- [ ] **Step 2: 실제로 뚫리는지 시험**

DummyClient에서 `raw 8 202`로 `LOGIN_RESPONSE` 크기·ID의 패킷을 보낸다.
Expected: `ProcessLogin`의 크기 검사(72)에 걸려 **조용히 무시된다.**

> **현재는 우연히 막혀 있다.** 크기 검사가 없었다면 뚫렸다.
> 우연에 의존하는 안전은 안전이 아니다. 그래서 지운다.

- [ ] **Step 3: 등록 줄 삭제**

- [ ] **Step 4: 빌드** → Expected: 성공

- [ ] **Step 5: 정상 로그인이 여전히 되는지 확인**

Run: `login testuser` → Expected: `[recv] id=202 len=8`, `Result = 0`

- [ ] **Step 6: 응답/통지 계열 ID 전수 확인**

`Init()`에 등록된 ID 중 서버→클라 전용(`*_RESPONSE`, `*_NOTIFY`)이 더 없는지 확인한다.

- [ ] **Step 7: 커밋**

```bash
git add GameServer/PacketManager.cpp
git commit -m "fix: LOGIN_RESPONSE가 수신 핸들러에 등록되어 있던 문제 (R-006)"
```

**[검토 게이트]** 확인할 것: Step 6의 전수 확인 결과.

---

## Task 1.9: R-008 — 유저 카운터 정상화

**목적:** 항상 0이라 죽어 있는 정원 체크를 실제로 동작하게 한다.
**커밋:** 있음 · **되돌리기:** 안전

**Files:** Modify `GameServer/UserManager.h`, `GameServer/UserManager.cpp`

---

- [ ] **Step 1: 죽은 코드임을 확인**

```bash
grep -rn "IncreaseUserCnt\|DecreaseUserCnt\|mCurrentUserCnt" GameServer/
```
Expected: **선언과 정의만 나오고 호출이 없다.** `mCurrentUserCnt`는 영원히 0이다.

- [ ] **Step 2: 카운터 대신 사전 크기를 쓴다**

`GameServer/UserManager.h`:
```cpp
	INT32 GetCurrentUserCnt() { return static_cast<INT32>(mUserIDDictionary.size()); }
```

- [ ] **Step 3: 죽은 멤버와 함수 삭제**

```cpp
	void IncreaseUserCnt() { mCurrentUserCnt++; }
	void DecreaseUserCnt() { if (mCurrentUserCnt > 0) mCurrentUserCnt--; }
	...
	INT32 mCurrentUserCnt = { 0 };
```

> **상태를 두 벌 관리하지 않는다.** 사전이 곧 로그인한 유저 목록이므로 그 크기가 정답이다.
> 카운터를 따로 두면 언젠가 반드시 어긋난다.

- [ ] **Step 4: `UserManager.cpp`의 잔여 참조 제거**

- [ ] **Step 5: 빌드** → Expected: 성공

- [ ] **Step 6: 정원 체크 시험 준비**

`ServerCore/Define.h`의 `MAX_CLIENT`를 **임시로 2**로 낮추고 빌드.

- [ ] **Step 7: 3번째 로그인이 거부되는지 확인**

DummyClient 3개로 각각 다른 ID 로그인.
Expected: 3번째가 `Result = 32`(`LOGIN_USER_USED_ALL_OBJ`)

⏸ **중단 조건:** 3번째도 통과하면 멈춰라. `MAX_CLIENT`가 세션 풀 크기이기도 해서
3번째 **접속 자체**가 안 될 수도 있다. 어떤 동작이었는지 알려줘 — 이건
"로그인 유저 수"와 "연결 수"를 구분해야 한다는 신호일 수 있다.

- [ ] **Step 8: 로그아웃 후 카운트가 줄어드는지 확인**

한 명을 끊고 새 ID로 로그인. Expected: 성공

- [ ] **Step 9: `MAX_CLIENT`를 100으로 복구**

⏸ **중단 조건:** 이걸 잊으면 안 된다. 복구했는지 반드시 확인해라.

- [ ] **Step 10: 빌드 + 정상 확인**

- [ ] **Step 11: 커밋**

```bash
git add GameServer/UserManager.h GameServer/UserManager.cpp
git commit -m "fix: 호출되지 않던 유저 카운터를 사전 크기로 대체 (R-008)

IncreaseUserCnt/DecreaseUserCnt가 아무 데서도 호출되지 않아 정원 체크가
항상 거짓이었다. 상태를 두 벌 관리하는 대신 mUserIDDictionary.size()를 쓴다."
```

**[검토 게이트]** 확인할 것: `MAX_CLIENT`가 100으로 돌아왔는지, Step 7의 관측 결과, 로그인 유저 수와 연결 수를 구분해야 하는지.

---

### ✅ Phase 1 완료 기준

- [ ] `Tests.exe` 전부 통과
- [ ] Debug / Release 둘 다 빌드
- [ ] DummyClient 2개로 정상 흐름 전체: login → enter → chat → leave → 재입장
- [ ] `ROOM_LEAVE_NOTIFY`가 `len=10`으로 온다
- [ ] `ROOM_CHAT_NOTIFY`의 `UserID`가 정확하다 (짧은 ID / 32자 ID 둘 다)
- [ ] 같은 ID 중복 로그인이 `Result=31`로 거부된다
- [ ] 끊고 다시 같은 ID로 로그인이 된다
- [ ] Release에서 20Hz 캐릭터 동기화 중 로그가 도배되지 않는다
- [ ] `raw 0 1001`을 보내도 서버가 계속 응답한다

**[Phase 게이트]** 여기서 내가 Phase 1 전체를 다시 읽는다.

---

# Phase 2 — 신뢰 경계 게이트

## Task 2.1: 판정 규칙을 순수 함수로 분리

**목적:** 크기·상태 검증 로직을 **테스트 가능한 형태로** 먼저 만든다. 아직 아무것도 연결하지 않는다.
**커밋:** 있음 · **되돌리기:** 안전 (아무 데서도 안 쓰이는 헤더 하나)

**Files:** Create `GameServer/PacketDispatchRule.h`, `Tests/Test_PacketDispatch.cpp`

**Interfaces:**
- Consumes: `DOMAIN_STATE::Enum`
- Produces: `stPacketHandler`, `IsAcceptablePacket(handler_, packetSize_, currentState_)` — Task 2.2가 쓴다

**왜 별도 헤더인가:** `PacketManager`는 스레드와 매니저들을 소유해서 테스트에서 인스턴스를 만들 수 없다. 판정 로직만 의존성 없는 헤더로 빼면 테스트가 그냥 포함해서 쓸 수 있다. **`GameServer`가 exe 프로젝트라 링크가 불가능하므로, 이 함수가 `inline`인 것이 중요하다.**

---

- [ ] **Step 1: 헤더 생성**

`GameServer/PacketDispatchRule.h`:
```cpp
#pragma once

#include <optional>

#include "Enum_DomainState.h"

// 패킷 하나를 어떻게 다룰지에 대한 등록 정보.
// 핸들러 포인터는 PacketManager가 채우므로 여기서는 void*로 두지 않고
// PacketManager.h에서 별도로 묶는다. 이 헤더는 "판정 규칙"만 담당한다.
struct stPacketRule
{
	UINT16 ExpectedSize = { 0 };                            // 0 = 크기 검사 생략(시스템 패킷)
	std::optional<DOMAIN_STATE::Enum> RequiredState = {};   // 없음 = 상태 검사 생략
};

// 이 패킷을 지금 처리해도 되는가.
// 순수 함수라 단위 테스트가 가능하고, inline이라 링크가 필요 없다.
inline bool IsAcceptablePacket(const stPacketRule& rule_,
	const UINT16 packetSize_, const DOMAIN_STATE::Enum currentState_)
{
	if (rule_.ExpectedSize != 0 && rule_.ExpectedSize != packetSize_)
		return false;

	if (rule_.RequiredState.has_value() && rule_.RequiredState.value() != currentState_)
		return false;

	return true;
}
```

⏸ **중단 조건:** `Enum_DomainState.h`가 `UINT16` 같은 Windows 타입에 의존해 단독 포함이 안 되면 알려줘. `#include <windows.h>`를 추가할지 판단이 필요하다.

- [ ] **Step 2: 테스트 프로젝트에 GameServer 포함 경로 추가**

`Tests` 속성 → C/C++ → 추가 포함 디렉터리:
```
$(SolutionDir)ServerCore;$(SolutionDir)GameServer
```

- [ ] **Step 3: 테스트 파일 생성 — 크기 검증**

`Tests/Test_PacketDispatch.cpp`:
```cpp
#include "pch.h"

#include "PacketDispatchRule.h"

TEST_CASE("크기가 다르면 거부한다", "[dispatch][R-005]")
{
	stPacketRule rule;
	rule.ExpectedSize = 10;
	rule.RequiredState = DOMAIN_STATE::LOGIN;

	REQUIRE(IsAcceptablePacket(rule, 10, DOMAIN_STATE::LOGIN) == true);
	REQUIRE(IsAcceptablePacket(rule, 6,  DOMAIN_STATE::LOGIN) == false);
	REQUIRE(IsAcceptablePacket(rule, 11, DOMAIN_STATE::LOGIN) == false);
	REQUIRE(IsAcceptablePacket(rule, 0,  DOMAIN_STATE::LOGIN) == false);
}
```

- [ ] **Step 4: 빌드 + 실행**

Run: `Tests.exe "[dispatch]"` → Expected: PASS

⏸ **중단 조건:** `PacketDispatchRule.h`를 못 찾으면 Step 2를 다시 확인해라.

- [ ] **Step 5: 테스트 — 상태 검증**

```cpp
TEST_CASE("상태가 다르면 거부한다", "[dispatch][R-009]")
{
	stPacketRule rule;
	rule.ExpectedSize = 10;
	rule.RequiredState = DOMAIN_STATE::ROOM;

	REQUIRE(IsAcceptablePacket(rule, 10, DOMAIN_STATE::ROOM)  == true);
	REQUIRE(IsAcceptablePacket(rule, 10, DOMAIN_STATE::LOGIN) == false);
	REQUIRE(IsAcceptablePacket(rule, 10, DOMAIN_STATE::NONE)  == false);
}
```

- [ ] **Step 6: 실행** → Expected: PASS

- [ ] **Step 7: 테스트 — 검사 생략 규칙**

```cpp
TEST_CASE("ExpectedSize 0은 크기 검사를 생략한다 (시스템 패킷)", "[dispatch]")
{
	stPacketRule rule;
	rule.ExpectedSize = 0;
	rule.RequiredState = {};

	REQUIRE(IsAcceptablePacket(rule, 0,   DOMAIN_STATE::NONE) == true);
	REQUIRE(IsAcceptablePacket(rule, 999, DOMAIN_STATE::ROOM) == true);
}

TEST_CASE("RequiredState 없음은 상태 검사를 생략한다", "[dispatch]")
{
	stPacketRule rule;
	rule.ExpectedSize = 10;
	rule.RequiredState = {};

	REQUIRE(IsAcceptablePacket(rule, 10, DOMAIN_STATE::NONE)  == true);
	REQUIRE(IsAcceptablePacket(rule, 10, DOMAIN_STATE::ROOM)  == true);
	REQUIRE(IsAcceptablePacket(rule, 11, DOMAIN_STATE::ROOM)  == false);   // 크기는 여전히 본다
}
```

- [ ] **Step 8: 전체 실행**

Run: `Tests.exe` → Expected: Phase 0의 12개 + dispatch 4개 전부 PASS

- [ ] **Step 9: 커밋**

```bash
git add GameServer/PacketDispatchRule.h Tests/Test_PacketDispatch.cpp
git commit -m "feat: 패킷 디스패치 판정 규칙을 순수 함수로 분리 (R-005, R-009 준비)

크기와 요구 상태 검증을 테스트 가능한 자유 함수로 만든다.
아직 아무 데서도 쓰이지 않는다 — 연결은 다음 커밋."
```

**[검토 게이트]** 확인할 것: 헤더 자립성, `optional` 사용이 이 저장소 스타일과 맞는지(다른 곳에서 안 쓰고 있다면 판단이 필요하다).

---

## Task 2.2: 등록 테이블을 규칙 포함 형태로 교체

**목적:** 핸들러를 등록할 때 크기와 요구 상태를 **함께** 등록하게 만든다. 검증을 빠뜨리는 것이 불가능해진다.
**커밋:** 있음 · **되돌리기:** 안전
**⚠ Step 2~6 사이에는 빌드가 깨진다.**

**Files:** Modify `GameServer/PacketManager.h`, `GameServer/PacketManager.cpp`

**Interfaces:**
- Consumes: Task 2.1의 `stPacketRule`, `IsAcceptablePacket`

---

- [ ] **Step 1: 현재 테이블 타입을 확인**

`GameServer/PacketManager.h`:
```cpp
	using PROCESS_RECV_PACKET_FUNCTION = void(PacketManager::*)(UINT32, UINT16, shared_ptr<char[]>);
	unordered_map<int, PROCESS_RECV_PACKET_FUNCTION> mRecvFunctionDictionary;
```

- [ ] **Step 2: 핸들러 + 규칙을 묶는 구조체 추가**

`GameServer/PacketManager.h`, 클래스 선언 안 `private:` 영역:
```cpp
	using PROCESS_RECV_PACKET_FUNCTION = void(PacketManager::*)(UINT32, UINT16, shared_ptr<char[]>);

	struct stPacketHandler
	{
		PROCESS_RECV_PACKET_FUNCTION Func = { nullptr };
		stPacketRule Rule = {};
	};

	unordered_map<UINT16, stPacketHandler> mRecvFunctionDictionary;
```

- [ ] **Step 3: 헤더에 include 추가**

`GameServer/PacketManager.h` 상단:
```cpp
#include "PacketDispatchRule.h"
```

- [ ] **Step 4: 시스템 패킷 등록을 새 형태로**

`GameServer/PacketManager.cpp`, `Init()`:
```cpp
	mRecvFunctionDictionary.clear();

	// 시스템 패킷: 서버가 스스로 넣으므로 크기·상태 검사 없음
	mRecvFunctionDictionary[PACKET_ID::SYS_USER_CONNECT_RESPONSE] = { &PacketManager::ProcessSysUserConnectResponse, {} };
	mRecvFunctionDictionary[PACKET_ID::SYS_USER_CONNECT]          = { &PacketManager::ProcessUserConnect, {} };
	mRecvFunctionDictionary[PACKET_ID::SYS_USER_DISCONNECT]       = { &PacketManager::ProcessUserDisConnect, {} };
```

- [ ] **Step 5: 클라이언트 패킷 등록을 새 형태로**

```cpp
	// 클라이언트 패킷: 크기와 상태를 모두 검사한다
	mRecvFunctionDictionary[PACKET_ID::LOGIN_REQUEST] =
		{ &PacketManager::ProcessLogin,
		  { sizeof(LOGIN_REQUEST_PACKET), DOMAIN_STATE::NONE } };

	mRecvFunctionDictionary[PACKET_ID::ROOM_ENTER_REQUEST] =
		{ &PacketManager::ProcessEnterRoom,
		  { sizeof(ROOM_ENTER_REQUEST_PACKET), DOMAIN_STATE::LOGIN } };

	mRecvFunctionDictionary[PACKET_ID::ROOM_LEAVE_REQUEST] =
		{ &PacketManager::ProcessLeaveRoom,
		  { sizeof(ROOM_LEAVE_REQUEST_PACKET), DOMAIN_STATE::ROOM } };

	mRecvFunctionDictionary[PACKET_ID::ROOM_CHAT_REQUEST] =
		{ &PacketManager::ProcessRoomChatMessage,
		  { sizeof(ROOM_CHAT_REQUEST_PACKET), DOMAIN_STATE::ROOM } };

	mRecvFunctionDictionary[PACKET_ID::CHARACTER_SYNC] =
		{ &PacketManager::ProcessCharacterSync,
		  { sizeof(CHARACTER_SYNC_PACKET), DOMAIN_STATE::ROOM } };

	CreateComponent(maxClient_);
```

> `sizeof(...)`가 `size_t`라 `UINT16`으로 좁혀진다는 경고가 날 수 있다.
> `static_cast<UINT16>(...)`를 감싸거나, `stPacketRule` 생성자를 두는 편이 낫다.
> 판단해서 정해라.

- [ ] **Step 6: `ProcessRecvPacket`에 게이트 적용**

`GameServer/PacketManager.cpp`:
```cpp
void PacketManager::ProcessRecvPacket(const UINT32 clientIndex_, const UINT16 packetId_, const UINT16 packetSize_, shared_ptr<char[]> pPacket_)
{
	auto iter = mRecvFunctionDictionary.find(packetId_);
	if (iter == mRecvFunctionDictionary.end())
	{
		spdlog::warn("[dispatch] unknown PacketId({}) from client({})", packetId_, clientIndex_);
		return;
	}

	const auto& handler = iter->second;

	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	const auto currentState = pUser->GetDomainState();

	if (IsAcceptablePacket(handler.Rule, packetSize_, currentState) == false)
	{
		spdlog::warn("[dispatch] rejected PacketId({}) size({}) state({}) from client({})",
			packetId_, packetSize_, static_cast<int>(currentState), clientIndex_);
		return;
	}

	(this->*(handler.Func))(clientIndex_, packetSize_, pPacket_);
}
```

- [ ] **Step 7: 빌드** → Expected: 성공

⏸ **중단 조건:** `PacketManager.h`가 `PacketDispatchRule.h`를 포함하면서 순환 포함이 생기면 알려줘.

- [ ] **Step 8: 핸들러 안의 중복 크기 검사 제거**

`ProcessLogin`에서:
```cpp
	if (LOGIN_REQUEST_PACKET_SIZE != packetSize_)
		return;
```
을 지운다. 게이트가 대신한다.

**두 곳에서 같은 검사를 하면 나중에 한쪽만 고쳐진다.**

- [ ] **Step 9: 빌드** → Expected: 성공

- [ ] **Step 10: 정상 흐름 전체 확인**

DummyClient 2개:
```
(A) login alice
(A) enter 0
(B) login bob
(B) enter 0
(A) chat hello
(A) leave
```
Expected: 전부 정상. **`rejected` 로그가 하나도 뜨면 안 된다.**

⏸ **중단 조건:** `rejected`가 뜨면 상태 요구가 실제 흐름과 어긋난 것이다.
어느 패킷이 어떤 상태에서 거부됐는지 로그를 그대로 알려줘.
**이 Task에서 가장 깨지기 쉬운 지점이다.**

- [ ] **Step 11: fl-client로도 확인**

DummyClient는 내가 설계한 순서대로만 보낸다. **실제 클라이언트는 다를 수 있다.**
특히 `ROOM_LEAVE_REQUEST`를 룸 밖에서 보내는지, 로그인 응답 전에 다음 패킷을 보내는지 확인해라.

⏸ **중단 조건:** fl-client에서 `rejected`가 뜨면 멈춰라. 상태 요구를 완화할지, 클라이언트를 고칠지 판단이 필요하다.

- [ ] **Step 12: 거부 경로 확인 — 로그인 전 룸 입장**

새 DummyClient에서 로그인 없이:
```
enter 0
```
Expected: 서버 로그에 `rejected PacketId(206) size(10) state(0)`. 서버는 정상 유지.

**이전에는 이게 통과해서 빈 UserID로 룸에 입장했다.**

- [ ] **Step 13: 거부 경로 확인 — 잘못된 크기**

```
login testuser
raw 6 206
```
Expected: `rejected PacketId(206) size(6)`. **크래시 없음.**

**이전에는 6바이트 버퍼를 `ROOM_ENTER_REQUEST_PACKET*`로 캐스팅해 힙 범위 밖을 읽었다.**

- [ ] **Step 14: 거부 경로 확인 — 미등록 ID**

```
raw 20 9999
```
Expected: `unknown PacketId(9999)`. 크래시 없음.

- [ ] **Step 15: 거부 후에도 정상 동작하는지 확인**

Step 12~14를 보낸 그 연결에서 이어서:
```
login testuser
enter 0
chat still works
```
Expected: 정상. **거부가 연결을 망가뜨리지 않는다.**

- [ ] **Step 16: 커밋**

```bash
git add GameServer/PacketManager.h GameServer/PacketManager.cpp
git commit -m "feat: 패킷 디스패처에 크기·상태 검증 게이트 추가 (R-005, R-009)

핸들러마다 흩어질 검증을 등록 테이블 한 곳으로 모은다. 핸들러를 추가하면
크기와 요구 상태를 함께 등록해야 하므로 검증을 빠뜨릴 수 없다."
```

**[검토 게이트]** 확인할 것: Step 10/11의 정상 흐름 결과, Step 12~14의 거부 로그, 시스템 패킷이 검사를 생략하는 게 안전한지.

---

## Task 2.3: 거부 로그 폭주 방지

**목적:** 공격자가 초당 수천 개의 잘못된 패킷을 보내면 `warn` 로그가 디스크를 채우고 서버를 느리게 만든다.
**커밋:** 있음 · **되돌리기:** 안전

**Files:** Modify `GameServer/PacketManager.cpp`

> **이 Task는 선택이다.** Task 2.2까지로 기능은 완성이다. 다만 "거부를 로그로 남긴다"는
> 결정이 그 자체로 새 공격면을 만들었으므로 짚고 넘어간다. 하기 싫으면
> `limits.md`에 「알면서 안 한 것」으로 옮기고 건너뛰어라. 그것도 기록이다.

---

- [ ] **Step 1: 실제로 문제가 되는지 확인**

DummyClient를 고쳐 `raw 6 206`을 루프로 1만 번 보낸다.
Expected: 로그가 폭주하고 처리량이 떨어진다.

⏸ **중단 조건:** 체감할 만한 문제가 없으면 이 Task를 건너뛰고 `limits.md`에 기록해라. **측정 없이 최적화하지 않는다.**

- [ ] **Step 2: 세션별 거부 카운터 추가**

`User`에 필드 하나를 더한다:
```cpp
	UINT32 mRejectCount = { 0 };
```
`User::Clear()`에서 0으로 되돌린다.

- [ ] **Step 3: 로그를 조건부로**

```cpp
	if (IsAcceptablePacket(handler.Rule, packetSize_, currentState) == false)
	{
		pUser->IncreaseRejectCount();

		// 처음 몇 번만 남긴다. 이후는 조용히 버린다.
		if (pUser->GetRejectCount() <= 5)
		{
			spdlog::warn("[dispatch] rejected PacketId({}) size({}) state({}) from client({})",
				packetId_, packetSize_, static_cast<int>(currentState), clientIndex_);
		}
		return;
	}
```

- [ ] **Step 4: Step 1을 다시 돌려 확인**

Expected: 로그가 5줄만 남는다

- [ ] **Step 5: 정상 흐름이 안 깨졌는지 확인**

- [ ] **Step 6: 커밋**

```bash
git add GameServer/
git commit -m "fix: 패킷 거부 로그를 세션당 5회로 제한

거부 자체를 로그로 남기는 것이 새 공격면이 된다. 처음 몇 번만 기록한다."
```

**[검토 게이트]** 확인할 것: 5라는 숫자의 근거, 임계를 넘으면 연결을 끊는 게 나은지(그건 Phase 4에서 세션 소유권과 함께 하는 게 자연스럽다).

---

## Task 2.4: 문서 마감

**목적:** Phase 0–2의 결과를 문서에 반영하고, 남은 것을 명시한다.
**커밋:** 있음

**Files:** Modify `docs/wiki/review-log.md`, `docs/wiki/limits.md`, `AGENTS.md`, `docs/wiki/architecture.md`

---

- [ ] **Step 1: review-log의 해결 항목 상태 변경**

다음 항목의 `상태`를 `해결`로:
R-003, R-004, R-005, R-006, R-007, R-008, R-009, R-011, R-012, R-014, R-017

- [ ] **Step 2: 각 항목의 **결정** / **결과** 칸을 채운다**

**이건 사람만 채울 수 있는 칸이다** (→ AGENTS.md). 각각에:
- 커밋 해시
- 어떻게 확인했는지 (어떤 시나리오로 재현하고 무엇을 봤는지)
- 제안과 다르게 한 부분이 있으면 그 이유

- [ ] **Step 3: 반려한 게 있으면 decisions.md로 옮긴다**

Task 1.2 Step 6(템플릿 헬퍼), Task 2.3(로그 제한) 등에서 "지금 안 한다"고 판단한 것이 있으면
`decisions.md`의 「반려 기록」에 X-번호로 남긴다.

- [ ] **Step 4: 미해결 요약 표 정리**

해결된 행을 「해결됨」 표로 옮긴다. **지우지 않는다.**

남는 것: R-001, R-002, R-010, R-013, R-015, R-016

- [ ] **Step 5: `AGENTS.md` 불변식 표 갱신**

- I-7의 강제 수단: `static_assert`
- I-9 추가:

| I-9 | 핸들러는 등록된 크기·상태 조건을 만족할 때만 실행된다 | `IsAcceptablePacket` (디스패처) |

- [ ] **Step 6: `AGENTS.md` 코드 지도에 Tests 반영**

`Tests/` 항목의 "계획 Phase 0에서 신설 예정"을 실제 내용으로 바꾼다.

- [ ] **Step 7: `architecture.md`의 수신 흐름 갱신**

`ProcessRecvPacket`이 이제 게이트를 거친다는 것을 반영한다.
R-016 경고 박스는 **그대로 둔다** — 아직 안 고쳤다.

- [ ] **Step 8: `limits.md` 갱신**

「알고 있는 미해결 결함」에서 해결된 행 제거. **남은 것을 명시적으로 추가:**
- 프레이밍이 깨진 세션을 `Clear()`만 하고 **끊지는 않는다**
- 거부 임계를 넘어도 연결을 유지한다
- 인증은 여전히 없다

「품질 인프라 부재」의 "자동화 테스트" 행 갱신 — `PacketBuffer`와 디스패처 판정은 덮였고, 나머지는 아직 없다.

- [ ] **Step 9: 커밋**

```bash
git add docs/ AGENTS.md
git commit -m "docs: Phase 0-2 결과 반영

해결 항목 상태 갱신, 불변식 I-9 추가, 남은 한계 명시."
```

---

### ✅ Phase 2 완료 기준

- [ ] `Tests.exe` 16개 테스트 전부 통과
- [ ] Debug / Release 둘 다 빌드
- [ ] fl-client 정상 흐름에서 `rejected` 로그가 하나도 안 뜬다
- [ ] 로그인 전 `enter`가 거부된다
- [ ] 잘못된 크기 패킷이 거부되고 크래시가 없다
- [ ] 미등록 PacketId가 거부되고 크래시가 없다
- [ ] 거부 후에도 그 연결이 정상 동작한다
- [ ] review-log의 11개 항목이 `해결`이고 **결정/결과가 채워져 있다**

**[Phase 게이트]** Phase 0–2 전체를 다시 읽고, 목표였던 **"fl-client로 믿을 수 있는 테스트가 가능한 상태"**가 되었는지 판정한다.

---

# 이후 로드맵 (별도 계획)

**여기부터는 지금 상세 계획을 쓰지 않는다.** 이유는 둘이다.

1. 각 Phase가 독립적으로 동작하는 산출물이고, 앞 Phase의 결과가 뒤의 설계를 바꾼다.
2. **Phase 3은 네가 답하기 전엔 설계할 수 없다.** `decisions.md`의 D-004 — SendThread를 왜 껐는지가 송신 재작성의 방향을 정한다.

| Phase | 내용 | 항목 | 시작 조건 |
|---|---|---|---|
| **3** | 송신 경로 재작성 — pending 큐 + 링버퍼, 락 도메인 통일, SendThread 죽은 코드 결론, `SendPacketTo` 템플릿 헬퍼 | R-001, R-002, D-004 | **D-004 답변 필요** |
| **4** | 수신 소유권 이전 — `PacketBuffer`를 `stClientInfo`로, 큐 원소를 패킷으로, 세션 generation, 프레이밍 깨진 세션 절단 | R-016, R-010, D-005 재검토 | Phase 3 완료 |
| **5** | 룸 액터 + 고정 틱 — 룸별 잡큐, 워커 디스패치, `m_RoomLock` 제거, 룸 설정 외부화 | R-013, R-015, D-003 → D-010 | **"룸 10/정원 4가 확정값인가", "어떤 게임인가" 답변 필요** |
| **6** | 관측 + 부하 — 메트릭, DummyClient 다중 접속 확장, 실측 | limits.md의 측정 항목 | Phase 5 완료 |

Phase 3에 착수할 때 이 계획과 같은 수준으로 상세 계획을 쓴다.

---

# 이 계획에서 의도적으로 안 하는 것

나중에 "왜 안 했지?" 싶을 것들. **판단이지 누락이 아니다.**

| 안 하는 것 | 이유 |
|---|---|
| `PacketManager.cpp` 분할 | Phase 2에서 핸들러가 짧아진다. 그 후에 판단하는 게 정확하다 |
| `SendPacketTo` 템플릿 헬퍼 | 호출부 12곳 동시 변경. Phase 3에서 송신 경로를 어차피 건드리므로 그때가 총 변경량이 적다 (Task 1.2 Step 6) |
| 프레이밍 깨진 세션 절단 | `PacketBuffer`에서 세션 종료까지 배선이 필요. Phase 4의 소유권 이전과 함께 하는 게 자연스럽다 |
| 인증 / 비밀번호 검증 | DB 방향이 안 정해졌다 (D-008). 지금 넣으면 두 번 짓는다 |
| `ERROR_CODE` 정리 | 클라이언트도 같이 고쳐야 한다. 「와이어 무변경」 제약에 걸린다 |
| 룸 설정 외부화 (R-015) | Phase 5에서 룸 구조 자체가 바뀐다. 그때 같이 |
| 멤버 접두 `m_` → `m` 통일 | 순수 이름 변경 커밋은 diff만 키우고 검토를 방해한다. 건드리는 파일에서 기회 있을 때만 |
| `PACKET_HEADER`를 5바이트로 축소 | fl-client 동시 수정 필요. 이득(1바이트)보다 위험이 크다 |

---

# 자기 점검

**커버리지** — review-log의 R-003 ~ R-009, R-011, R-012, R-014, R-017이 Task에 배정됨.
R-001/R-002는 Phase 3, R-010/R-016은 Phase 4, R-013/R-015는 Phase 5로 **명시적으로 이월**.

**미확정 값**
- Task 0.8의 `static_assert` 숫자는 계산값 → 컴파일로 검증하는 절차를 넣었다 (Step 11에 기록표)
- Task 0.7 Step 1에서 `sizeof(PACKET_HEADER)`를 실측한 뒤 진행하도록 순서를 잡았다
- Task 2.1 Step 1의 `Enum_DomainState.h` 자립성은 확인이 필요 → 중단 조건으로 명시

**타입 일관성**
- `AddUser(const string&, int)` — Task 1.4에서 정의, Task 1.9에서 같은 파일을 다시 건드리지만 시그니처는 유지
- `GetUserId()` → `const string&` — Task 1.5에서 변경, 호출부는 `.c_str()`이라 영향 없음
- `stPacketRule` / `IsAcceptablePacket` — Task 2.1에서 정의, Task 2.2에서 소비
- `stPacketHandler` — Task 2.2에서만 존재 (`PacketManager` 내부 타입)

**알려진 위험 (가장 깨지기 쉬운 순서)**
1. **Task 2.2 Step 10~11** — 상태 요구가 실제 클라이언트 흐름과 어긋나면 정상 동작이 막힌다. 그래서 DummyClient와 fl-client 양쪽으로 확인하는 스텝을 따로 뒀다.
2. **Task 1.4 Step 13** — 재로그인. `mUserIDDictionary`에서 안 지워지는 문제가 여기서만 드러난다.
3. **Task 0.2 Step 13** — Catch2 링크. 환경 문제라 시간을 먹을 수 있다.
4. **Task 1.9 Step 9** — `MAX_CLIENT` 복구를 잊으면 이후 모든 테스트가 이상해진다.

**빌드가 깨지는 구간이 있는 Task** (쪼갤 수 없어 한 커밋으로 묶음)
- Task 0.2 (프로젝트 골격)
- Task 1.3 (DummyClient)
- Task 1.4 (R-003 시그니처 변경)
- Task 2.2 (등록 테이블 교체)
