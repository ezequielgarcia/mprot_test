#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define ARG0_OFF	100
#define ARG1_OFF	101
#define RESULT_OFF	200

#define PAGE_SIZE 4096
#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

#ifdef DEBUG
#define pr_debug(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...)
#endif

typedef char* shbuf;

/* App IDs are used as indexes into the buffer,
 * and they control the permissions.
 */
static int app_id;
static int app_other_id;

static void segv_handler(int sig_no, siginfo_t* info, void *context)
{
	shbuf buffer = (shbuf)((unsigned long)info->si_addr & ~(PAGE_SIZE - 1));

	if (mprotect(buffer, PAGE_SIZE, PROT_READ | PROT_WRITE) < 0)
		handle_error("mprotect");

	/* First, we need to tell the remote side we want to use the buffer. */
	buffer[app_id] = 1;

	/* This is a shared-memory-barrier. Without it, we can't guarantee
	 * proper ownership handshake. */
	msync(buffer, 16, MS_SYNC);

	/* Now, we check if the buffer was already in-use. */
	if (buffer[app_other_id]) {

		/* Can't use the buffer, release it. */
		buffer[app_id] = 0;

		/* Exiting here will make the buffer access spin
		 * the process until the buffer is "released",
		 * via the app_other_id index.
		 */
		if (mprotect(buffer, PAGE_SIZE, PROT_NONE) < 0)
			handle_error("mprotect");
		return;
	}
}

/*
 * Memory should always satisfy the rule:
 *
 * buffer[RESULT_OFF] == buffer[ARG0] + buffer[ARG1_OFF]
 */
static void touch_mem(shbuf buffer, int do_check)
{
	if (do_check)
		assert(buffer[RESULT_OFF] == buffer[ARG0_OFF] + buffer[ARG1_OFF]);

	buffer[ARG0_OFF] = rand() % 0x1f;
	buffer[ARG1_OFF] = rand() % 0x1f;
	buffer[RESULT_OFF] = buffer[ARG0_OFF] + buffer[ARG1_OFF];

	pr_debug("%d = %d + %d\n", buffer[RESULT_OFF], buffer[ARG0_OFF], buffer[ARG1_OFF]);

	/* Done, release the buffer. */
	buffer[app_id] = 0;

	if (mprotect(buffer, PAGE_SIZE, PROT_NONE) < 0)
		handle_error("mprotect");
}

/* This init_mem is *not* race-safe. */
static shbuf init_mem(void)
{
	int shmdes;
	shbuf buffer;

	shmdes = shm_open("mprot_test", O_RDWR | O_CREAT, 0666);
	if (shmdes < 0)
		handle_error("shm_open");
	if (ftruncate(shmdes, PAGE_SIZE) < 0)
		handle_error("ftruncate");
	buffer = mmap(NULL, PAGE_SIZE, PROT_NONE, MAP_SHARED, shmdes, 0);
	close(shmdes);
	return buffer;
}

int main(int argc, char *argv[])
{
	struct sigaction sa;
	shbuf buffer;
	int do_check;

	if (argc < 3) {
		printf("Please, give me local and remote IDs\n");
		exit(EXIT_SUCCESS);
	} else {
		app_id = atoi(argv[1]);
		app_other_id = atoi(argv[2]);
	}

	if (app_id > 15 || app_other_id > 15) {
		printf("Sorry, IDs are too large\n");
		exit(EXIT_SUCCESS);
	}

	printf("Local ID %d. Remote ID %d\n", app_id, app_other_id);

	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = segv_handler;
	if (sigaction(SIGSEGV, &sa, NULL) == -1)
		handle_error("sigaction");

	srand(time(NULL));
	buffer = init_mem();

	printf("Memory initialized, got %p\n", buffer);

	do_check = 0;
	while (1) {
		touch_mem(buffer, do_check);
		do_check = 1;
	}

	exit(EXIT_SUCCESS);
}
