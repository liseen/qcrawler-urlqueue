#include "url_queue_client.h"


UrlQueueClient::~UrlQueueClient()
{
    if (queue != NULL) {
        memcached_free(queue);
    }
}


UrlQueueClient::UrlQueueClient(const std::string &queue_server)
{
    server_str = queue_server;

    queue  = memcached_create(NULL);
    memcached_server_st *servers;
    servers= memcached_servers_parse((char*)server_str.c_str());
    if (servers != NULL) {
       memcached_server_push(queue, servers);
       memcached_server_list_free(servers);
    }
}

bool UrlQueueClient::push(const std::string host, const std::string content)
{
    memcached_return rc;
    rc = memcached_add(queue, host.c_str(), host.length(),
                content.c_str(), content.length(),
                1, 0);

    if (rc == MEMCACHED_SUCCESS) {
        return true;
    } else if (rc == MEMCACHED_NOTSTORED) {
        return false;
    } else {
        return false;
    }

    return true;
}

bool UrlQueueClient::shift(std::string *str)
{
    memcached_return rc;
    unsigned int flags;
    size_t data_size;
    char * data = memcached_get (queue,
                   URL_QUEUE_KEY_NAME, sizeof(URL_QUEUE_KEY_NAME),
                   &data_size,
                   &flags,
                   &rc);

    if (rc == MEMCACHED_SUCCESS) {
        str->clear();
        str->append(data, data_size);
        free(data);
        return true;
    }

    if (rc == MEMCACHED_NOTFOUND) {
        return false;
    }

     // error
    return false;
}

