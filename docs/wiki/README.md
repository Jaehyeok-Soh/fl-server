# fl-server wiki

DirectX11 자체엔진 클라이언트(`fl-client`)와 붙는 룸 기반 MO 게임 서버.
Windows IOCP, C++20, 단일 프로세스.

이 위키는 **사람과 LLM이 같은 문서를 읽고 같은 그림을 갖기 위한 것**이다.
코드를 읽지 않고도 시스템 전체를 머릿속에 올릴 수 있을 만큼 조밀하게, 대신
코드가 이미 말해주는 것(함수 시그니처 나열 같은 것)은 쓰지 않는다.

## 목차

진입점은 저장소 루트의 **[`CLAUDE.md`](../../CLAUDE.md)**다. 불변식·관례·검증
방법이 거기 있고, 매 세션 자동으로 읽힌다. 이 디렉터리는 그보다 깊은 내용이다.

| 문서 | 성격 | 언제 읽나 |
|---|---|---|
| [structure.md](structure.md) | 설명서 | **전체 구성도.** 처음 볼 때, 어디에 뭐가 있는지 찾을 때 |
| [target-structure.md](target-structure.md) | 청사진 | **목표 구성도.** 어디로 가는지, 무엇이 아직 미결인지 |
| [target-spec.md](target-spec.md) | 규범 | **목표 사양서.** 무엇을 충족해야 하는지. 요구사항 154개 |
| [architecture.md](architecture.md) | 설명서 | 스레드/패킷 흐름을 건드릴 때 |
| [protocol.md](protocol.md) | 계약 | 패킷을 추가·변경할 때 |
| [decisions.md](decisions.md) | 원장 | **제안하기 전에.** 이미 버려진 안일 수 있다 |
| [review-log.md](review-log.md) | 원장 | 미해결 지적 확인 / 검토 결과 기록 |
| [limits.md](limits.md) | 스냅샷 | "이거 왜 안 해놨지?" 싶을 때 |

**설명서와 원장은 규칙이 다르다.**
설명서(architecture, protocol)에는 **크기 상한**이 있다 — 길어지면 코드를
베끼기 시작했다는 신호다. 원장(decisions, review-log)은 **추가만 하고 지우지
않는다** — 6개월치 사고 이력이 쌓이는 게 정상이다.
스냅샷(limits)은 현재 상태만 담고, 해결되면 지운다.

## ⛔ 협업 규칙 — 소스 코드는 사람만 수정한다

**정본은 저장소 루트의 [AGENTS.md](../../AGENTS.md)다.** 모든 AI 도구는
작업 전에 그 파일을 읽어야 한다. 여기엔 요약만 둔다.

- **개발/수정은 사람(Jaehyeok)이 한다.** AI는 소스 코드를 수정하지 않는다.
  `*.cpp` `*.h` `*.vcxproj` `*.sln` 생성·수정·삭제 금지, git 쓰기 작업 금지.
  "어차피 한 줄인데"는 예외가 아니다.
- **AI는 검토와 피드백을 한다.** 근거(심볼 위치 + 깨지는 시나리오)를 대고,
  고치는 대신 **"이렇게 고치면 돼" + 코드 블록**으로 제시한다.
- **문서는 AI가 관리한다.** `docs/**`와 `*.md`는 AI가 쓰고 유지한다.
  단 사람이 채우기로 한 칸(검토 로그의 **결정**/**결과**, 결정 기록의
  **버린 대안**)은 비워둔 채 물어본다.
- **주고받은 것은 기록한다.** 검토는 [review-log.md](review-log.md),
  설계 결정과 반려는 [decisions.md](decisions.md).

이 규칙이 왜 있는지는 [decisions.md](decisions.md)의 **D-009**에 있다.

규칙이 배치된 파일 — 도구마다 자동으로 읽는 파일이 다르므로 세 군데에 둔다.

| 파일 | 읽는 도구 |
|---|---|
| [AGENTS.md](../../AGENTS.md) | **정본.** Codex, Cursor, Amp, Windsurf, Jules 등 |
| [CLAUDE.md](../../CLAUDE.md) | Claude Code |
| [.github/copilot-instructions.md](../../.github/copilot-instructions.md) | GitHub Copilot |

## 빌드 / 실행

```
fl-server.sln          # Visual Studio, x64
├── ServerCore         # 정적 라이브러리: IOCP, 세션, 패킷 버퍼
├── GameServer         # 실행 파일: 게임 로직 (main)
└── DummyClient        # 테스트용 더미 클라이언트
```

의존성은 vcpkg로 받는다 (`ServerCore/CorePch.h` 상단 주석 참조).

```
vcpkg install spdlog:x64-windows
vcpkg integrate install
```

기본 포트 9000, 최대 접속 100. 콘솔에 `quit` 입력으로 종료.
