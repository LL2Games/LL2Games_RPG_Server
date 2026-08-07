# ChannelServer 통합·부하 테스트 결과

## 1. 테스트 목적

패킷 규격, 일회성 인증 티켓, 플레이어 상태 저장 기능을 변경한 뒤 다음 항목을 검증했습니다.

- 패킷 경계 입력 처리
- 인증 티켓 재사용·만료·채널 불일치 거부
- fd 재사용 상황의 오래된 인증 결과 차단
- MySQL 트랜잭션 커밋과 롤백
- 저장 중 신규 변경 상태 보존
- 실제 TCP 연결 기반 Channel 인증·맵 입장·이동 처리
- 부하 종료 후 신규 인증 응답성
- ChannelServer CPU 및 메모리 사용량

테스트 일자는 2026년 8월 6일입니다.

---

## 2. 테스트 환경

### 서버 구성

- Linux
- C++17
- g++ `-O2 -Wall -Wextra -Werror`
- 단일 ChannelServer 테스트
- ChannelServer 포트: `9001`
- TCP 대상: `127.0.0.1`
- MySQL 및 Redis는 별도의 테스트 인스턴스 사용
- 테스트 캐릭터 ID는 MySQL에 미리 등록

### 프로세스 제한

```bash
ulimit -n
```

```text
1048576
```

### Listen 상태

```bash
ss -lntp | grep ':9001'
```

```text
LISTEN 0 4096 0.0.0.0:9001 0.0.0.0:*
```

본 테스트는 localhost 기반 단일 서버 검증이며 네트워크 지연이 포함된 운영 환경의 최대 수용 인원을 의미하지 않습니다.

---

## 3. 자동 회귀·통합 테스트

### 환경변수

Redis와 MySQL 통합 테스트를 실행하려면 테스트용 접속 정보를 설정해야 합니다.

```bash
export TEST_REDIS_HOST="<redis-host>"
export TEST_REDIS_PORT="6379"

export TEST_MYSQL_HOST="<mysql-host>"
export TEST_MYSQL_PORT="3306"
export TEST_MYSQL_USER="<mysql-user>"
export TEST_MYSQL_PASSWORD="<mysql-password>"
export TEST_MYSQL_DATABASE="<test-database>"
```

접속 정보와 비밀번호는 저장소에 커밋하지 않습니다.

`PlayerStateRepositoryTest`는 데이터를 생성하고 트랜잭션 실패를 발생시키므로 운영 DB가 아닌 별도의 테스트 DB에서 실행해야 합니다.

### 실행 명령

```bash
cd SERVER
make test
```

### 테스트 구성

| 테스트 | 검증 내용 |
| --- | --- |
| `PacketParserLengthTest` | 최소·최대·초과 패킷과 부분·병합 수신 |
| `AuthTokenGeneratorTest` | 티켓 길이, 형식 및 10,000개 중복 여부 |
| `AuthChannelTicketReplayTest` | 최초 사용, 재사용, TTL 만료 및 Channel ID 검증 |
| `AuthSessionRegressionTest` | fd 재사용 후 오래된 인증 결과 폐기 |
| `PlayerStateRepositoryTest` | MySQL 트랜잭션 커밋과 롤백 |
| `PlayerSaveFlowTest` | 저장 버전, 실패 상태 유지 및 재시도 |

### 결과

```text
[PASS] 패킷 파서 길이 접두사 테스트 통과
[PASS] 토큰 길이가 64자임
[PASS] 토큰이 소문자 16진수 문자로만 구성됨
[PASS] 중복 없이 토큰 10000개 생성

[PASS] 최초 입장권 사용 성공
[PASS] 일치하는 Channel ID 승인
[PASS] 불일치 Channel ID 거부
[PASS] 재사용된 입장권 거부
[PASS] 만료된 입장권 거부

[PASS] fd 재사용 후 오래된 인증 결과 거부
[PASS] 현재 세션 승인 및 종료 중 세션 거부

[PASS] 플레이어 상태 트랜잭션 커밋 성공
[PASS] 중복 퀵슬롯 트랜잭션 거부
[PASS] 롤백 후 이전 커밋 상태 보존 확인

[PASS] 저장 중 신규 변경 상태 유지
[PASS] 저장 실패 후 대기 상태 유지
[PASS] 재시도 저장 성공
```

모든 자동 테스트가 통과했습니다.

---

## 4. 테스트 티켓 생성

Channel 인증 부하 테스트는 각 클라이언트마다 서로 다른 일회성 티켓이 필요합니다.

### 테스트 도구 빌드

```bash
make -C Test tools
```

### 사용 형식

```text
channel_ticket_test_data_maker \
    <시작 캐릭터 ID> \
    <생성 개수> \
    <Channel ID> \
    <출력 파일>
```

### 300개 티켓 생성

```bash
SERVER/bin/channel_ticket_test_data_maker \
    900000 \
    300 \
    1 \
    /tmp/move_300_tickets.txt
```

```text
[PASS] Channel 입장권 300개 발급 완료
[PASS] 출력 파일: /tmp/move_300_tickets.txt
```

티켓은 일회성이며 TTL이 지나면 사용할 수 없으므로 각 테스트 전에 다시 발급했습니다.

---

## 5. Channel 인증 단일 검증

부하 테스트 전에 클라이언트 한 개로 인증과 응답 파싱을 확인했습니다.

```bash
python3 Test/python/channel_auth_load_test.py \
    --host 127.0.0.1 \
    --port 9001 \
    --tickets-file /tmp/channel_tickets.txt \
    --clients 1 \
    --workers 1 \
    --timeout 5 \
    --verbose
```

결과:

```text
success=1
failed=0
auth_ms_avg=244.320
received_packets_avg=3.000
received_bytes_avg=155.000
```

- Channel 인증 성공
- 캐릭터 이름 파싱 성공
- 인증 이후 서버 패킷 수신 확인

---

## 6. 300 클라이언트 이동 부하 테스트

### 조건

- 클라이언트: 300
- worker: 300
- 테스트 시간: 15초
- 클라이언트당 이동 패킷: 초당 1개
- 동일 Channel 및 동일 맵 사용
- 인증과 맵 입장 후 이동 패킷 전송

### 실행 명령

```bash
python3 Test/python/channel_move_load_test.py \
    --host 127.0.0.1 \
    --port 9001 \
    --tickets-file /tmp/move_300_tickets.txt \
    --clients 300 \
    --workers 300 \
    --timeout 90 \
    --duration 15 \
    --moves-per-sec 1
```

### 결과

| 항목 | 결과 |
| --- | ---: |
| 전체 클라이언트 | 300 |
| 성공 | 300 |
| 실패 | 0 |
| 인증 성공 | 300 |
| 맵 입장 성공 | 300 |
| 전체 경과 시간 | 약 21.440초 |
| 인증 최소 지연 | 약 274 ms |
| 인증 평균 지연 | 약 3,004 ms |
| 인증 p50 지연 | 약 3,059 ms |
| 인증 p95 지연 | 약 5,453 ms |
| 인증 p99 지연 | 약 5,661 ms |
| 인증 최대 지연 | 약 5,806 ms |
| 맵 입장 평균 지연 | 약 54 ms |
| 맵 입장 p95 지연 | 약 134 ms |
| 이동 패킷 전송 | 4,500 |
| 이동 브로드캐스트 수신 | 1,032,019 |
| 전체 수신 패킷 | 1,082,096 |
| 전체 수신 바이트 | 62,991,703 |
| 초당 수신 바이트 | 약 2.94 MB |

서버 비정상 종료와 테스트 클라이언트 실패는 발생하지 않았습니다.

---

## 7. CPU·메모리 측정

동일한 이름의 ChannelServer 프로세스가 여러 개 실행될 수 있으므로 `pgrep -n`이 아닌 포트 `9001`을 실제로 Listen 중인 PID를 선택했습니다.

### Listen PID 확인

```bash
channel_pid="$(
    ss -lntp |
    grep ':9001' |
    sed -n 's/.*pid=\([0-9]\+\).*/\1/p' |
    head -n 1
)"

echo "측정 PID: $channel_pid"
ps -p "$channel_pid" -o pid,stat,lstart,cmd
```

### 측정

`pidstat`은 `sysstat` 패키지에 포함되어 있습니다.

```bash
mkdir -p Test/results

pidstat -u -r \
    -p "$channel_pid" \
    1 30 |
    tee Test/results/channel_300_resource.log
```

### 평균 결과

| 항목 | 결과 |
| --- | ---: |
| ChannelServer 평균 CPU | 약 44.8% |
| ChannelServer 평균 RSS | 약 35.7 MiB |

CPU와 메모리는 테스트 클라이언트가 아니라 포트 `9001`을 Listen 중인 ChannelServer 프로세스를 기준으로 측정했습니다.

---

## 8. 부하 종료 후 신규 인증 검증

이동 부하 테스트가 끝난 직후 별도의 일회성 티켓으로 신규 인증을 수행했습니다.

```bash
python3 Test/python/channel_auth_load_test.py \
    --host 127.0.0.1 \
    --port 9001 \
    --tickets-file /tmp/probe_300_ticket.txt \
    --clients 1 \
    --workers 1 \
    --timeout 90
```

결과:

```text
success=1
failed=0
auth_ms_avg=207.950
received_packets_avg=3.000
received_bytes_avg=132.000
```

부하 종료 후 신규 인증이 약 208 ms에 완료됐습니다.

---

## 9. 종료 저장 작업 풀 분리 전후 비교

종료 저장을 일반 작업 풀에서 처리하던 시점에는 50개 클라이언트가 동시에 연결을 종료한 뒤 신규 인증이 약 14초 지연됐습니다.

원인은 종료 시 발생한 MySQL 저장 작업이 인증 및 게임 패킷과 같은 작업 풀의 worker를 점유했기 때문입니다.

종료 저장을 전용 작업 풀로 분리하고 세션과 독립적인 `PlayerSaveData` 스냅샷을 전달하도록 변경했습니다.

| 조건 | 부하 종료 후 신규 인증 |
| --- | ---: |
| 전용 종료 저장 풀 적용 전 | 약 14,003 ms |
| 전용 종료 저장 풀 적용 후 | 약 133 ms |
| 300 클라이언트 테스트 후 | 약 208 ms |

측정 조건에 따라 값은 달라질 수 있으므로 절대 성능 수치보다 작업 풀 분리 후 인증 지연이 해소된 사실을 확인하는 용도로 사용했습니다.

---

## 10. 플레이어 상태 저장 검증

### 자동 검증

`PlayerStateRepositoryTest`에서 다음 상태를 저장하고 재조회했습니다.

- 캐릭터 맵과 좌표
- 레벨과 캐릭터 스탯
- 인벤토리 메타데이터
- 인벤토리 아이템
- 퀵슬롯

중복된 퀵슬롯 기본키를 이용해 의도적인 저장 실패를 발생시킨 뒤, 트랜잭션 롤백으로 이전 커밋 상태가 유지되는지 확인했습니다.

### 수동 검증

실제 클라이언트에서 다음 절차를 확인했습니다.

1. 게임 접속
2. 플레이어 이동
3. 아이템 사용
4. 퀵슬롯 등록
5. 60초 이상 대기
6. 게임 종료
7. 다시 접속
8. 맵·위치·아이템 수량·퀵슬롯 복원 확인

수동 검증 결과 저장된 맵과 위치로 입장했으며, 인벤토리 수량과 퀵슬롯 정보가 복원됐습니다.

---

## 11. 결과 해석

이번 테스트로 다음 사항을 확인했습니다.

- 300개 클라이언트가 모두 인증과 맵 입장에 성공
- 4,500개 이동 요청 처리
- 약 103만 개 이동 브로드캐스트 수신
- 부하 중 ChannelServer 비정상 종료 없음
- 종료 저장 작업 풀 분리 후 신규 인증 지연 해소
- 패킷·인증·DB 저장 회귀 테스트 통과
- 트랜잭션 실패 시 이전 저장 상태 보존
- 주기 및 종료 저장 후 플레이어 상태 복원

## 12. 한계 및 후속 과제

- localhost 기반 단일 서버 테스트
- 클라이언트와 서버가 동일 장비에서 실행되어 자원 경합 가능
- 같은 맵 전체 브로드캐스트로 접속자 증가 시 전송량이 빠르게 증가
- 300개 클라이언트에서 인증 p95가 약 5.45초로 증가
- 주기 저장은 인증·게임 패킷과 일반 작업 풀을 공유
- 종료 저장 실패가 세 번 반복되면 영속적인 재시도 큐가 없음
- 장시간 운영 환경과 실제 외부 네트워크 테스트는 수행하지 않음

따라서 본 결과는 운영 환경의 최대 동시접속자 수가 아니라 현재 구현의 회귀 방지와 부하 특성 확인을 위한 기준값으로 사용합니다.