#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define BUFLEN 1024
#define NPACK 10
#define PORT 6666

#define ROWS 50
unsigned g_sliceCount = 50;

void diep(char *s)
{
  perror(s);
  exit(1);
}

void ClearVolume() {
  printf("ClearVolume\n");
}

struct sockaddr_in g_si_me;
int g_socket = -1;


void udpSetup() {
  if ((g_socket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
    diep("socket");

  memset((char *) &g_si_me, 0, sizeof(g_si_me));
  g_si_me.sin_family = AF_INET;
  g_si_me.sin_port = htons(PORT);
  g_si_me.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(g_socket, (struct sockaddr *)&g_si_me, sizeof(g_si_me))==-1)
    diep("bind");
}

void udpRecv() {
  static s_expectedSlice = 0;
  struct sockaddr_in si_other;
  int slen=sizeof(si_other);
  char buf[BUFLEN];

  int byteCount = recvfrom(g_socket, buf, BUFLEN, 0, (struct sockaddr *)&si_other, &slen);

  if (byteCount == -1 ) diep("recvfrom()");
  if (byteCount > 0) {
    /*
    printf("Received packet from %s:%d DataLen: %d\n", 
        inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), byteCount);
    */

    if (buf[0] == 0xff) {
      ClearVolume();
      s_expectedSlice = 0;
    } else {
      if(byteCount != ROWS*8 + 2) {
        printf("Warning: Short slice buffer\n");
      }
      unsigned slice = buf[0];
      assert(slice >= 0);
      assert(slice < g_sliceCount);
      //printf("Got Slice %d\n",slice);

      if (slice != s_expectedSlice) {
        printf("Warning: Slice (%d) Dropped\n",s_expectedSlice);
      }
      // TODO
      // memcpy(&g_vol[slice],buf[1],byteCount-2);
          /* -2: one for slice index, one for terminator */ 
      if(buf[byteCount-1] != 0) {
        printf("Warning: Slice buffer has no terminator\n");
      }
      s_expectedSlice = ++slice;
      if (s_expectedSlice == g_sliceCount) {
        s_expectedSlice = 0;
      }
    }
  }
}


void udpCleanUp() {
  close(g_socket);
  g_socket = -1;
}

int main(void)
{
  udpSetup();
  while(1) {
    udpRecv();
  }
  udpCleanUp();
  return 0;
}
