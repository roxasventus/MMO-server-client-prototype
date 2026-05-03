# MMO-server-client-prototype

Unity 클라이언트와 C++ REST API 서버를 연동하여 구현한 MMORPG 핵심 시스템 프로토타입입니다.

이 프로젝트는 완성형 MMORPG를 목표로 한 것이 아니라, 온라인 게임에서 필요한 핵심 흐름인 **인증, 캐릭터 관리, 데이터베이스 연동, 월드 입장, 채팅, 플레이어 위치 동기화**를 직접 구현하며 클라이언트-서버 구조를 학습하고 검증하는 것을 목표로 했습니다.
<br>
<br>

## 📌 프로젝트 소개

본 프로젝트는 Unity 클라이언트와 C++ 기반 게임 서버를 HTTP REST API로 연결한 서버-클라이언트 구조의 게임 프로토타입입니다.

클라이언트는 Unity로 제작했으며, 서버는 C++로 구현했습니다. 서버는 MySQL 데이터베이스와 연동하여 유저 정보, 캐릭터 정보, 채팅 메시지, 월드 입장 상태, 플레이어 위치 정보를 처리합니다.

또한 AWS EC2 환경에 서버와 DB를 배포하여, 로컬 환경이 아닌 외부 네트워크에서도 Unity 클라이언트가 서버에 접속할 수 있도록 구성했습니다.
<br>
<br>

## 🛠 사용 기술

### Client

- Unity
- C#
- UnityWebRequest
- CharacterController
- Cinemachine

### Server

- C++
- cpp-httplib
- nlohmann/json
- jwt-cpp
- MySQL C API / libmysqlclient
- bcrypt

### Database / Infra

- MySQL
- AWS EC2
- Ubuntu Server
- Environment Variables
- Git / GitHub
<br>
<br>

## 🧩 주요 기능

### 1. 회원가입 / 로그인

- Unity 클라이언트에서 ID와 비밀번호를 입력받아 서버에 요청
- 서버는 MySQL DB에서 유저 정보를 조회
- 비밀번호는 평문 저장이 아닌 bcrypt 해시로 저장
- 로그인 성공 시 JWT 토큰 발급
- 이후 API 요청은 JWT 토큰을 통해 인증 처리

### 2. JWT 기반 Stateless 인증

- 로그인 성공 시 서버가 JWT 발급
- 클라이언트는 발급받은 토큰을 저장한 뒤 API 요청마다 Authorization Header에 포함
- 서버는 매 요청마다 토큰을 검증하여 유저 식별
- 서버 메모리에 세션을 저장하지 않아도 인증 상태를 유지할 수 있도록 구성

### 3. 캐릭터 관리

- 캐릭터 생성
- 캐릭터 목록 조회
- 캐릭터 선택
- 캐릭터 삭제
- 캐릭터의 레벨, 경험치, 골드, HP, MP, 공격력, 방어력, 위치 정보 관리

### 4. 월드 입장

- 선택한 캐릭터로 월드에 입장
- 서버는 현재 접속 중인 플레이어 목록 관리
- 입장 시 자신의 스폰 정보와 주변 플레이어 정보를 클라이언트에 전달

### 5. 플레이어 위치 동기화

- Unity 클라이언트에서 일정 주기로 자신의 위치와 회전값을 서버에 전송
- 서버는 플레이어별 최신 위치 정보를 저장
- 클라이언트는 서버에서 다른 플레이어들의 위치 정보를 받아 Remote Player를 생성하거나 갱신
- 캐릭터 ID를 Key로 사용하는 Dictionary 구조를 통해 Remote Player 오브젝트를 관리

### 6. 채팅 시스템

- 클라이언트에서 채팅 메시지를 서버로 전송
- 서버는 메시지를 MySQL DB에 저장
- 클라이언트는 일정 주기로 새 메시지를 요청
- 접속 이후의 메시지를 중심으로 표시하여 불필요한 전체 히스토리 로딩을 줄임

### 7. 환경 설정 분리

- 서버의 DB 접속 정보와 JWT Secret은 코드에 직접 작성하지 않고 환경변수로 관리
- Unity 클라이언트의 서버 주소는 `.env` 파일의 `SERVER_URL` 값을 읽어 설정
- 실제 `.env` 파일은 GitHub에 업로드하지 않고 `.env.example`만 제공
- 로컬 서버, AWS EC2 서버 등 실행 환경에 따라 설정값을 쉽게 교체할 수 있도록 구성
<br>
<br>

## 🔧 시스템 구조
Unity Client<br>
↓<br>
HTTP REST API<br>
↓<br>
C++ Game Server<br>
↓<br>
MySQL Database

Unity 클라이언트는 서버에 HTTP 요청을 보내고, 서버는 요청을 처리한 뒤 JSON 형식으로 응답합니다.

서버는 인증, 캐릭터 데이터, 월드 입장 상태, 채팅 메시지, 플레이어 위치 정보를 처리하며, 영구적으로 보관해야 하는 정보는 MySQL에 저장합니다.
<br>
<br>

## 🔐 보안 처리
### bcrypt 비밀번호 해싱

유저 비밀번호는 DB에 평문으로 저장하지 않고 bcrypt를 사용해 해시 처리했습니다.

이를 통해 DB가 노출되더라도 원본 비밀번호가 직접적으로 유출되지 않도록 했습니다.

### Prepared Statement

SQL 쿼리 작성 시 문자열을 직접 이어붙이는 방식 대신 Prepared Statement를 사용했습니다.

이를 통해 로그인, 회원가입, 캐릭터 생성 등 사용자 입력이 포함되는 API에서 SQL Injection 위험을 줄였습니다.

### 환경변수 사용

DB 접속 정보와 JWT Secret은 코드에 직접 작성하지 않고 환경변수로 관리했습니다.
