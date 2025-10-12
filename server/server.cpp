#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <atomic>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define FILE_OUT ("/var/tmp/aesdsocketdata")

std::atomic_int running = 1;
std::atomic_int waiting_for_connection = 0;

static int serve_client(int server_fd, struct sockaddr_in *address);

void sig_handler(int sgnum) {
  running = 0;
  // If we're blocked in accept(), we need to handle cleanup
  if (waiting_for_connection) {
    syslog(LOG_USER, "Caught signal, exiting");
  }
}

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

void daemonize() {
  pid_t pid;

  // Fork the parent process
  pid = fork();
  if (pid < 0) {
    perror("fork failed");
    exit(EXIT_FAILURE);
  }

  // Exit the parent process
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  // Create new session
  if (setsid() < 0) {
    perror("setsid failed");
    exit(EXIT_FAILURE);
  }

  // Change working directory to root
  if (chdir("/") < 0) {
    perror("chdir failed");
    exit(EXIT_FAILURE);
  }

  // Redirect stdin, stdout, stderr to /dev/null
  int fd = open("/dev/null", O_RDWR);
  if (fd < 0) {
    perror("Cannot open /dev/null");
    exit(EXIT_FAILURE);
  } else {
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > 2) {
      close(fd);
    }
  }
}

int main(int argc, char *argv[]) {
  bool daemon_mode = false;
  int ret = 0;

  // Parse command line arguments
  if (argc > 1 && strcmp(argv[1], "-d") == 0) {
    daemon_mode = true;
  }

  openlog("aesdsocket", 0, LOG_USER);
  sig_setup();

  int server_fd;
  struct sockaddr_in address;
  int opt = 1;

  // Creating socket file descriptor
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket failed");
    return EXIT_FAILURE;
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("setsockopt");
    close(server_fd);
    return EXIT_FAILURE;
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(9000);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    close(server_fd);
    return EXIT_FAILURE;
  }

  // Daemonize after successful bind
  if (daemon_mode) {
    daemonize();
    // Reopen syslog after daemonizing since file descriptors were closed
    openlog("aesdsocket", 0, LOG_USER);
  }

  if (listen(server_fd, 5) < 0) {
    perror("listen");
    close(server_fd);
    return EXIT_FAILURE;
  }

  do {
    ret = serve_client(server_fd, &address);
    if (ret < 0) {
      break;
    }
  } while (running);

  close(server_fd);
  remove(FILE_OUT);
  closelog();
  return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

int serve_client(int server_fd, struct sockaddr_in *address) {
  ssize_t pkt_len = 0;
  std::string buffer;
  size_t buffer_idx = 0;
  char pkt[2048] = {0};
  socklen_t addrlen = sizeof(*address);
  int new_socket;
  int fd = -1;

  waiting_for_connection = 1;
  new_socket = accept(server_fd, (struct sockaddr *)address, &addrlen);
  waiting_for_connection = 0;

  if (new_socket < 0) {
    if (errno == EINTR || running == 0) {
      return 0; // Signal received, normal exit
    }
    perror("accept");
    return EXIT_FAILURE;
  }

  inet_ntop(AF_INET, &(address->sin_addr), pkt, INET_ADDRSTRLEN);
  syslog(LOG_USER, "Accepted connection from %s", pkt);

  // Open file for this connection
  fd = open(FILE_OUT, O_RDWR | O_CREAT | O_APPEND, 0644);
  if (fd < 0) {
    perror("Cannot open file");
    close(new_socket);
    return EXIT_FAILURE;
  }

  while (running) {
    pkt_len = recv(new_socket, pkt, sizeof(pkt) - 1, 0);

    if (pkt_len == 0 || running == 0) {
      // Connection closed or signal received
      break;
    } else if (pkt_len < 0) {
      if (errno == EINTR) {
        break;
      }
      perror("Error while recv");
      break;
    }

    // Append received data to buffer
    buffer.insert(buffer.end(), pkt, pkt + pkt_len);

    // Process all complete packets (ending with \n)
    for (size_t i = buffer_idx; i < buffer.length(); ++i) {
      if (buffer[i] == '\n') {
        // Write this packet to file
        ssize_t write_len = i - buffer_idx + 1;
        if (write(fd, &buffer[buffer_idx], write_len) != write_len) {
          perror("Error writing to file");
          close(fd);
          close(new_socket);
          return EXIT_FAILURE;
        }

        buffer_idx = i + 1;

        // Close and reopen file for reading
        close(fd);
        fd = open(FILE_OUT, O_RDONLY);
        if (fd < 0) {
          perror("Cannot open file for reading");
          close(new_socket);
          return EXIT_FAILURE;
        }

        // Send entire file contents back to client
        char buff_read[1500];
        ssize_t bytes_read;
        while ((bytes_read = read(fd, buff_read, sizeof(buff_read))) > 0) {
          size_t sent = 0;
          while (sent < (size_t)bytes_read) {
            ssize_t send_ret =
                send(new_socket, buff_read + sent, bytes_read - sent, 0);
            if (send_ret < 0) {
              perror("Can't send");
              close(fd);
              close(new_socket);
              return EXIT_FAILURE;
            } else if (send_ret == 0) {
              break;
            }
            sent += send_ret;
          }
        }

        // Close and reopen file for appending
        close(fd);
        fd = open(FILE_OUT, O_RDWR | O_CREAT | O_APPEND, 0644);
        if (fd < 0) {
          perror("Cannot open file for appending");
          close(new_socket);
          return EXIT_FAILURE;
        }
      }
    }

    // Clean up processed data from buffer to prevent unbounded growth
    if (buffer_idx > 0) {
      buffer.erase(0, buffer_idx);
      buffer_idx = 0;
    }
  }

  // Cleanup
  if (fd >= 0) {
    close(fd);
  }

  if (running == 0) {
    syslog(LOG_USER, "Caught signal, exiting");
  } else {
    inet_ntop(AF_INET, &(address->sin_addr), pkt, INET_ADDRSTRLEN);
    syslog(LOG_USER, "Closed connection from %s", pkt);
  }

  close(new_socket);
  return 0;
}
