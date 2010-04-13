/*
 * libevent echo server example using buffered events.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>

/* Required by event.h. */
#include <sys/time.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
//#include <queue.h>
#include <sys/queue.h>

/* Libevent. */
#include <event.h>

#define VERSION "0.0.1"
/* Port to listen on. */
#define SERVER_PORT 19854
int debug = 0;

#define MAX_MESSAGE_SIZE 1048576

#include <string>
#include <queue>
#include <map>

typedef struct client_st {
    int fd;
    struct bufferevent *buf_ev;
} client_st;

typedef struct stat_st {
    unsigned int uptime;
    unsigned int time;
    unsigned long enqueue_items;
    unsigned long dequeue_items;
    unsigned long cmd_pushs;
    unsigned long cmd_shifts;
    unsigned long cmd_clears;
    unsigned long curr_connections;
} stat_st;

typedef struct host_st{
    /* for stat */
    unsigned long enqueue_items;
    unsigned long dequeue_items;

    unsigned int last_crawl_time;
    std::queue<std::string> *url_queue;
} host_st;

static stat_st global_stats;
typedef std::map<std::string, host_st> host_map_st;
typedef std::map<std::string, host_st>::iterator host_map_it_st;

static struct event_base *main_base;
static host_map_st global_host_map;
static host_map_it_st global_host_map_it = global_host_map.end();
static unsigned int sleep_cycle = 10;

unsigned int
get_current_time() {
    struct timeval timer;
    gettimeofday(&timer, NULL);
    return timer.tv_sec;
}

volatile unsigned int current_time;
static struct event clockevent;

/* time-sensitive callers can call it by hand with this, outside the normal ever-1-second timer */
static void set_current_time(void) {
    struct timeval timer;

    gettimeofday(&timer, NULL);
    current_time = timer.tv_sec;
}

static void clock_handler(const int fd, const short which, void *arg) {
    struct timeval t;
    t.tv_sec = 1;
    t.tv_usec = 0;

    static bool initialized = false;

    if (initialized) {
        /* only delete the event if it's actually there. */
        evtimer_del(&clockevent);
    } else {
        initialized = true;
    }

    evtimer_set(&clockevent, clock_handler, 0);
    event_base_set(main_base, &clockevent);
    evtimer_add(&clockevent, &t);

    set_current_time();
}


void
init_stat() {
    memset((void*)&global_stats, 0, sizeof(stat_st));
    global_stats.uptime = current_time;
}

void
signal_handler(int sig) {
    switch(sig) {
        case SIGTERM:
        case SIGHUP:
        case SIGINT:
            event_loopbreak();
            break;
        default:
            syslog(LOG_WARNING, "Unhandled signal (%d) %s", sig, strsignal(sig));
            break;
    }
}

/**
 * Set a socket to non-blocking mode.
 */
int
setnonblock(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL);
    if (flags < 0)
        return flags;
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0)
        return -1;

    return 0;
}
/**
 * Called by libevent when there is data to read.
 */
void
buffered_on_read(struct bufferevent *bev, void *arg)
{
    /* Write back the read buffer. It is important to note that
     * bufferevent_write_buffer will drain the incoming data so it
     * is effectively gone after we call it. */
    struct client_st *cli = (struct client_st *)arg;
    //struct tuple_entry *entry, *tmp_entry;
    //struct reader_entry *reader;
    struct evbuffer *evb;
    char *cmd;
    const char *pattern, *bytes;
    char buf[MAX_MESSAGE_SIZE];
    int i;

    *buf = '\0';
    cmd = evbuffer_readline(bev->input);
    if (cmd == NULL) {
        return;
    }

// ERROR CLIENT_ERROR SERVER_ERROR <error>\r\n
// push <host> bytes\r\n
// <data block>\r\n
// STORED\r\n
// NOT_STORED\r\n
// FULL\r\n
//
// shift\r\n
// <data block>\r\n 
// or
// EMPTY\r\n
//
// stats\r\n
// STAT key value\r\n
// END\r\n
//
    evb = evbuffer_new();
    if (strncmp(cmd, "shift", 5) == 0) {
        global_stats.cmd_shifts++;
        host_map_it_st last_it = global_host_map_it;

        if (global_host_map.empty() ) {
            evbuffer_add_printf(evb, "EMPTY\r\n");
            goto out;
        }

        do {
            if (global_host_map_it == global_host_map.end()) {
                global_host_map_it = global_host_map.begin();
            } else {
                global_host_map_it++;
            }

            if (global_host_map_it != global_host_map.end()) {
                std::queue<std::string> *url_queue = (*global_host_map_it).second.url_queue;
                if (!url_queue->empty()) {
                    int need_sleep_time = (*global_host_map_it).second.last_crawl_time + sleep_cycle - current_time;
                    if (need_sleep_time > 0) {
                        evbuffer_add_printf(evb, "SLEEP %d\r\n", need_sleep_time);
                        global_host_map_it--;
                        goto out;
                    }
                    (*global_host_map_it).second.last_crawl_time = current_time + 1; // for trasfer time
                    evbuffer_add_printf(evb, "%s\r\n", url_queue->front().c_str());
                    url_queue->pop();
                    global_stats.dequeue_items++;
                    if (debug) {
                        printf("shift host: %s\n", (*global_host_map_it).first.c_str());
                    }
                    goto out;
                }
            }
        } while (global_host_map_it != last_it);

        evbuffer_add_printf(evb, "EMPTY\r\n");
        goto out;
    } else if (strncmp(cmd, "push", 4) == 0) {
        global_stats.cmd_pushs++;
        const char *host_begin = cmd + 5;
        const char *host_end = strchr(host_begin, ' ');
        if (host_end == NULL) {
            evbuffer_add_printf(evb, "CLIENT_ERROR %s\r\n", "format error");
            goto out;
        }

        bytes = host_end + 1;
        int bytes_num = atoi(bytes);
        if (bytes_num <= 0) {
            evbuffer_add_printf(evb, "CLIENT_ERROR %s\r\n", "format error");
            goto out;
        }

        std::string host;
        host.append(host_begin, host_end - host_begin);

        if (debug) {
            fprintf(stdout, "push host: %s bytes_num: %d\n", host.c_str(), bytes_num);
        }

        int read_cnt = bufferevent_read(bev, buf, bytes_num + 2);
        if (read_cnt <= 0) {
            evbuffer_add_printf(evb, "ERROR %s\r\n", "read error");
            goto out;
        } else if (read_cnt != bytes_num + 2 || buf[read_cnt-1] != '\n' || buf[read_cnt-2] != '\r' ) {
            evbuffer_add_printf(evb, "CLIENT_ERROR %s\r\n", "format error");
            goto out;
        } else {
            std::string content;
            content.append(buf, read_cnt);
            if (debug) {
                fprintf(stdout, "read count: %d, content: %s\n", read_cnt, content.c_str());
            }
            host_map_it_st it = global_host_map.find(host);
            if (it == global_host_map.end()) {
                std::queue<std::string> *url_queue = new std::queue<std::string>();
                url_queue->push(content);
                host_st new_host;
                new_host.enqueue_items = 1;
                new_host.dequeue_items = 0;
                new_host.url_queue = url_queue;
                global_host_map[host] = new_host;
            } else {
                (*it).second.enqueue_items++;
                (*it).second.url_queue->push(content);
            }
            global_stats.enqueue_items++;
            evbuffer_add_printf(evb, "STORED\r\n");
            goto out;
        }
    } else if (strncmp(cmd, "stat", 4) == 0) {
        char buf[1024];
        memset(buf, 0, 1024);
        int sum = 0;

        unsigned int time = current_time;
        sum += sprintf(buf + sum, "STAT uptime %d\r\n", time - global_stats.uptime);
        sum += sprintf(buf + sum, "STAT time %d\r\n", time);
        sum += sprintf(buf + sum, "STAT enqueue_items %ld\r\n", global_stats.enqueue_items);
        sum += sprintf(buf + sum, "STAT dequeue_items %ld\r\n", global_stats.dequeue_items);
        sum += sprintf(buf + sum, "STAT cmd_pushs %ld\r\n", global_stats.cmd_pushs);
        sum += sprintf(buf + sum, "STAT cmd_shifts %ld\r\n", global_stats.cmd_shifts);
        sum += sprintf(buf + sum, "STAT cmd_clears %ld\r\n", global_stats.cmd_clears);
        sum += sprintf(buf + sum, "STAT curr_connections %ld\r\n", global_stats.curr_connections);
        sum += sprintf(buf + sum, "END\r\n");

        evbuffer_add_printf(evb, "%s\r\n", buf);
        goto out;

    } else if (strncmp(cmd, "clear", 5) == 0) {
    } else if (strncmp(cmd, "exit", 4) == 0
               || strncmp(cmd, "quit", 4) == 0) {
        evbuffer_add_printf(evb, "ok bye\n");
        shutdown(cli->fd, SHUT_RDWR);
    } else {
        evbuffer_add_printf(evb, "error unknown command\n");
    }
out:
    bufferevent_write_buffer(bev, evb);
    evbuffer_free(evb);
    free(cmd);
}


void
buffered_on_write(struct bufferevent *bev, void *arg)
{
}

void
buffered_on_error(struct bufferevent *bev, short what, void *arg)
{
    struct client_st *client = (struct client_st *)arg;
    if (what & EVBUFFER_EOF) {
        if (debug) {
            printf("Client disconnected.\n");
        }
    } else {
        warn("Client socket error, disconnecting.\n");
    }
    bufferevent_free(client->buf_ev);
    close(client->fd);
    free(client);

    global_stats.curr_connections--;
}

void
on_accept(int fd, short ev, void *arg)
{
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct client_st *client;

    client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        warn("accept failed");
        return;
    }

    /* Set the client socket to non-blocking mode. */
    if (setnonblock(client_fd) < 0)
        warn("failed to set client socket non-blocking");

    /* We've accepted a new client, create a client object. */
    client = (struct client_st*)calloc(1, sizeof(*client));
    if (client == NULL)
        err(1, "malloc failed");
    client->fd = client_fd;

    // Create the buffered event.
    client->buf_ev = bufferevent_new(client_fd, buffered_on_read,
        buffered_on_write, buffered_on_error, client);

    /* We have to enable it before our callbacks will be
     * called. */
    bufferevent_enable(client->buf_ev, EV_READ);

    global_stats.curr_connections++;
}

void print_usage(int exit_status) {
    printf( "QcrawlerUrlQueue" VERSION "\n");
    printf("-p <num> TCP port number to listen on(default 19854)\n"
           "-d            run as a daemon\n"
           "-v            verbose (print errors/warnings while in event loop)\n");
    exit(exit_status);
}

int
main(int argc, char **argv)
{
    int listen_fd, ch;
    int daemon = 0;
    int port = SERVER_PORT;
    struct sockaddr_in listen_addr;
    struct event ev_accept;
    int reuseaddr_on;
    pid_t   pid, sid;

    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);

    while ((ch = getopt(argc, argv, "dvp:")) != -1) {
        switch (ch) {
        case 'd':
            daemon = 1;
            break;
        case 'v':
            debug = 1;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        }
    }

    if (daemon) {
        pid = fork();
        if (pid < 0) {
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            exit(EXIT_SUCCESS);
        }

        umask(0);
        sid = setsid();
        if (sid < 0) {
            exit(EXIT_FAILURE);
        }
    }

    /* Initialize libevent. */
    main_base = event_init();
    /* initialise clock event */
    clock_handler(0, 0, 0);
    /* initialise stat */
    init_stat();



    /* Create our listening socket. */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
        err(1, "listen failed");
    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(port);
    if (bind(listen_fd, (struct sockaddr *)&listen_addr,
        sizeof(listen_addr)) < 0)
        err(1, "bind failed");
    if (listen(listen_fd, 5) < 0)
        err(1, "listen failed");
    reuseaddr_on = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on, 
        sizeof(reuseaddr_on));

    /* Set the socket to non-blocking, this is essential in event
     * based programming with libevent. */
    if (setnonblock(listen_fd) < 0)
        err(1, "failed to set server socket to non-blocking");

    /* We now have a listening socket, we create a read event to
     * be notified when a client connects. */
    event_set(&ev_accept, listen_fd, EV_READ|EV_PERSIST, on_accept, NULL);
    event_add(&ev_accept, NULL);

    /* Start the event loop. */
    event_dispatch();
    shutdown(listen_fd, SHUT_RDWR);
    close(listen_fd);
    printf("dying\n");

    return 0;
}
