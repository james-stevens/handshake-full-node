/*
    #################################################################
    #    (c) Copyright 2009-2025 JRCS Ltd  - All Rights Reserved    #
    #################################################################
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <arpa/nameser.h>
#include <sys/mman.h>

#include "liball.h"
#include "log_message.h"
#include "udp_sock.h"
#include "stats.h"

#define SERVER_PORT 53
#define AUTH_PORT 5349
#define REC_PORT 5350
#define RD_BIT_SET(qry) (qry[2]&1)

#define MAX_THREADS 50

typedef uint16_t dns_id_t;
#define MAX_HANDSHAKE 20
#define MAX_RESP 0x10000
#define MAX_QUERY 512
#define MAX_EACH_PROXY 0x10000
struct each_proxy_st {
	dns_id_t id,old_id;
	struct net_addr_st from_ni;
	unsigned char query[MAX_QUERY];
	int query_len;
	} each_proxy[MAX_EACH_PROXY];

dns_id_t proxy_sequence[MAX_EACH_PROXY];
int next_proxy = 0;

int server_sock = 0;
int client_sock = 0;
struct net_addr_st server_ni;
struct net_addr_st rd_zero_ni;
struct net_addr_st client_ni;
struct net_addr_st rd_one_ni;
time_t stats_interval = 10;
int force_norec = 0;

char prom_file[PATH_MAX] = { 0 };


int interupt = 0;
void sig(int s) { if (s==SIGALRM) exit(91); interupt=s; }




struct each_proxy_st *get_proxy(char * who, unsigned char *pkt)
{
unsigned char *p = pkt;
dns_id_t id_idx;

	GETSHORT(id_idx,p);
	struct each_proxy_st * t = &each_proxy[id_idx];
	logmsg(MSG_DEBUG,"%s: proxy %d has id %d, return %s:%d\n",who,id_idx,t->id,IPCHAR(t->from_ni),t->from_ni.port);
	if (!t->query_len) return NULL;
	return t;
}



int send_resp(struct stats_st *stats, struct each_proxy_st *t,unsigned char *packet,int pkt_len)
{
unsigned char *p = packet;

	logmsg(MSG_DEBUG,"response id %d to %s:%d\n",t->old_id,IPCHAR(t->from_ni),t->from_ni.port);

	PUTSHORT(t->old_id,p);
	int ret = write_udp_any(server_sock,&t->from_ni,packet,pkt_len);
	t->query_len = 0;
	stats_add(&stats->client.resp,ret);
	return ret;
}



int handle_client(struct stats_st *stats)
{
unsigned char packet[MAX_RESP];
int pkt_size = MAX_RESP;
struct net_addr_st from_ni;

	int pkt_len = read_udp_any(client_sock,client_ni.is_type,&from_ni,packet,pkt_size);

	if (pkt_len < 0) { logmsg(MSG_ERROR,"ERROR: failed to read from first - %s\n",ERRMSG); return 0; }
	stats_add(&stats->client.resp,pkt_len);

	struct each_proxy_st *this_proxy = get_proxy("client",packet);
	if (!this_proxy) { puts("client extra respose dropped"); return 0; }

	return send_resp(stats,this_proxy,packet,pkt_len);
}



int handle_query(struct stats_st *stats)
{
unsigned char *p;

	struct each_proxy_st *this_proxy = &each_proxy[proxy_sequence[next_proxy++]];
	next_proxy %= MAX_EACH_PROXY;
	this_proxy->old_id = this_proxy->query_len = 0;
	logmsg(MSG_DEBUG,"new id = %d\n",this_proxy->id);

	int pkt_size = sizeof(this_proxy->query);
	if ((this_proxy->query_len  = read_udp_any(server_sock,server_ni.is_type,&this_proxy->from_ni,this_proxy->query,pkt_size)) < 0) {
		if (errno == EAGAIN) return 0;
		logmsg(MSG_DEBUG,"WARNING: query_len = %d - %s\n",this_proxy->query_len,ERRMSG);
		return -1;
		}
	stats_add(&stats->client.qry,this_proxy->query_len);
	if (force_norec) this_proxy->query[2] = this_proxy->query[2] & 0xfe; // clear RD bit

	p = this_proxy->query;
	GETSHORT(this_proxy->old_id,p);

	logmsg(MSG_DEBUG,"read %d bytes, %s:%d, old id %d, new id %d\n",
		this_proxy->query_len,IPCHAR(this_proxy->from_ni),this_proxy->from_ni.port,this_proxy->old_id,this_proxy->id);

	p = this_proxy->query;
	PUTSHORT(this_proxy->id,p);

	struct stats_count_st *c = NULL;
	struct net_addr_st *ni = NULL;
	int rd_bit = RD_BIT_SET(this_proxy->query);
	if (rd_bit) {
		c = &stats->recs.qry;
		ni = &rd_one_ni;
	} else {
		c = &stats->auth.qry;
		ni = &rd_zero_ni;
		}

	int ret = write_udp_any(client_sock,ni,this_proxy->query,this_proxy->query_len);
	stats_add(c,ret);
	if (ret < 0) { logmsg(MSG_ERROR,"ERROR: write to %s:%d - %s\n",IPCHAR(*ni),ni->port,ERRMSG); return 0; }
	logmsg(MSG_DEBUG,"RD=%d, wrote %d bytes to %s:%d\n",rd_bit,ret,IPCHAR(*ni),ni->port);

	return 0;
}



void end_sock(int sock)
{
	shutdown(sock,SHUT_RDWR);
	close(sock);
}



int run_main_server(struct stats_st *stats)
{
	srand(getpid()*time(NULL)*13);
	for(int l=0;l<MAX_EACH_PROXY;l++) {
		int pos = rand() % MAX_EACH_PROXY;
		dns_id_t swap = proxy_sequence[pos];
		proxy_sequence[pos] = proxy_sequence[l];
		proxy_sequence[l] = swap;
		}

	if ((client_sock = udp_client_any(&client_ni,0)) < 0) {
		logmsg(MSG_ERROR,"udp_client_any client_sock : %s\n",ERRMSG); return 1; }

	logmsg(MSG_DEBUG,"server_sock = %d, client_sock %d\n",server_sock,client_sock);

	logmsg(MSG_NORMAL,"Running...\n");

	int max_sock = (client_sock > server_sock)?client_sock:server_sock;
	max_sock++;

	while(!interupt) {

		fd_set read_fds;
		FD_ZERO(&read_fds);
		FD_SET(server_sock,&read_fds);
		FD_SET(client_sock,&read_fds);

		struct timeval tv;
		tv.tv_sec = 1; tv.tv_usec = 0;
		int ret = select(max_sock,&read_fds,NULL,NULL,&tv);

		if (ret < 0) break;
		if (ret == 0) continue;

		logmsg(MSG_DEBUG,"FD_ISSET server_sock %d, client_sock %d\n",
			FD_ISSET(server_sock,&read_fds),FD_ISSET(client_sock,&read_fds));

		if (FD_ISSET(server_sock,&read_fds)) {
			if (handle_query(stats) < 0) { logmsg(MSG_DEBUG,"WARNING: handle_query failed\n"); break; } }

		if (FD_ISSET(client_sock,&read_fds)) {
			if (handle_client(stats) < 0) { logmsg(MSG_DEBUG,"WARNING: failed\n"); break; } }
		}

	logmsg(MSG_DEBUG,"thread end, interupt=%d\n",interupt);

	end_sock(client_sock);

	return 0;
}



void usage()
{
	puts("");
	puts("Usage: root_proxy");
	puts("");
	puts("    -a <addr>  - Address that will handle AUTH queries");
	puts("    -r <addr>  - Address that will handle RECURSIVE queries");
	puts("    -s <addr>  - My server listing address");
	puts("    -c <addr>  - Client address for querying ROOT servers");
	puts("    -p <path>  - File to write promtheus stats to, default = no stats written");
	puts("    -i <sec>   - Interval in secs for writing stats, default=10s");
	puts("    -l <level> - Logging level & outptu stream, see `log_message.h`");
	puts("    -n         - Force +norec (rd=0)");
	puts("");
	exit(1);
}



int main(int argc,char * argv[])
{
int running_threads = 3;
struct sigaction sa;
loglvl_t level = MSG_NORMAL|MSG_DEBUG|MSG_STDOUT|MSG_FILE_LINE;

	init_log(argv[0],level);

	signal(SIGPIPE,SIG_IGN);
	ZERO(sa);
	sa.sa_handler = sig;
	sigaction(SIGINT,&sa,NULL);
	sigaction(SIGTERM,&sa,NULL);
	sigaction(SIGALRM,&sa,NULL);
	sigaction(SIGUSR1,&sa,NULL);

	ZERO(server_ni);
	ZERO(rd_zero_ni);
	ZERO(rd_one_ni);
	ZERO(client_ni);

	AZERO(each_proxy);

	int opt;
	while ((opt=getopt(argc,argv,"i:c:s:a:r:p:l:t:n")) > 0)
		{
		switch(opt)
			{
			case 'n': force_norec = 1; break;
			case 't': running_threads = atoi(optarg); break;
			case 'i': stats_interval = atoi(optarg); break;
			case 'c': decode_net_addr(&client_ni,optarg); break;
			case 's': decode_net_addr(&server_ni,optarg); break;
			case 'a': decode_net_addr(&rd_zero_ni,optarg); break;
			case 'r': decode_net_addr(&rd_one_ni,optarg); break;
			case 'p': strcpy(prom_file,optarg); break;
			case 'l': level = LEVEL(optarg); init_log(argv[0],level); break;
			default: usage(); exit(1);
			}
		}

	if (running_threads > MAX_THREADS) { 
		logmsg(MSG_ERROR,"ERROR: %d threads exceeds maximum of %d\n",running_threads,MAX_THREADS); exit(1); }

	if (!rd_one_ni.is_type) decode_net_addr(&rd_zero_ni,"127.0.0.1");
	if (!rd_zero_ni.is_type) decode_net_addr(&rd_zero_ni,"127.0.0.1");
	if (!server_ni.is_type) decode_net_addr(&server_ni,"127.0.0.9");
	if (!client_ni.is_type) decode_net_addr(&client_ni,"0.0.0.0");

	if (!server_ni.port) server_ni.port = SERVER_PORT;
	if (!rd_zero_ni.port) rd_zero_ni.port = AUTH_PORT;
	if (!rd_one_ni.port) rd_one_ni.port = REC_PORT;

	logmsg(MSG_DEBUG,"one %s:%d\n",IPCHAR(rd_one_ni),rd_one_ni.port);
	logmsg(MSG_DEBUG,"zero %s:%d\n",IPCHAR(rd_zero_ni),rd_zero_ni.port);
	logmsg(MSG_DEBUG,"server %s:%d\n",IPCHAR(server_ni),server_ni.port);
	logmsg(MSG_DEBUG,"client %s:%d\n",IPCHAR(client_ni),client_ni.port);

	for(int l=0;l<MAX_EACH_PROXY;l++) proxy_sequence[l] = each_proxy[l].id = l;

	size_t stats_sz = sizeof(struct stats_st)*running_threads;
	struct stats_st *stats = mmap(0, stats_sz,  PROT_READ | PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	if (stats==MAP_FAILED) { logmsg(MSG_ERROR,"ERROR: mmap failed - %s\n",ERRMSG); exit(1); }
	memset(stats,0,stats_sz);

	server_sock = udp_server_any(&server_ni,0,0);
	change_to_user("daemon");

	for(int i = 0;i < running_threads;i++) {
		pid_t pid = fork();
		logmsg(MSG_DEBUG,"PID: %ld\n",pid);
		if (pid==0) exit(run_main_server(&stats[i]));
		}

	int ret;
	logmsg(MSG_DEBUG,"Master running ...\n");
	time_t now = time(NULL),next_stats = now + stats_interval;

	while(!interupt) {

		now = time(NULL);
		if (now > next_stats) {
			struct stats_st total;
			memset(&total,0,sizeof(struct stats_st));
			for(int i = 0;i < running_threads;i++) stats_add_to_total(&total,&stats[i]);
			stats_write_to_file(prom_file,&total);
			next_stats = now + stats_interval;
			}

		pid_t pid = waitpid(-1,&ret,WNOHANG);
		if (pid < 0) { logmsg(MSG_DEBUG,"waitpid - %s\n",ERRMSG); break; }

		sleep(1);
		}

	end_sock(server_sock);
	while(waitpid(-1,&ret,0) > 0);

	logmsg(MSG_DEBUG,"master end, interupt=%d\n",interupt);
	return 0;
}
