#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define PORT 12345


static int send_value;
class NetSend {
public:
  NetSend();
  NetSend* getInstance();
  int start_server();
  int stop_server();
private:
  static NetSend* instance;
  static void* init_server(void* threadarg);
  int thread_id;
  pthread_t thread;
};
