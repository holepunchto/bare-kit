#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main() {
  int pipe_fds[2];
  char buffer[1024];

  // Create a pipe (pipe_fds[0] = read end, pipe_fds[1] = write end)
  if (pipe(pipe_fds) == -1) {
    perror("pipe failed");
    return 1;
  }

  // Set read-end to non-blocking mode
  fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK);

  // Try reading from the empty pipe
  ssize_t bytes_read = read(pipe_fds[0], buffer, sizeof(buffer));

  // Print what happened
  if (bytes_read > 0) {
    printf("Read %zd bytes successfully.\n", bytes_read);
  } else if (bytes_read == 0) {
    printf("Read returned 0 (EOF or empty pipe).\n");
  } else {
    if (errno == EAGAIN) {
      printf("Read would (EAGAIN) block.\n");
    } else if (errno == EWOULDBLOCK) {
      printf("Read would (EWOULDBLOCK) block.\n");
    } else {
      perror("Read error");
    }
  }

  // Clean up
  close(pipe_fds[0]);
  close(pipe_fds[1]);

  return 0;
}
