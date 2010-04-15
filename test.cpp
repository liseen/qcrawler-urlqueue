#include "url_queue_client.h"

#include <string>
#include <iostream>

using namespace std;

int main()
{
    UrlQueueClient client("localhost:19854");

    for (int i = 0; i < 10; i++) {
        std::string content("iiiiiiiiiiiiiibbbbbbbbbbba");
        content[4] = '\00';

        client.push("host.host.host", content);
    }

    for (int i = 0; i < 10; i++) {
        std::string content;
        client.shift(&content);
        cout << "shift: ";
        for (int j = 0; j < content.size(); j++) {
            std::cout << (int) content[j] << " ";
        }
        cout << endl;
    }

    return 0;
}
