#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <mysql/mysql.h>
#include <time.h> // 시간 정보를 사용하기 위해 헤더파일 추가

#define BUF_SIZE 100
#define NAME_SIZE 20
#define ARR_CNT 8
#define DEBUG

void* recv_bluetooth(void* arg);
void error_handling(char* msg);

char zone;
int scan_cnt;

int main(int argc, char* argv[])
{
	int btfd;
	pthread_t bt_thread;
	void* thread_return;
	struct sockaddr_rc addr = { 0 };
	char dest[18] = "98:DA:60:08:0C:D9"; // iot09 MAC 주소

	if (argc != 4) {
		printf("Usage : %s <zone> <device> <scan_cnt>\n", argv[0]);
		exit(1);
	}
	zone = argv[1][0];
	if (argv[2][0] == '1')
	{
		strcpy(dest, "98:DA:60:09:6E:F4"); //iot12 MAC 주소
	}
	scan_cnt = atoi(argv[3]);

	// 블루투스 소켓 생성 및 연결
	btfd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
	if (btfd == -1) {
		perror("Bluetooth socket()");
		exit(1);
	}

	addr.rc_family = AF_BLUETOOTH;
	addr.rc_channel = (uint8_t)1;
	str2ba(dest, &addr.rc_bdaddr);

	printf("Connecting to Bluetooth device %s...\n", dest);
	if (connect(btfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("Bluetooth connect()");
		exit(1);
	}
	printf("Bluetooth connected.\n");

	// 블루투스 수신 스레드 생성
	pthread_create(&bt_thread, NULL, recv_bluetooth, (void*)&btfd);

	pthread_join(bt_thread, &thread_return);

	close(btfd);
	return 0;
}




// 블루투스 데이터 수신 및 처리
void* recv_bluetooth(void* arg)
{
	int btfd = *(int*)arg;
	int str_len;

	char buf[1024];
	char line_buf[1024];
	int total = 0;

	MYSQL* conn;
	int res;
	// SQL 명령을 담을 버퍼, 하나의 INSERT 문을 담기에 충분한 크기
	char sql_cmd[512] = { 0 };
	char* host = "10.10.14.72";
	char* user = "wifi";
	char* pass = "user";
	char* dbname = "wifiFingerprinting";

	int i;
	char* pToken;
	char* pArray[ARR_CNT] = { 0 };
	int count = 0;

	// --- 새로운 로직을 위한 변수 ---
	// 한 번의 스캔 이벤트에 대한 타임스탬프를 저장할 버퍼
	char scan_timestamp[20] = { 0 };

	conn = mysql_init(NULL);

	puts("MYSQL startup");
	if (!(mysql_real_connect(conn, host, user, pass, dbname, 0, NULL, 0)))
	{
		fprintf(stderr, "ERROR : %s[%d]\n", mysql_error(conn), mysql_errno(conn));
		exit(1);
	}
	else
		printf("SQL Connection Successful!\n\n");

	while (1) {
		str_len = read(btfd, buf + total, sizeof(buf) - total - 1);
		if (str_len <= 0) {
			printf("Bluetooth connection closed.\n");
			break;
		}
		total += str_len;
		buf[total] = '\0';

		char* p_lf;
		while ((p_lf = strchr(buf, '\n')) != NULL) {
			int line_len = p_lf - buf;

			if (line_len < sizeof(line_buf)) {
				strncpy(line_buf, buf, line_len);
				line_buf[line_len] = '\0';

				char* p_cr = strchr(line_buf, '\r');
				if (p_cr != NULL) *p_cr = '\0';

				char msg_to_send[1024];
				snprintf(msg_to_send, sizeof(msg_to_send), "%s\n", line_buf);

				// 1. AP 정보 라인 처리 (+CWLAP)
				if (strncmp(msg_to_send, "+CWLAP:(", 8) == 0) {

					// 새 스캔 시작: 타임스탬프가 비어있으면 현재 시간으로 생성
					if (scan_timestamp[0] == '\0') {
						time_t t = time(NULL);
						struct tm* tm = localtime(&t);
						strftime(scan_timestamp, sizeof(scan_timestamp), "%Y-%m-%d %H:%M:%S", tm);
					}

					pToken = strtok(msg_to_send, ",");
					i = 0;
					while (pToken != NULL && i < ARR_CNT) {
						pArray[i] = pToken;
						pToken = strtok(NULL, ",");
						i++;
					}

					// pArray[1]: SSID, pArray[2]: RSSI, pArray[3]: BSSID
					if (i > 3) {
						char clean_ssid[33] = { 0 };
						char clean_bssid[18] = { 0 };

						// pArray에서 양쪽 큰따옴표 제거
						// 예: "iptime" -> iptime
						sscanf(pArray[1], "\"%32[^\"]\"", clean_ssid);
						sscanf(pArray[3], "\"%17[^\"]\"", clean_bssid);

						// DB에 INSERT할 SQL문 생성
						snprintf(sql_cmd, sizeof(sql_cmd),
							"INSERT INTO wifi_scan_gem (zone, ssid, bssid, rssi, scan_timestamp) VALUES ('%c', '%s', '%s', %s, '%s')",
							zone, clean_ssid, clean_bssid, pArray[2], scan_timestamp);

#ifdef DEBUG
						printf("SQL: %s\n", sql_cmd);
#endif
						// SQL 쿼리 실행
						res = mysql_query(conn, sql_cmd);
						if (res != 0) {
							fprintf(stderr, "INSERT ERROR: %s\n", mysql_error(conn));
						}
					}
				}
				// 2. 스캔 종료 라인 처리
				else if (strncmp(msg_to_send, "---SCAN_END---", 14) == 0) {
					printf("Scan batch finished for timestamp: %s, batch no : %d\n", scan_timestamp, count);
					// 다음 스캔을 위해 타임스탬프 초기화
					scan_timestamp[0] = '\0';
					if (++count > scan_cnt)
						break;
					//갯수지정
				}
			}

			total -= (line_len + 1);
			memmove(buf, p_lf + 1, total);
			buf[total] = '\0';
		}
	}
	mysql_close(conn);
	return NULL;
}

void error_handling(char* msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
