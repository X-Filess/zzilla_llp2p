#ifndef __COMMON_H__
#define __COMMON_H__


#define FD_SETSIZE		2048
////resuce PARAMETER////
#define PARAMETER_X		10
#define PK_PID			999999

// M 次測量發生N次 or 連續P次發生 則判斷需要Rescue
#define PARAMETER_M		20
#define PARAMETER_N		5
#define PARAMETER_P		2

#include "configuration.h"

#include <cstdio>
#include <cstdlib>
#include <ctype.h>
#include <iostream>
#include <errno.h>
#include <assert.h>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <bitset>
#include <stdlib.h>
#include <signal.h>
//#include <getopt.h> 
#include <fstream>

#ifdef _WIN32
#include <winsock2.h>
#include <sys/timeb.h> 
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>			// for getipv4 mask
#include <Iphlpapi.h>
#include <conio.h>
#include <process.h>
#include <Mmsystem.h>
#include <time.h>
#include <TCHAR.h>
#pragma comment(lib,"ws2_32.lib")				// for visual c++ compatible
#pragma comment(lib,"iphlpapi.lib")				// for visual c++ compatible
#pragma comment (lib, "wsock32.lib")
#pragma comment (lib, "Iphlpapi.lib")
#pragma comment (lib, "Winmm.lib")
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/cdefs.h>
#include <sys/wait.h>
#include <sys/timeb.h> //--!!0208
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <net/if.h>
#include <pthread.h>
#endif

#include <map>
#include <queue>
#include <set>
#include <list>
#include <string>

#include "basic_class.h"

#ifndef __GNUG__	// I'm not g++
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#ifdef _WIN32
typedef int socklen_t; 		 // windows 沒有 socklen_t 類型 
#endif


using namespace std;
using std::bitset;

#pragma pack(1)

#define PAUSE cout << "PAUSE , Press any key to continue..."; fgetc(stdin);

#define LOGFILE			"log.txt"
#define LOGBINARYFILE	"logbinary.txt"

#define SIG_FREQ		30000
#define SYS_FREQ		1

//#define RTMP_PKT_BUF_MAX	1536	// This value defines the max rtmp packet size
#define RTMP_PKT_BUF_MAX	1024*30	// This value defines the max rtmp packet size
#define RTMP_PKT_BUF_PAY_SIZE	(RTMP_PKT_BUF_MAX - sizeof(struct chunk_header_t))	// This value defines the max rtp packet size

#define CHNK_CMD_PEER_REG				0x01	// register
#define CHNK_CMD_PEER_TCN				0x02	// topology change notification
#define CHNK_CMD_PEER_RSC				0x03	// rescue cmd
#define CHNK_CMD_PEER_CUT				0x04	// cut cmd
#define CHNK_CMD_PEER_BWN				0x05	// bandwidth notification cmd
#define CHNK_CMD_PEER_CON				0x06	// connect cmd (connect to peer)
#define CHNK_CMD_PEER_DATA				0x07	// Data cmd (this cmd encapsulate data into transport layer)
#define CHNK_CMD_PEER_REQ_FROM			0x08	// req from (the peer will send this command that which seq itslf want to request from)
#define CHNK_CMD_PEER_RSC_LIST			0x09	// rescue to pk's cmd
#define CHNK_CMD_PEER_SWAP				0x0a	// swap cmd
#define CHNK_CMD_CHN_OPEN				0x0b		// open channel
#define CHNK_CMD_CHN_STOP				0x0c		// stop channel
#define CHNK_CMD_CHN_INFO				0x0d		// query channel information
#define CHNK_CMD_RT_NLM					0x0e    // network latency measurement    //--!!0121
#define CHNK_CMD_PEER_LAT				0x0f	// latency cmd, can only used by NS2, implementation can only use this cmd until iplement NTP to sync peer's time
#define CHNK_CMD_PEER_DEP				0x10	// departure
#define CHNK_CMD_PEER_NOTIFY        	0x11
#define CHNK_CMD_PEER_LATENCY           0x12
#define CHNK_CMD_CHN_UPDATA_DATA        0x13	// update steam id to peer
#define CHNK_CMD_PEER_UNKNOWN			0xFF	// 1 B cmd => 0xFF is reserved for unknown cmd


#define OK				0x01
#define REJECT			0x02

#define RTP_PKT_BUF_MAX	1024*30	// This value defines the max rtp packet size
#define RTP_PKT_BUF_PAY_SIZE	(RTP_PKT_BUF_MAX - sizeof(struct chunk_header_t) - sizeof(struct rtp_hdr_t))	// This value defines the max rtp packet size
#define MAXFDS 			2048
#define EVENTSIZE 		2048
#define MAX_POLL_EVENT 	64
#define HTML_SIZE 		8192
#define BUCKET_SIZE		2048/4
#define BANDWIDTH_BUCKET	7
#define MAX_PEER_LIST		30
#define WIN_COUNTER		50

#define STRM_TYPE_AUDIO	0x00	// 0 = AUDIO STREAM
#define STRM_TYPE_VIDEO	0x01	// 1 = VIDEO STREAM
#define STRM_TYPE_MEDIA	0x02	// 2 = MEDIA STREAM

#define ADD_STREAM          2
#define DELETE_STREAM       3

#define CLASS_ARY_SIZE		1
#define CRLF_LEN			4

#define mode_RTSP			1
#define mode_RTMP			2
#define mode_SG				3
#define mode_BitStream		4
#define mode_HTTP			5
#define mode_File			6

#define TE_RTSP				1
#define TE_RTMP				2	
#define TE_SG				3
#define STRATEGY_DFS		1
#define STRATEGY_BFS		2

#define REQUEST				0
#define REPLY				1

#define DBG_MODE

#ifdef DBG_MODE

#define DBG_PRINTF( ...) do {	\
									printf("[%s](Line:%d):", __FILE__, __LINE__);\
									printf(__VA_ARGS__);\
								} while (0);
#else
#define DBG_PRINTF 
#endif

enum RET_VALUE {RET_WRONG_SER_NUM = -3, RET_SOCK_ERROR = -2, RET_ERROR = -1, RET_OK = 0};


struct peer_info_t {
	unsigned long pid;
	unsigned long level;
	unsigned long public_ip;
	unsigned long private_ip;
	unsigned short tcp_port;
	unsigned short udp_port;
	unsigned long manifest;
	int rescueStatsArry[PARAMETER_M];
};


struct level_info_t {
	unsigned long pid;
	unsigned long level;
	unsigned long public_ip;
	unsigned long private_ip;
	unsigned short tcp_port;
	unsigned short udp_port;
};

struct request_info_t {
	unsigned long pid;
	unsigned long channel_id;
	unsigned long private_ip;
	unsigned short tcp_port;
	unsigned short udp_port;
};

struct rtsp_int_hdr_t {
	unsigned char magic;
	unsigned char channel;
	unsigned short length; 
};

struct ts_block_t {		//--!!0124
	unsigned long pid;
	unsigned long time_stamp;
	unsigned long
				rsv:31,
				isDST:1;
};

///P2P  main  header
struct chunk_header_t {
	unsigned char cmd;
	unsigned char 
		stream:3,
		payload_type:5;
	unsigned char 
		rsv_1:1,
		mf:1,
		part_seq:6;
	unsigned char stream_id;
	unsigned int sequence_number;
	unsigned int timestamp;
	unsigned short rsv_3;
	unsigned short length;
};


//detection Info
struct detectionInfo{
	LARGE_INTEGER	lastAlarm;
	LARGE_INTEGER	firstAlarm;
	LARGE_INTEGER	previousAlarm;

	unsigned int	last_timestamp;
	unsigned int	first_timestamp;
	unsigned long	last_seq;

	unsigned int	measure_N;		//第N次測量
	unsigned int	count_X;		//X個封包量一次

	unsigned int	total_buffer_delay;
	double			last_sourceBitrate;
	double			last_localBitrate;
	unsigned int	total_byte;
};


struct rtp_hdr_t {
	unsigned char
		csrc_cnt:4, // The CSRC count contains the number of CSRC identifiers that follow the fixed header
		extension:1, // set to indicate that fixed header MUST be followed by exactly one header extension
		padding:1,	// set to indicate that there exist padding
		ver:2;
	unsigned char 
		pay_type:7, // payload type
		marker:1;
	unsigned short seq_num;
	unsigned long timestamp;
	unsigned long ssrc;	// identifies the synchronization source		
};


struct chunk_t{
	struct chunk_header_t header;
	unsigned char buf[0];
};


struct chunk_rtp_t{
	struct chunk_header_t header;
//	struct rtp_hdr_t hdr;
	unsigned char payload[RTP_PKT_BUF_PAY_SIZE];
};


struct chunk_rtmp_t{
	struct chunk_header_t header;
	//struct rtmp_hdr_t hdr;
	//unsigned char hdr_buf[13];
	unsigned char buf[RTMP_PKT_BUF_PAY_SIZE];
};


struct chunk_bitstream_t{
	struct chunk_header_t header;
	unsigned char buf[RTMP_PKT_BUF_PAY_SIZE];
};



struct chunk_request_msg_t{
	struct chunk_header_t header;
	struct request_info_t info;
	unsigned char buf[0];
};

struct chunk_request_pkt_t{
	struct chunk_header_t header;
	unsigned long pid;
	unsigned int request_from_sequence_number;
	unsigned int request_to_sequence_number;
};

struct chunk_register_reply_t {
	struct chunk_header_t header;
	unsigned long pid;
	unsigned long level;
	unsigned long bit_rate;
	unsigned long sub_stream_num;
	unsigned long parallel_rescue_num;
	unsigned long inside_lane_rescue_num;
	struct level_info_t *level_info[0];	
};

struct chunk_bandwidth_t {
	struct chunk_header_t header;
	unsigned long bandwidth;
};


///sent to pk , rescue from pk
struct chunk_pk_rescue_t {
	struct chunk_header_t header;
	unsigned long pid;
	unsigned long manifest;
	unsigned int from_sequence_number;
	
};



struct chunk_rescue_reply_t {
	struct chunk_header_t header;
	unsigned char reply;
	unsigned long manifest;
};


struct chunk_rescue_list_t {
	struct chunk_header_t header;
	unsigned long pid;
};


struct chunk_rescue_list_reply_t {
	struct chunk_header_t header;
	unsigned long pid;
	unsigned long level;
	struct level_info_t *level_info[0];	
};

enum network_nonblocking_ctl_state {
	READY = 0,
	RUNNING = 1
};

enum pkg_nonblocking_ctl_state {
	READ_HEADER_TYPE = 0,
	READ_HEADER_CHANNEL_0 = 1,
	READ_HEADER_CHANNEL_1 = 2,
	READ_HEADER_LAST = 3,
	READ_HEADER_EXTEND_TIME = 4,
	READ_BODY = 5,
	READ_CHUNK_FINISH = 6
};

typedef struct {
	char *buffer;
	struct chunk_rtmp_t* rtmp_chunk;
	unsigned int offset;
	unsigned int total_len;
	unsigned int expect_len;
	unsigned int serial_num; //never use
	network_nonblocking_ctl_state ctl_state;
} Network_nonblocking_ctl;

typedef struct {
	pkg_nonblocking_ctl_state recv_packet_state;
	Network_nonblocking_ctl recv_ctl_info;
} Recv_nonblocking_ctl;

struct chunk_level_msg_t {
	struct chunk_header_t header;
	unsigned long pid;
	unsigned long level;
	struct level_info_t *level_info[0];	
};

struct chunk_cut_peer_info_t {
	struct chunk_header_t header;
	unsigned long pid;
};

struct chunk_rt_latency_t {   //--!!0124
	struct chunk_header_t header;
	unsigned short dts_offset;
	unsigned short dts_length; //followed by struct ts_block_t[]
	struct ts_block_t buf[0];
};

struct web_control_info {
    int type;
    int stream_id;
	char user_name[64];
};

struct channel_stream_map_info_t {
	unsigned char stream_id;
	unsigned char rsv_1;
	char user_name[64];
};

struct channel_chunk_size_info_t {
	unsigned char stream_id;
	unsigned char rsv_1;
	int chunk_size;
};

struct channel_stream_notify {
	struct chunk_header_t header;
	unsigned long total_num;
	struct channel_stream_map_info_t *channel_stream_map_info[0];	
};

struct peer_timestamp_info_t {
	unsigned long pid;
	unsigned long peer_sec;
	unsigned long peer_usec;
};

struct peer_latency_measure {
	struct chunk_header_t header;
	unsigned long pk_sec;
	unsigned long pk_usec;
	unsigned long total_num;
	struct peer_timestamp_info_t peer_timestamp_info[0];
};






#endif
