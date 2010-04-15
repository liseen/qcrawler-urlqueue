#ifndef URL_QUEUE_CLIENT_H
#define URL_QUEUE_CLIENT_H

#include "url_queue_common.h"

#include <libmemcached/memcached.h>
#include <string>

class UrlQueueClient
{
public:
    UrlQueueClient(const std::string &queue_server);
    ~UrlQueueClient();

    bool connect();
    bool push(const std::string host, const std::string content);
    bool shift(std::string *str);

private:
    std::string server_str;
    memcached_st *queue;
};

#endif
