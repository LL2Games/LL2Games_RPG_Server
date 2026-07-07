# 채널 서버 성능/수신 경계 테스트 기준 측정

Related issue: #99

## 환경
- Branch: 99-채널-서버-성능-테스트-기준-측정
- Target: ChannelServer
- Host: 127.0.0.1
- Port: 9001

## 테스트 날짜
- 2026-06-30

## 테스트 파일
- `partial_packet_test.py`

## 실행 명령

### 전체 테스트 명령어
```zsh
python3 partial_packet_test.py --host 127.0.0.1 --port 9001
```

### 개별 테스트 명령어
```zsh
python3 partial_packet_test.py --host 127.0.0.1 --port 9001 --case partial_header
python3 partial_packet_test.py --host 127.0.0.1 --port 9001 --case partial_body
python3 partial_packet_test.py --host 127.0.0.1 --port 9001 --case invalid_short_length
python3 partial_packet_test.py --host 127.0.0.1 --port 9001 --case invalid_large_length
python3 partial_packet_test.py --host 127.0.0.1 --port 9001 --case coalesced_packets
```

## 실행 결과

```text
target=127.0.0.1:9001
Run ChannelServer first, then execute this script.
[PASS] partial_header: expected open, actual open
[PASS] partial_body: expected open, actual open
[PASS] invalid_short_length: expected closed, actual closed
[PASS] invalid_large_length: expected closed, actual closed
[PASS] coalesced_packets: expected open, actual open
5/5 passed
```

## 테스트 결과

### partial header
- 결과: PASS
- 관찰: header 일부만 전송한 상태에서 서버가 연결을 유지함
- 의미: 불완전한 TCP 수신에 대해 즉시 disconnect하지 않고 추가 데이터를 기다림

### partial body
- 결과: PASS
- 관찰: header와 body 일부만 전송한 상태에서 서버가 연결을 유지함
- 의미: body가 모두 도착하기 전까지 추가 데이터를 기다림

### invalid short length
- 결과: PASS
- 관찰: 패킷 길이가 header 크기보다 작은 경우 서버가 연결을 종료함
- 의미: 비정상 length 값을 InvalidPacket으로 처리함

### invalid large length
- 결과: PASS
- 관찰: 패킷 길이가 BUFFER_SIZE보다 큰 경우 서버가 연결을 종료함
- 의미: 허용 범위를 초과한 패킷을 방어함

### coalesced packets
- 결과: PASS
- 관찰: 두 개의 패킷을 한 번에 전송했을 때 서버가 연결을 유지함
- 의미: TCP coalescing 상황에서 서버가 즉시 오류 처리하지 않음
- 비고: 현재 테스트는 연결 유지 여부만 확인하며, 각 패킷의 개별 Dispatch 여부는 서버 로그 보강 후 추가 확인 필요

## 테스트 방식
- Python socket 스크립트를 사용해 외부 클라이언트 관점에서 TCP 패킷 경계 상황을 재현
- 서버 코드를 직접 호출하지 않고 실제 TCP 연결을 통해 partial/invalid/coalesced packet을 전송

## 남은 테스트
- coalesced packet 개별 Dispatch 로그 확인
- 동시 접속 기준 측정
- 패킷 처리량 기준 측정
- 평균/최대 응답 시간 측정


<br></br>
# 동시 접속 테스트


## 테스트 파일`  
- channel_load_test.py

## 테스트 목적
- 여러 클라이언트가 동시에 ChannelServer에 접속했을 때 서버가 안정적으로 accept/session 생성/disconnect 처리를 수행하는지 확인
- TCP 경계 테스트 이후 기본 부하 상황에서 서버가 크래시 없이 유지되는지 확인

## 실행 명령

```zsh
python3 channel_load_test.py --host 127.0.0.1 --port 9001 --clients 10
python3 channel_load_test.py --host 127.0.0.1 --port 9001 --clients 50
python3 channel_load_test.py --host 127.0.0.1 --port 9001 --clients 100
python3 channel_load_test.py --host 127.0.0.1 --port 9001 --clients 10000 --timeout 5
python3 channel_load_test.py --host 127.0.0.1 --port 9001 --clients 20000 --timeout 10
python3 channel_load_test.py --host 127.0.0.1 --port 9001 --clients 50000 --timeout 10
```

## 실행 결과

### clients 10

```text
target=127.0.0.1:9001
clients=10, timeout=2.0, hold_time=0.2
success=10
failed=0
elapsed_sec=0.214
connect_ms_min=0.268
connect_ms_avg=0.479
connect_ms_max=0.978
```

### clients 50

```text
target=127.0.0.1:9001
clients=50, timeout=2.0, hold_time=0.2
success=50
failed=0
elapsed_sec=0.217
connect_ms_min=0.172
connect_ms_avg=0.499
connect_ms_max=1.470
```

### clients 100

```text
target=127.0.0.1:9001
clients=100, timeout=2.0, hold_time=0.2
success=100
failed=0
elapsed_sec=0.260
connect_ms_min=0.168
connect_ms_avg=10.021
connect_ms_max=29.110
```

## 테스트 결과

### clients 10
- 결과: PASS
- 관찰: 10개 클라이언트가 모두 접속 성공했고 실패 없음
- 의미: 소규모 동시 접속 상황에서 연결 수락 및 종료 흐름이 정상 동작함

### clients 50
- 결과: PASS
- 관찰: 50개 클라이언트가 모두 접속 성공했고 실패 없음
- 의미: 중간 규모 동시 접속 상황에서도 accept/session 처리 흐름이 유지됨

### clients 100
- 결과: PASS
- 관찰: 100개 클라이언트가 모두 접속 성공했고 실패 없음. 평균 접속 시간은 10.021ms, 최대 접속 시간은 29.110ms로 증가함
- 의미: 100개 동시 접속까지 안정성은 확인했으며, 접속 수 증가에 따른 accept 지연이 baseline 수치로 관찰됨

### clients 10000
- 결과: PASS
- 관찰: 10000개 동시 접속 시도 모두 성공, 실패 없음
- 의미: 로컬 환경 기준 대량 접속 accept/disconnect 흐름이 안정적으로 처리됨

### clients 20000
- 결과: PASS
- 관찰: 20000개 동시 접속 시도 모두 성공, 실패 없음
- 의미: 로컬 환경 기준 20000개 접속까지 실패 없이 처리됨

### clients 50000
- 결과: BLOCKED
- 관찰: 테스트 클라이언트가 Python 스레드를 생성하는 중  RuntimeError: can't start new thread 로 종료됨
- 의미: ChannelServer 한계가 아니라 테스트 클라이언트/OS 스레드 생성 한계에 먼저 도달함

비고: 본 테스트 방식은 clients 수만큼 Python thread를 생성하므로, 50000 단계에서는 서버 부하 한계보다 테스트 클라이언트의 스레드 생성 한계가 먼저 관찰됨

## 테스트 방식
- channel_load_test.py 는  ThreadPoolExecutor 를 사용해 clients 수만큼 동시 접속 작업을 생성
- 각 작업은 하나의 TCP socket을 생성하고 ChannelServer에 접속한 뒤, hold_time 동안 연결을 유지하고 종료
- 따라서 본 테스트는 서버의 패킷 처리 성능이 아니라 대량 TCP 연결 accept/disconnect 안정성을 확인하는 테스트
- 50000 clients 단계에서는 서버 한계보다 Python 테스트 클라이언트의 thread 생성 한계가 먼저 관찰됨

## 남은 테스트
- 패킷 반복 전송 기준 측정
- 처리량 기준 측정
- 평균/최대 응답 시간 측정

<br></br>


# 패킷 반복 전송 테스트

## 테스트 파일
-  channel_packet_burst_test.py 

## 테스트 목적
- 여러 클라이언트가 ChannelServer에 접속한 뒤 짧은 패킷을 반복 전송했을 때 서버가 안정적으로 수신 흐름을 유지하는지 확인
-  recv -> OnBytes -> TryParse  경로에 반복 패킷 부하를 주고 연결 종료/크래시 여부를 확인
- 실제 게임 핸들러 처리량이 아니라 TCP 수신/파싱 경로의 안정성 baseline을 측정

## 실행 명령

```zsh
python3 channel_packet_burst_test.py --host 127.0.0.1 --port 9001 --clients 10 --packets 100
python3 channel_packet_burst_test.py --host 127.0.0.1 --port 9001 --clients 50 --packets 100
python3 channel_packet_burst_test.py --host 127.0.0.1 --port 9001 --clients 500 --packets 1000 --timeout 10
python3 channel_packet_burst_test.py --host 127.0.0.1 --port 9001 --clients 1000 --packets 100 --timeout 10
python3 channel_packet_burst_test.py --host 127.0.0.1 --port 9001 --clients 1000 --packets 500 --timeout 15
python3 channel_packet_burst_test.py --host 127.0.0.1 --port 9001 --clients 1000 --packets 1000 --timeout 30
```

## 실행 결과

### clients 10, packets 100

```text
target=127.0.0.1:9001
clients=10, packets_per_client=100, total_expected=1000, timeout=2.0, delay=0.0
success_clients=10
failed_clients=0
total_sent=1000
elapsed_sec=0.071
packets_per_sec=14026.072
connect_ms_avg=1.678
connect_ms_max=2.751
send_ms_avg=65.028
send_ms_max=68.484
```

### clients 50, packets 100

```text
target=127.0.0.1:9001
clients=50, packets_per_client=100, total_expected=5000, timeout=2.0, delay=0.0
success_clients=50
failed_clients=0
total_sent=5000
elapsed_sec=0.347
packets_per_sec=14390.211
connect_ms_avg=5.355
connect_ms_max=15.628
send_ms_avg=281.236
send_ms_max=311.365
```

### clients 500, packets 1000

```text
target=127.0.0.1:9001
clients=500, packets_per_client=1000, total_expected=500000, timeout=10.0, delay=0.0
success_clients=500
failed_clients=0
total_sent=500000
elapsed_sec=25.637
packets_per_sec=19503.141
connect_ms_avg=34.347
connect_ms_max=148.269
send_ms_avg=21570.690
send_ms_max=23468.261
```

### clients 1000, packets 100

```text
target=127.0.0.1:9001
clients=1000, packets_per_client=100, total_expected=100000, timeout=10.0, delay=0.0
success_clients=1000
failed_clients=0
total_sent=100000
elapsed_sec=6.183
packets_per_sec=16174.264
connect_ms_avg=34.008
connect_ms_max=216.466
send_ms_avg=1448.410
send_ms_max=2263.050
```

### clients 1000, packets 500

```text
target=127.0.0.1:9001
clients=1000, packets_per_client=500, total_expected=500000, timeout=15.0, delay=0.0
success_clients=1000
failed_clients=0
total_sent=500000
elapsed_sec=25.000
packets_per_sec=19999.856
connect_ms_avg=51.927
connect_ms_max=261.606
send_ms_avg=12841.233
send_ms_max=16426.109
```

### clients 1000, packets 1000

```text
target=127.0.0.1:9001
clients=1000, packets_per_client=1000, total_expected=1000000, timeout=30.0, delay=0.0
success_clients=1000
failed_clients=0
total_sent=1000000
elapsed_sec=46.778
packets_per_sec=21377.561
connect_ms_avg=53.508
connect_ms_max=343.508
send_ms_avg=32658.624
send_ms_max=37331.865
```

## 테스트 결과

### clients 10, packets 100
- 결과: PASS
- 관찰: 10개 클라이언트가 총 1000개 패킷을 실패 없이 전송함
- 의미: 소규모 반복 패킷 전송 상황에서 수신/파싱 경로가 안정적으로 유지됨

### clients 50, packets 100
- 결과: PASS
- 관찰: 50개 클라이언트가 총 5000개 패킷을 실패 없이 전송함
- 의미: 클라이언트 수가 증가해도 반복 패킷 전송 중 연결 실패가 발생하지 않음

### clients 500, packets 1000
- 결과: PASS
- 관찰: 500개 클라이언트가 각 1000개 패킷을 전송해 총 500000개 패킷을 실패 없이 전송함
- 의미: 다수 클라이언트의 반복 패킷 전송 상황에서도 TCP 수신/파싱 경로가 안정적으로 유지됨

### clients 1000, packets 100
- 결과: PASS
- 관찰: 1000개 클라이언트가 각 100개 패킷을 전송해 총 100000개 패킷을 실패 없이 전송함
- 의미: 1000개 동시 클라이언트 환경에서도 반복 패킷 전송 중 연결 실패가 발생하지 않음

### clients 1000, packets 500
- 결과: PASS
- 관찰: 1000개 클라이언트가 각 500개 패킷을 전송해 총 500000개 패킷을 실패 없이 전송함
- 의미: 동일한 총 500000개 패킷 기준에서 500 clients/1000 packets와 1000 clients/500 packets 모두 실패 없이 완료됨

### clients 1000, packets 1000
- 결과: PASS
- 관찰: 1000개 클라이언트가 각 1000개 패킷을 전송해 총 1000000개 패킷을 실패 없이 전송함
- 의미: 1000개 동시 클라이언트의 반복 패킷 전송 상황에서 연결 실패 없이 수신/파싱 경로가 유지됨

## 테스트 방식
-  channel_packet_burst_test.py 는 clients 수만큼 Python thread를 생성하고, 각 thread가 하나의 TCP socket을 통해 반복 패킷을 전송
- 각 패킷은 서버의  PacketHeader(length, type)  구조에 맞춰 구성
- 테스트 패킷 타입은 unknown packet을 사용하므로 실제 게임 핸들러 처리량을 측정하지 않음
- 본 테스트는 클라이언트 기준 send 성공 및 연결 안정성을 측정

## 패킷 반복 전송 테스트 결론
- 로컬 환경 기준 1000개 클라이언트가 총 1000000개 패킷을 실패 없이 전송함
- 클라이언트 기준 약 21k packets/sec 수준의 반복 전송이 관찰됨
- TCP 수신/파싱 경로는 반복 패킷 부하 상황에서도 안정적으로 유지됨
- 실제 게임 로직 처리량, handler 처리 완료율, 응답 latency는 별도 테스트가 필요함

## 남은 테스트
- 실제 패킷 타입 기반 handler 부하 테스트
- coalesced packet 개별 Dispatch 로그 확인
- 서버 내부 수신/Dispatch 카운터 로그 보강
- CPU/메모리/FD 사용량 측정
- 장시간 soak test

<br></br>

# 채널 인증 패킷 테스트

## 테스트 파일
- channel_auth_test.py

## 테스트 목적
- 실제 PKT_CHANNEL_AUTH 패킷을 ChannelServer에 전송해 인증 핸들러 흐름이 정상 동작하는지 확인
- DB에서 캐릭터/스탯/스킬/인벤토리/퀵슬롯 초기 데이터를 로드하고 인증 응답을 반환하는지 확인

## 테스트 데이터
- account_id: test_account_001
- character_id: 900000
- character_name: PerfTest0000

## 실행 명령

```zsh
python3 channel_auth_test.py --host 127.0.0.1 --port 9001 --character-id 900000
```

## 실행 결과

```text
target=127.0.0.1:9001
character_id=900000
received_bytes=2744
received_packets=7
elapsed_sec=2.172
packet[1] type=0x0024 field_count=7 body_bytes=51
packet[2] type=0x0025 field_count=12 body_bytes=43
packet[3] type=0x0026 field_count=3 body_bytes=10
packet[4] type=0x0080 field_count=16 body_bytes=53
packet[5] type=0x0081 field_count=641 body_bytes=2035
packet[6] type=0x1002 field_count=161 body_bytes=506
packet[7] type=0x0009 fields=['ok', 'PerfTest0000']
[PASS] channel_auth: received ok
```

## 테스트 결과

### channel auth
- 결과: PASS
- 관찰: PKT_CHANNEL_AUTH 요청 후 총 7개 응답 패킷을 수신했고, 마지막 인증 응답에서 ok와 캐릭터명 PerfTest0000을 확인함
- 의미: ChannelServer가 실제 인증 패킷을 처리하고 DB 기반 캐릭터 초기 데이터를 로드한 뒤 정상 인증 응답을 반환함

## 테스트 방식
- Python socket 스크립트를 사용해 실제 PKT_CHANNEL_AUTH 패킷을 생성해 전송
- 응답 패킷을 파싱해 인증 응답 타입 0x0009와 ok 결과를 확인
- 단순 연결 유지가 아니라 실제 ChannelServer 인증 핸들러 경로를 검증

## 비고

- 응답 패킷에는 캐릭터 정보, 스탯, 스킬, 인벤토리 메타, 인벤토리 슬롯, 퀵슬롯 정보가 포함됨
- 단일 캐릭터 인증 성공을 확인한 뒤, 다음 단계로 서로 다른 character_id를 사용한 다중 인증 부하 테스트를 진행함


# 채널 인증 과부화 테스트

## 테스트 파일 
- channel_auth_load_test_auth.py

## 테스트 목적
- 여러 클라이언트가 동시에 실제 PKT_CHANNEL_AUTH 패킷을 전송했을 때 ChannelServer가 인증 요청을 안정적으로 처리하는지 확인
- DB 기반 캐릭터 초기 데이터 로드, Player 세션 등록, 초키 패킷 전송, 인증 응답 반환까지 포함된 실제 핸들러 경로를 측정

## 테스트 데이터
- account_id = test_account_001
- character_id range = 900000 ~ 900099
- character_name pattern : PerfTest0000 ~ PerfTest0099

## 실행 명령

```zsh
python3 channel_auth_load_test.py --host 127.0.0.1 --port 9001 --start-character-id 900000 --clients 10
python3 channel_auth_load_test.py --host 127.0.0.1 --port 9001 --start-character-id 900000 --clients 30 --timeout 5
python3 channel_auth_load_test.py --host 127.0.0.1 --port 9001 --start-character-id 900000 --clients 32 --timeout 5
python3 channel_auth_load_test.py --host 127.0.0.1 --port 9001 --start-character-id 900000 --clients 33 --timeout 5
python3 channel_auth_load_test.py --host 127.0.0.1 --port 9001 --start-character-id 900000 --clients 100 --timeout 5  
```

## 실행 결과

### client 10

```text
target=127.0.0.1:9001
start_character_id=900000, clients=10, timeout=3.0
success=10
failed=0
elapsed_sec=4.556
auth_ms_min=3163.172
auth_ms_avg=3868.912
auth_ms_max=4550.601
received_packets_avg=7.000
received_packets_max=7
received_bytes_avg=2744.000
received_bytes_max=2744
```

### client 30

``` text
target=127.0.0.1:9001
start_character_id=900000, clients=30, timeout=5.0
success=30
failed=0
elapsed_sec=4.503
auth_ms_min=144.610
auth_ms_avg=2304.002
auth_ms_max=4491.252
received_packets_avg=7.000
received_packets_max=7
received_bytes_avg=2744.000
received_bytes_max=2744
```

### client 100

```text
target=127.0.0.1:9001
start_character_id=900000, clients=100, timeout=5.0
success=32
failed=68
elapsed_sec=9.897
auth_ms_min=5001.188
auth_ms_avg=5819.200
auth_ms_max=9878.425
received_packets_avg=2.240
received_packets_max=7
received_bytes_avg=878.080
received_bytes_max=2744
```

### client 32

```text
success=32
failed=0
```

### client 33

```text
success=32
failed=1
```

## 테스트 결과


### client10

- 결과 : PASS
- 관찰 : 10개 클라이언트가 서로 다른 캐릭터 ID로 PKT_CHANNEL_AUTH 요청을 보냈고 모두 ok 응답을 수신함
- 의미 : 소규모 동시 인증 상황에서 DB기반 캐릭터 초기 데이터 로드 및 인증 응답 반환 흐름이 정상 동작함
- 비고 : 최초 측정 당시에는 응답 수신 후 socket timeout 대기 시간이 포함되어 있어 auth_ms가 실제 인증 완료 시간보다 크게 측정됨

### client30

- 결과: PASS
- 관찰: 30개 클라이언트가 모두 PKT_CHANNEL_AUTH 요청 후 ok 응답을 수신함
- 의미: 로컬 환경 기준 30명 동시 채널 인증까지 DB 기반 초기 데이터 로드 및 인증 응답 반환 흐름이 정상 처리됨
- 비고: 수정된 테스트는 PKT_CHANNEL_AUTH의 ok 응답을 수신하는 즉시 측정을 종료하므로, auth_ms는 클라이언트 기준 인증 완료 시간으로 해석할 수 있음

### clients100
- 결과: FAIL
- 관찰: 100개 동시 인증 요청 중 32개만 성공하고 68개는 no_auth_response로 실패함
- 의미: 인증 처리/DB 조회/초기 데이터 송신 경로에서 동시 처리 한계가 32명 근처에 존재하는 것으로 관찰됨

### clients32
- 결과: PASS
- 관찰: 32개 클라이언트가 모두 인증 성공함
- 의미: 현재 로컬 환경과 timeout 5초 기준에서 32명 동시 인증까지 안정적으로 처리됨

### clients 33
- 결과: FAIL
- 관찰: 33개 동시 인증 요청 중 32개는 성공했으나 1개 요청이 timeout 내 인증 응답을 수신하지 못함
- 의미: 32명을 초과하는 동시 인증 상황에서 인증 처리 지연 또는 병목이 발생함


## 테스트 방식

- 각 클라이언트는 서로 다른 character_id로 TCP 연결을 생성하고 PKT_CHANNEL_AUTH 패킷을 전송함
- 서버 응답 중 PKT_CHANNEL_AUTH의 ok 또는 nok를 수신하는 즉시 해당 클라이언트의 측정을 종료함
- 따라서 수정 후 auth_ms는 클라이언트 기준으로 인증 요청 전송부터 인증 응답 수신까지의 시간임
- 같은 character_id를 동시에 중복 사용하지 않도록 900000부터 순차적으로 캐릭터 ID를 배정함

## 결론 

- 단일 인증 및 10명/30명/32명 동시 인증은 정상 처리됨
- 33명 이상부터 timeout 내 인증 응답을 받지 못하는 요청이 발생함
- 현재 로컬 테스트 환경 기준 ChannelServer의 PKT_CHANNEL_AUTH 안정 처리 기준은 32명 동시 인증으로 기록함
- 향후 DB connection pool, 인증 처리 worker/thread 구조, 초기 데이터 송신량, PlayerManager 등록 과정의 lock 경합 여부를 추가 확인할 필요가 있음

<br></br>

# 채널 인증 ThreadPool 혼용 실험

## 실험 목적
- `PKT_CHANNEL_AUTH` 처리를 epoll 메인 루프에서 분리하기 위해 기존 ChannelServer ThreadPool에 인증 작업을 위임했을 때 성능이 개선되는지 확인

## 실험 방식
- `PacketTask`를 추가해 `PacketContext`, payload, packet handler를 task 내부에서 소유하도록 구성
- `ChannelSession::Dispatch()`에서 `PKT_CHANNEL_AUTH` 패킷만 기존 `ChannelServer::GetThreadPool()`에 Submit
- 기존 ThreadPool은 `MapManager`의 `MapUpdateTask` 처리에도 사용 중인 상태

## 실행 결과

### clients 33

```text
target=127.0.0.1:9001
start_character_id=900000, clients=33, timeout=5.0
success=1
failed=32
elapsed_sec=5.020
auth_ms_min=144.820
auth_ms_avg=4858.954
auth_ms_max=5015.764
received_packets_avg=0.212
received_packets_max=7
received_bytes_avg=83.152
received_bytes_max=2744
```

## 결과
- 결과: FAIL
- 관찰: 33개 동시 인증 요청 중 1개만 성공하고 32개는 timeout 내 인증 응답을 받지 못함
- 의미: 기존 ThreadPool에 인증 작업을 단순히 추가하는 방식은 성능을 개선하지 못했고, 오히려 인증 처리 성공률이 크게 낮아짐

## 원인 후보
- 기존 ThreadPool은 MapManager의 MapUpdateTask 처리에도 사용 중이므로 인증 작업과 맵 업데이트 작업이 같은 worker queue를 공유함
- PKT_CHANNEL_AUTH는 DB 조회와 초기 데이터 송신을 포함하는 무거운 작업이라 공유 pool에서 대기 시간이 증가할 수 있음
- PacketTask가 ChannelSession*를 보유하므로 클라이언트 timeout/disconnect 이후 세션 생명주기 문제가 발생할 수 있음
- 인증 작업은 일반 맵 업데이트 작업과 성격이 다르므로 같은 pool에 섞기보다 별도 처리 구조가 필요함

## 결론
- 기존 ChannelServer ThreadPool을 인증 처리에 그대로 재사용하는 방식은 부적합한 것으로 판단함
- 인증 처리는 별도 auth 전용 ThreadPool 또는 인증 전용 queue/worker 구조로 분리하는 방향이 적절함
- 최종 적용 전에는 ChannelSession 생명주기 보호, Send thread safety, 동일 세션 패킷 순서 보장 방안을 함께 설계해야 함

## 후속작업
- 실패 실험 코드는 제거하고 Redis 캐시 개선 상태만 유지
- auth 전용 worker 구조 설계
- 세션 생명주기 보호 방식 검토
- auth 전용 worker 적용 후 33/50/100명 인증 부하 재측정

<br></br>

# 채널 인증 비동기 처리 개선 재측정


## 개선 목적
- 기존 `PKT_CHANNEL_AUTH` 처리에서 DB/Redis 기반 Player 로딩과 초기 인증 처리가 epoll 이벤트 흐름에 직접 영향을 주는 문제를 완화
- auth worker thread에서 `ChannelSession*`를 직접 사용해 응답을 전송하던 구조의 session lifetime 위험 제거
- 인증 결과를 `AuthResult queue`로 전달하고, `ChannelServer` event loop에서 fd 생존 여부를 확인한 뒤 응답을 전송하도록 구조 변경

## 개선 내용
- `ChannelAuthTask`를 auth 전용 ThreadPool에서 실행하도록 분리
- worker thread는 characterId 파싱 및 `LoadPlayer` 수행 후 `ChannelAuthResult`를 queue에 push
- `ChannelServer` event loop에서 `ProcessAuthResults()`를 통해 인증 결과 처리
- `ProcessAuthResults()`에서 fd가 아직 살아있는 경우에만 PlayerManager 등록 및 응답 전송 수행
- `Player`에 `ChannelSession`을 설정한 뒤 PlayerPacketSender/QuickSlotPacketSender를 호출하도록 수정
- `AddPlayer()` 실패 시 raw pointer를 계속 사용하지 않고 `already connected` 응답 후 처리 중단
- `LoadPlayer` 구간에 mutex를 적용해 PlayerService/Redis/MySQL 로딩 경로의 공유 자원 경합을 방지

## 실행 조건
- Branch: `100-refactor-채널인증처리-기능-개선`
- Target: ChannelServer
- Host: 127.0.0.1
- Port: 9001
- AUTH_THREAD_POOL_COUNT: 4
- 테스트 데이터: character_id `900000 ~ 900099`

## 실행 명령

```zsh
python3 channel_auth_test.py --host 127.0.0.1 --port 9001 --character-id 900000
python3 channel_auth_load_test.py --host 127.0.0.1 --port 9001 --start-character-id 900000 --clients 10 --timeout 5
python3 channel_auth_load_test.py --host 127.0.0.1 --port 9001 --start-character-id 900000 --clients 30 --timeout 5
python3 channel_auth_load_test.py --host 127.0.0.1 --port 9001 --start-character-id 900000 --clients 50 --timeout 10
python3 channel_auth_load_test.py --host 127.0.0.1 --port 9001 --start-character-id 900000 --clients 100 --timeout 30
python3 channel_auth_load_test.py --host 127.0.0.1 --port 9001 --start-character-id 900000 --clients 5000 --workers 500 --ramp-seconds 10 --timeout 60
```

## 실행 결과
### 단일 인증
```text
target=127.0.0.1:9001
character_id=900000
received_bytes=22
received_packets=1
elapsed_sec=2.098
packet[1] type=0x0009 fields=['ok', 'PerfTest0000']
[PASS] channel_auth: received ok
```

### clients 10
```text
success=10
failed=0
```

### clients 30
```text
success=30
failed=0
```

### clients 50
```text
결과: PASS
조건: timeout 증가 후 성공
```

### clients 100
```text
결과: PASS
조건: timeout 30초 기준 성공
``` 

### clients 5000, workers 500, ramp 10s
```text
target=127.0.0.1:9001
start_character_id=900000, clients=5000, workers=500, timeout=60.0, hold_seconds=0.0, ramp_seconds=10.0
success=5000
failed=0
elapsed_sec=133.271
auth_ms_min=316.794
auth_ms_avg=12547.786
auth_ms_p50=13023.971
auth_ms_p95=15384.294
auth_ms_p99=15982.493
auth_ms_max=16413.311
received_packets_avg=3.450
received_packets_max=7
received_bytes_avg=620.332
received_bytes_max=2744
```

## 테스트 결과
### 단일 인증
- 결과: PASS
- 관찰: PKT_CHANNEL_AUTH 요청 후 ok, PerfTest0000 응답을 수신함
- 의미: AuthResult queue 기반 인증 응답 경로가 정상 동작함

### clients 10
- 결과: PASS
- 관찰: 10개 동시 인증 요청이 모두 성공함
- 의미: auth worker 분리 후 소규모 동시 인증 요청을 안정적으로 처리함

### clients 30
- 결과: PASS
- 관찰: 30개 동시 인증 요청이 모두 성공함
- 의미: 기존 32명 근처에서 관찰되던 인증 병목이 개선됨

### clients 50
- 결과: PASS
- 관찰: 서버 재시작 후 단독 실행 및 timeout 증가 조건에서 50개 인증 요청이 성공함
- 의미: 50명 자체가 서버 처리 한계는 아니며, 짧은 timeout 또는 이전 테스트의 세션 정리 상태가 결과에 영향을 줄 수 있음

### clients 100
- 결과: PASS
- 관찰: timeout 30초 기준으로 100개 인증 요청이 성공함
- 의미: 서버가 100개 인증 요청을 처리할 수 있으나, LoadPlayer 구간 mutex 직렬화로 인해 요청 수가 증가할수록 대기 시간이 증가함

### clients 5000, workers 500, ramp 10s
- 결과: PASS
- 관찰: 총 5000건의 PKT_CHANNEL_AUTH 요청이 모두 성공했고 실패 없음
- 관찰: 테스트 클라이언트는 최대 500 worker로 인증 요청을 병렬 실행했으며, 5000건 요청을 10초 동안 점진적으로 투입함
- 관찰: 클라이언트 기준 인증 응답 시간은 avg 12547.786ms, p95 15384.294ms, p99 15982.493ms로 측정됨
- 의미: ChannelServer는 로컬 환경 기준 5000건의 채널 인증 부하를 실패 없이 처리했으나, 본 결과는 5000명 연결 유지 테스트가 아니라 500 worker 병렬 인증 요청 처리 결과임
- 의미: 인증 성공률은 개선되었지만 p95/p99 지연 시간이 높아, 인증 latency 개선은 별도 최적화 과제로 남음

## 개선 전/후 비교
### 개선 전
- 단일 인증은 PASS
- clients 30까지는 PASS
- clients 32는 PASS
- clients 33부터 timeout 발생
- clients 100은 success=32, failed=68
- 기존 ThreadPool에 인증 작업을 넣는 실험은 clients 33 기준 success=1, failed=32

### 개선 후
- auth 전용 ThreadPool + AuthResult queue 구조 적용
- worker thread에서 ChannelSession* 직접 접근 제거
- AUTH_THREAD_POOL_COUNT=4 기준 clients 30/50/100 인증 처리 성공 확인
- clients 100은 timeout 30초 기준 성공
- PKT_CHANNEL_AUTH ok 응답을 초기 데이터 패킷보다 먼저 전송하도록 조정
- clients 5000, workers 500, ramp 10s, timeout 60초 조건에서 success=5000, failed=0 확인

### 현재 한계
- LoadPlayer 구간은 mutex로 보호되어 있어 실제 DB/Redis 로딩은 직렬 처리됨
- 따라서 동시 인증 수가 증가하면 서버가 crash하지는 않지만 뒤쪽 요청의 대기 시간이 증가함
- 현재 테스트는 PKT_CHANNEL_AUTH ok 응답 수신 기준이며, 전체 초기화 패킷 수신 검증은 별도 확인 필요
- 동일 character_id 범위를 서버 재시작 없이 반복 테스트하면 이전 세션 정리 상태가 결과에 영향을 줄 수 있음
- clients 5000 결과는 총 인증 요청 5000건 처리 결과이며, 5000개 연결을 일정 시간 유지하는 동시접속 검증은 별도 hold_seconds 조건으로 측정 필요

### 남은 과제
- PlayerService/Redis/MySQL 로딩 경로를 worker별 독립 connection 또는 thread-safe 구조로 개선
- LoadPlayer mutex 제거 후 AUTH_THREAD_POOL_COUNT 증가 효과 재측정
- 전체 초기화 패킷(PlayerInfo/Stat/Skill/QuickSlot) 수신 검증
- 연속 재접속 시 동일 character_id 정리 완료 전 재접속 안정성 검증
- AuthResult queue 처리 시 eventfd 등을 사용해 epoll wait timeout 의존 제거 검토

<br></br>

# 채널 이동 패킷 부하 테스트  - 개선 전 구조 기록

## 테스트 목적
- 일반 패킷 handler가 ChannelSession::Dispatch()에서 직접 실행되던 기존 구조에서 이동 패킷 처리 성능과 안정성을 측정
- PKT_CHANNEL_AUTH, PKT_ENTER_MAP, PKT_PLAYER_MOVE를 포함한 실제 게임 패킷 경로를 검증
- 이후 PacketProcessTask 기반 멀티스레드 처리 구조와 비교하기 위한 기준값 확보

## 기존 구조
- OnReceive()에서 수신한 데이터를 ChannelSession::OnBytes()로 전달
- OnBytes() 에서 패킷 파싱 후 Dispatch() 호출
- Dispatch() 에서  ChannelPacketFactory로 handler 생성
- handler가 epoll 이벤트 처리 흐름에서 직접 실행됨

## 실행 조건
- Branch: 100-refactor-채널인증처리-기능-개선
- Target: ChannelServer
- Host:  127.0.0.1
- Port: 9001
- 테스트 파일:  channel_move_load_test.py
- 테스트 데이터: character_id  900000 ~ 900199
- map_id: 100000000

## 실행 명령

```zsh
python3 Test/channel_move_load_test.py --port 9001 --clients 200 --duration 30 --moves-per-sec 5
```

## 실행 결과
```text
개선 전 동일 조건 정량 결과는 별도 보관하지 못함.
본 섹션은 개선 전 구조와 측정 필요성을 기록하고,
개선 후 결과를 기준 측정값으로 남긴다.

-  일반 패킷 handler가 epoll 이벤트 처리 흐름에서 직접 실행되던 상태에서 특정 thread CPU 사용률이 약 95%로 관찰됨
- htop을 통해서 확인
```

## 테스트 결과

### clients 200, duration 30, moves_per_sec 5

- 결과 : PASS 또는 FAIL
- 관찰 : 기존 구조의 처리량 수치 비교는 수행하지 못했으며, 개선 후 측정값을 기준값으로 기록함
- 의미 : 기존 구조에서 실제 이동 패킷 처리 및 브로드캐스트 fan-out 기준 성능을 확인함

## 관찰 사항
- 일반 패킷 handler가 epoll 이벤트 처리 흐름에서 직접 실행되므로, 게임 로직 처리 비용이 증가하면 네트워크 이벤트 처리 지연으로 이어질 수 있음

- 동일 맵 전체 브로드캐스트 구조에서는 클라이언트 수 증가에 따라 send queue enqueue 및 EPOLLOUT 처리 비용이 크게 증가함


<br></br>

# 채널 이동 패킷 멀티스레드 처리 부하 테스트 - 개선 후

## 개선 목적
- 일반 패킷 handler를 epoll 이벤트 처리 흐름에서 분리해 네트위크 I/O와 게임 로직 처리의 결합도를 낮춤
- PKT_PLAYER_MOVE 같은 게임 로직 패킷을 ThreadPool worker에서 처리하도록 변경
- worker thread에서 패킷이 늦게 실행될 때 이미 종료된 세션을 사용할 수 있는 위험을 줄이기 위해 세션 유효성 검증을 추가함


## 개선 내용
- PacketProcessTask 추가
- ChannelSession::Dispatch()에서 PKT_CHANNEL_AUTH를 제외한 일반 패킷을 ThreadPool에 submit하도록 변경
- PacketProcessTask::Excute()에서 ChannelPacketFactory로 handler 생성 후 PacketContext를 구성해 handler 실행
- ChannelSession에 sessionId, geneeration 추가
- ChannelServer::FindValidSession()에서 fd, sessinId, generation이 모두 일치하는 경우에만 handler 실행
- m_sessions 접근을 m_sessionMutex로 보호
- ChannelSession의 send queue는 m_sendMutex로 보호
- disconnect 중인 세션에 worker task가 새로 진입하지 못하도록 ChannelSession closing 상태 추가
- PacketProcessTask 실행 중 세션이 삭제되지 않도록 in-flight task count와 WaitForNoTasks() 추가
- OnDisconnect()에서 세션을 m_sessions에서 먼저 제거하고 closing 처리한 뒤, 진행 중인 task 종료를 기다린 후 delete 수행
- PlayerPacketSender::SendPlayersMove()에서 nullptr Player/Session을 건너뛰도록 방어 로직 추가
- channel_move_load_test.py에 실패 원인 집계(failure_by_error) 출력 추가

## 실행 조건
- Branch: 100-refactor-채널인증처리-기능-개선
- Target: ChannelServer
- Host: 127.0.0.1
- Port: 9001
- 테스트 파일: channel_move_load_test.py
- 테스트 데이터: character_id 900000 ~ 900199
- map_id: 100000000

## 실행 명령

```zsh
python3 Test/channel_move_load_test.py --port 9001 --clients 200 --duration 30 --moves-per-sec 5
```

## 실행 결과

```text
target=127.0.0.1:9001
start_character_id=900000, clients=200, map_id=100000000, duration=30.0, moves_per_sec=5.0, timeout=10.0
success=200
failed=0
auth_success=200
enter_success=200
elapsed_sec=30.968
auth_ms_min=31.549
auth_ms_avg=363.700
auth_ms_p50=376.601
auth_ms_p95=679.382
auth_ms_p99=698.658
auth_ms_max=712.389
enter_ms_min=0.813
enter_ms_avg=31.168
enter_ms_p50=18.185
enter_ms_p95=108.513
enter_ms_p99=121.758
enter_ms_max=140.845
move_sent_total=30000
move_sent_per_sec=968.733
move_broadcast_recv_total=5924068
move_broadcast_recv_per_sec=191294.752
packets_recv_total=5929502
bytes_recv_total=294415890
bytes_recv_per_sec=9507016.932
```

### clients 300, duration 30, moves_per_sec 5

```zsh
python3 channel_move_load_test.py --port 9001 --clients 300 --duration 30 --moves-per-sec 5 --timeout 30
```

```text
target=127.0.0.1:9001
start_character_id=900000, clients=300, map_id=100000000, duration=30.0, moves_per_sec=5.0, timeout=30.0
success=300
failed=0
auth_success=300
enter_success=300
elapsed_sec=38.910
auth_ms_min=215.972
auth_ms_avg=4046.794
auth_ms_p50=3976.854
auth_ms_p95=7563.454
auth_ms_p99=7804.042
auth_ms_max=7976.160
enter_ms_min=0.436
enter_ms_avg=307.529
enter_ms_p50=77.162
enter_ms_p95=852.822
enter_ms_p99=891.234
enter_ms_max=929.340
move_sent_total=44998
move_sent_per_sec=1156.467
move_broadcast_recv_total=10614425
move_broadcast_recv_per_sec=272795.039
packets_recv_total=10628376
bytes_recv_total=623939921
bytes_recv_per_sec=16035509.732
```

## 테스트 결과
### clients 200, duration 30, moves_per_sec 5
- 결과 : PASS
- 관찰: 200개 클라이언트가 모두 인증 및 맵 입장에 성공함
- 관찰: 30초 동안 총 30000개의 PKT_PLAYER_MOVE 요청을 전송했고 실패 없이 완료됨
- 관찰: 클라이언트 기준 이동 입력 처리량은 초당 약 968.733개로 측정됨
- 관찰: 이동 브로드캐스트 수신은 총 5924068개, 초당 약 191294.752개로 측정됨
- 의미: 일반 패킷 handler를 PacketProcessTask로 분리한 뒤에도 인증, 맵 입장, 이동 처리, 이동 브로드캐스트 흐름이 안정적으로 유지됨 

### clients 300, duration 30, moves_per_sec 5
- 결과 : PASS
- 관찰: 300개 클라이언트가 모두 인증 및 맵 입장에 성공했고 실패 없음
- 관찰: 30초 동안 총 44998개의 PKT_PLAYER_MOVE 요청을 전송함
- 관찰: 클라이언트 기준 이동 입력 처리량은 초당 약 1156.467개로 측정됨
- 관찰: 이동 브로드캐스트 수신은 총 10614425개, 초당 약 272795.039개로 측정됨
- 관찰: 인증 응답 시간은 avg 4046.794ms, p95 7563.454ms, p99 7804.042ms로 측정됨
- 의미: 세션 closing/in-flight task 완화 후 300개 클라이언트 이동 부하에서 crash/restart 없이 인증, 맵 입장, 이동 브로드캐스트 흐름이 유지됨
- 의미: 인증 latency는 여전히 높아 별도 최적화 대상으로 남지만, 이동 브로드캐스트 안정성은 300 clients 조건에서 확인됨

## 테스트 방식

- 각 클라이언트는 TCP 연결 후 PKT_CHANNEL_AUTH로 인증을 수행함
- 인증 성공 후 PKT_ENTER_MAP으로 동일 map_id 100000000에 입장함
- 입장 성공 후 30초 동안 초당 5개의 PKT_PLAYER_MOVE 패킷을 전송함
- 각 클라이언트는 서버로부터 수신한 PKT_PLAYER_MOVE 브로드캐스트 개수를 집계함
- 테스트는 실제 socket 통신을 사용하며 서버 코드를 직접 호출하지 않음

## 관찰 사항

- htop에서 channelD의 여러 thread가 표시되어 ThreadPool worker가 동작 중임을 확인함
- 특정 thread의 CPU 사용률이 높게 관찰되었으며, 원인 후보는 이동 브로드캐스트 fan-out, send queue enqueue, EPOLLOUT/send 처리 비용으로 판단됨
- 200명이 같은 맵에 있을 때 이동 패킷 1개가 다수 클라이언트에게 브로드캐스트되므로 입력 패킷 수보다 송신 패킷 수가 크게 증가함
- 30000개의 이동 입력에 대해 약 592만개의 이동 브로드캐스트 수신이 관찰되어, 동일 맵 전체 브로드캐스트 비용이 주요 부하로 확인됨
- 300명 조건에서는 약 44998개의 이동 입력에 대해 약 1061만개의 이동 브로드캐스트 수신이 관찰됨
- worker task가 실행되는 동안 disconnect가 발생하면 ChannelSession이 먼저 삭제될 수 있는 위험이 있어, 세션 closing 상태와 in-flight task count로 완화함
- 다만 MapInstance/PlayerManager 경로에는 여전히 raw Player* 기반 snapshot/broadcast 구조가 남아 있어, 장기적으로는 shared_ptr/weak_ptr 또는 세션 식별자 기반 검증 송신 구조가 필요함

## 개선 전/후 비교

| 항목 | 개선 전 | 개선 후 |
|---|---:|---:|
| 일반 패킷 처리 위치 | epoll 흐름의 Dispatch() 직접 실행 | PacketProcessTask 기반 worker 실행 |
| 세션 유효성 검증 | 없음 | fd/sessionId/generation/closing 검증 |
| 세션 task 생명주기 완화 | 없음 | in-flight task count 적용 |
| m_sessions 동기화 | 일부 미보호 | m_sessionMutex 적용 |
| 동일 조건 정량 결과 | 미보관 | 측정 완료 |
| clients | 미측정 | 200 / 300 |
| duration | 미측정 | 30s |
| moves_per_sec/client | 미측정 | 5 |
| success | 미측정 | 200 / 300 |
| failed | 미측정 | 0 |
| move_sent_total | 미측정 | 30000 / 44998 |
| move_broadcast_recv_total | 미측정 | 5924068 / 10614425 |
| move_broadcast_recv_per_sec | 미측정 | 191294.752 / 272795.039 |


## 현재 한계

- 개선 전 동일 조건 정량 결과를 보관하지 못해 처리량 수치의 직접 비교는 수행하지 못함
- FindValidSession()/BeginValidSessionTask()는 fd/sessionId/generation/closing 검증으로 잘못된 세션 처리를 줄임
- in-flight task count로 PacketProcessTask 실행 중 ChannelSession delete 위험을 완화했지만, 전체 객체 생명주기를 shared ownership으로 바꾼 것은 아님
- MapInstance/PlayerManager의 raw Player* snapshot 구조에서는 disconnect/remove와 broadcast가 겹칠 때 추가 생명주기 위험이 남을 수 있음
- 동일 맵 전체 브로드캐스트 방식은 클라이언트 수가 증가할수록 fan-out 비용이 급격히 증가함
- 300 clients 조건에서 auth_ms_p95가 7563.454ms로 측정되어 인증 latency 최적화가 필요함

## 남은 과제 
- ChannelSession/Player 생명주기를 shared_ptr/weak_ptr 또는 세션 식별자 기반 검증 송신 구조로 보강
- 동일 세션 패킷 순서 보장 방식 검토
- 맵 단위 또는 세션 단위 직렬화 모델 검토
- 시야 범위 기반 브로드캐스트 적용
- 이동 패킷 coalescing 또는 tick 단위 이동 상태 병합 검토
- EPOLLOUT/send queue 처리 비용 분석
- 개선 전 커밋 기준 동일 조건 재측정
- 500 clients 기준 추가 부하 테스트
