#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <atomic>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define FILE_OUT ("/var/tmp/aesdsocketdata")

std::atomic_int running = 1;
static int serve_client(int server_fd, struct sockaddr_in *address, FILE *fout);

void sig_handler(int sgnum) { running = 0; }
void sig_setup() {
  struct sigaction sa;
  sa.sa_handler = sig_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    perror("Error setting up SIGINT handler with sigaction");
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGTERM, &sa, NULL) == -1) {
    perror("Error setting up SIGTERM handler with sigaction");
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char *argv[]) {
  openlog("server", 0, LOG_USER);
  sig_setup();

  int server_fd;
  struct sockaddr_in address;
  int opt = 1;
  FILE *fout = fopen(FILE_OUT, "w+");

  // Creating socket file descriptor
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket failed");
    return EXIT_FAILURE;
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt))) {
    perror("setsockopt");
    return EXIT_FAILURE;
  }
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(9000);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    return EXIT_FAILURE;
  }
  if (listen(server_fd, 0) < 0 && errno != EINTR) {
    perror("listen");
    return EXIT_FAILURE;
  } else if (errno == EINTR) {
    goto STOP;
  }
  do {
    int ret = serve_client(server_fd, &address, fout);
    if (ret < 0) {
      break;
    }
  } while (running);

STOP:
  close(server_fd);
  fclose(fout);
  return 0;
}

int serve_client(int server_fd, struct sockaddr_in *address, FILE *fout) {
  ssize_t pkt_len;
  std::string buffer;
  size_t buffer_idx = 0;
  char pkt[2048] = {0};
  size_t file_sz = 0;
  socklen_t addrlen = sizeof(*address);
  int new_socket;
  if (!fout) {
    perror("Cannot open file");
    return EXIT_FAILURE;
  }
  if ((new_socket = accept(server_fd, (struct sockaddr *)address, &addrlen)) <
          0 &&
      errno != EINTR) {
    perror("accept");
    return EXIT_FAILURE;
  } else if (errno == EINTR) {
    goto STOP;
  }
  inet_ntop(AF_INET, &(address->sin_addr), pkt, INET_ADDRSTRLEN);
  syslog(LOG_DEBUG, "Accepted connection from %s", pkt);
  syslog(LOG_DEBUG, "File sz: %zu", file_sz);

  while (running) {
    pkt_len = recv(new_socket, pkt, sizeof(pkt) / sizeof(pkt[0]) - 1, 0);
    if (running == 0 || pkt_len == 0) {
      goto STOP;
    } else if (pkt_len < 0) {
      perror("Error while recv");
      goto STOP;
    } else {
      buffer += std::string_view(pkt, pkt_len);
      for (size_t i = buffer_idx; i < buffer.length(); ++i) {
        if (buffer[i] == '\n') {
          fwrite(&buffer[buffer_idx], 1, i - buffer_idx + 1, fout);
          fflush(fout);
          fseek(fout, 0, SEEK_SET);
          buffer_idx = i + 1;
          do {
            char buff_read[1500];
            const size_t ret_code = fread(
                buff_read, 1, sizeof(buff_read) / sizeof(buff_read[0]), fout);
            if (ret_code > 0) {
              size_t sent = 0;
              do {
                const int send_ret =
                    send(new_socket, buff_read + sent, ret_code - sent, 0);
                if (send_ret < 0) {
                  perror("Can't send");
                  return EXIT_FAILURE;
                } else if (send_ret == 0) {
                  break;
                }
                sent += send_ret;
              } while (sent < ret_code);
            }
          } while (!feof(fout));
        }
      }
      file_sz += pkt_len;
    }
  }

STOP:
  if (running == 0) {
    syslog(LOG_DEBUG, "Caught signal, exiting");
  } else if (pkt_len == 0) {
    syslog(LOG_DEBUG, "File sz: %zu", file_sz);
    syslog(LOG_DEBUG, "End file reception");
  }
  close(new_socket);
  return pkt_len;
}
