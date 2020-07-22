// Test that fork fails gracefully.
// Tiny executable so that the limit can be filling the proc table.

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define N 10

/*
void
printf(int fd, const char *s, ...)
{
  write(fd, s, strlen(s));
}
*/

void
forktest(void)
{
  int n, pid;

  // printf(1, "fork test\n");

  for(n=0; n<N; n++){
    pid = fork();
    if(pid < 0)
      break;
    if(pid == 0) {
    	// printf(1, "child\n");
    	int fd = open("test3child.txt", O_CREATE | O_WRONLY);
		for (int i = 0; i < 10; i++) {
			// printf(fp, "Writing to test.txt ...\n");
			write(fd, "Writing to test.txt ...\n", 1);
			sleep(0);
		}
		close(fd);
		getpinfo(getpid());
    	exit();
    } else {
      	// printf(1, "parent\n");
   		int fd = open("test3parent.txt", O_CREATE | O_WRONLY);
		for (int i = 0; i < 10; i++) {
			// printf(fp, "Writing to test.txt ...\n");
			write(fd, "Writing to test.txt ...\n", 1);
		}
		close(fd);
		wait();
		getpinfo(getpid());
    }
  }

  if(n == N){
    printf(1, "\nTest3 Complete\n");
    exit();
  }

  for(; n > 0; n--){
    if(wait() < 0){
      printf(1, "wait stopped early\n");
      exit();
    }
  }

  if(wait() != -1){
    printf(1, "wait got too many\n");
    exit();
  }

  // printf(1, "Test3 Complete\n");
}

int
main(void)
{
  forktest();
  exit();
}
