# 🚀 Space War - System Programming Project

터미널 환경에서 즐기는 **실시간 슈팅 서바이벌 게임 Space War**입니다.
`ncurses` 라이브러리를 활용한 TUI(Text User Interface) 그래픽과 **TCP 소켓 통신 기반 멀티플레이**를 지원합니다.

---

## 📌 프로젝트 소개

**Space War**는 시스템 프로그래밍 과제로 제작된 리눅스 전용 터미널 게임입니다.
플레이어는 사방에서 날아오는 화살과 위험 구역(**Red Zone**)을 피해 최대한 오래 생존해야 합니다.

* 싱글 플레이: 혼자서 최고 점수에 도전
* 멀티 플레이: TCP 기반 1:1 실시간 대전

---

## ✨ 주요 기능

### 🎮 메인 메뉴 (Launcher)

* 게임 모드 선택
* 점수판(Hall of Fame) 확인
* 키보드 기반 직관적인 UI 제공

### 🧍 싱글 플레이 (Single Play)

* 시간 기반 점수 시스템의 서바이벌 모드
* 시간이 지날수록 난이도 증가
* 게임 종료 후 점수 파일 저장 및 랭킹 등록

### 👥 멀티 플레이 (Multi Play)

* TCP 소켓을 이용한 실시간 1:1 대전
* **HOST**: 서버 생성 후 클라이언트 접속 대기
* **JOIN**: IP 주소 입력을 통해 서버 접속
* 상대방보다 오래 생존하면 승리

### 🧰 아이템 시스템

| 키 | 아이템             | 설명                 |
| - | --------------- | ------------------ |
| 1 | 무적 (Invincible) | 일정 시간 동안 화살 데미지 무시 |
| 2 | 회복 (Heal)       | 체력 1 회복 (최대 3)     |
| 3 | 슬로우 (Slow)      | 일정 시간 동안 화살 속도 감소  |

### ☠️ 위협 요소

* **화살(Arrow)**: 무작위 위치에서 생성되어 플레이어를 향해 이동
* **레드존(Red Zone)**: 일정 시간 후 폭발하여 데미지 발생
* **특수 웨이브(Special Wave)**: 주기적으로 대량의 화살 패턴 등장

---

## 🛠️ 설치 및 빌드 방법

### 1️⃣ 필수 라이브러리 설치

본 게임은 Linux 환경에서 실행되며, `ncurses` 라이브러리가 필요합니다.

```bash
sudo apt-get update
sudo apt-get install libncurses5-dev libncursesw5-dev
```

---

### 2️⃣ 프로젝트 빌드

본 프로젝트는 **Makefile**을 제공하므로 `make` 명령어로도 간단히 빌드할 수 있습니다.

```bash
# Makefile을 이용한 전체 빌드
make
```

> 위 명령어 실행 시 `bin/` 디렉토리에 실행 파일들이 자동으로 생성됩니다.

아래는 Makefile을 사용하지 않고 **직접 컴파일하는 방법**입니다.

프로젝트 루트 디렉토리에서 아래 명령어를 순서대로 실행하세요.

```bash
# 1. bin 디렉토리 생성
mkdir -p bin

# 2. 메뉴 (Launcher) 빌드
gcc -o bin/menu src/menu.c src/common.c src/score.c -Iinclude -lncursesw

# 3. 싱글 플레이 빌드
gcc -o bin/single_play src/single_play.c src/game_logic.c src/view.c src/item.c src/common.c -Iinclude -lncursesw -lpthread

# 4. 멀티 플레이 서버 빌드
gcc -o bin/server src/server.c src/game_logic.c src/item.c src/common.c -Iinclude -lncursesw -lpthread

# 5. 멀티 플레이 클라이언트 빌드
gcc -o bin/client src/client.c src/view.c src/game_logic.c src/common.c -Iinclude -lncursesw -lpthread
```

---

## 🎥 데모 영상 (Demo Video)

아래 링크를 통해 **게임 플레이 데모 영상**을 확인할 수 있습니다.

* 📺 YouTube: [https://youtu.be/YU_Z-v1135](https://youtu.be/YU_Z-v1135s)

> 싱글 플레이 및 멀티 플레이 시연 영상이 포함되어 있습니다.

---

## ▶️ 실행 방법

빌드가 완료되면 아래 명령어로 게임을 시작합니다.

```bash
./bin/menu
```

---

## ⌨️ 조작 방법

### 📂 메뉴 조작

* ↑ / ↓ 또는 `k` / `j` : 메뉴 이동
* `Enter` : 선택
* `Q` : 뒤로 가기 / 종료

### 🕹️ 게임 플레이

* 방향키 : 캐릭터 이동
* `1` : 무적 아이템 사용
* `2` : 힐 아이템 사용
* `3` : 슬로우 아이템 사용
* `Q` : 게임 포기 (패배 처리)

---

## 🧭 게임 모드 설명

* **SCOREBOARD**
  저장된 상위 10개 점수 기록(Hall of Fame) 확인

* **1 PLAYER**
  싱글 플레이 모드 시작 → 종료 후 이름 입력 시 점수 저장

* **2P HOST**
  멀티 플레이 서버 생성 및 대기

* **2P JOIN**
  IP 주소 입력 후 서버 접속 (로컬 테스트: `127.0.0.1`)

* **EXIT**
  게임 종료

---
## 👥 팀원 정보

| 프로필 | 이름 | 역할 | GitHub |
|---|---|---|---|
| <img src="https://github.com/yoonhoc.png" width="48" /> | 최윤호 | 게임 로직 개발, 싱글/멀티플레이 개발 | https://github.com/yoonhoc |
| <img src="https://github.com/Sumin020726.png" width="48" /> | 정수민 | TUI 개발, 스코어보드 개발 | https://github.com/Sumin020726 |



---

## 📎 참고 사항

* 본 프로젝트는 **Linux 환경**을 기준으로 개발되었습니다.
* 멀티 플레이는 동일 네트워크 환경에서 테스트하는 것을 권장합니다.

---

🎯 **경북대학교 2025-2 System Programming 수업 과제용 프로젝트**
