#include <stdio.h>
#include <sys/syslog.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
  openlog("writer", 0, LOG_USER);
  if (argc != 3) {
    syslog(LOG_ERR, "Must be two parameters: writefile writestr");
    return 1;
  }
  FILE * fout = fopen (argv[1], "w");
  if (!fout) {
    const char * err_description = strerror(errno);
    syslog (LOG_ERR, "Cannot open file \"%s\": %s", argv[1], err_description);
    return -1;
  }
  syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);
  if (fprintf (fout, "%s", argv[2]) < 0) {
    const char * err_description = strerror(errno);
    syslog (LOG_ERR, "Cannot write to file: %s", err_description);
    return -1;
  }
  fclose(fout);
  closelog();
  return 0;
}
