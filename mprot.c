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
static unsigned long long loops;
static unsigned long long retries;

static void segv_handler(int sig_no, siginfo_t* info, void *context)
{
	shbuf remote = (shbuf)((unsigned long)info->si_addr & ~(PAGE_SIZE - 1));
	unsigned char sum;

	if (mprotect(remote, PAGE_SIZE, PROT_READ | PROT_WRITE) < 0)
		handle_error("mprotect");

	/* First, we need to tell the remote side we want to use the buffer. */
	remote[app_id] = 1;

	/* This is a shared-memory-barrier. Without it, we can't guarantee
	 * proper ownership handshake. */
	if (msync(remote, 16, MS_SYNC) < 0)
		handle_error("msync write");
	if (msync(remote, 16, MS_INVALIDATE) < 0)
		handle_error("msync invalidate");

	/* Now, we check if the buffer was already in-use. */
	if (remote[app_other_id]) {

		/* Can't use the buffer, release it. */
		remote[app_id] = 0;

		/* Exiting here will make the buffer access spin
		 * the process until the buffer is "released",
		 * via the app_other_id index.
		 */
		if (mprotect(remote, PAGE_SIZE, PROT_NONE) < 0)
			handle_error("mprotect");
		retries++;
		return;
	}
	sum = remote[ARG0_OFF] + remote[ARG1_OFF];
	assert(remote[RESULT_OFF] == sum);
}

static void get_mem(shbuf remote, shbuf local)
{
	/* Get a local copy */
	memcpy(local, remote, PAGE_SIZE);

	/* Done, release the buffer. */
	remote[app_id] = 0;

	if (mprotect(remote, PAGE_SIZE, PROT_NONE) < 0)
		handle_error("mprotect");
}

static void put_mem(shbuf remote, shbuf local)
{
	memcpy(remote + 16, local + 16, PAGE_SIZE - 16);

	/* Done, release the buffer. */
	remote[app_id] = 0;

	if (mprotect(remote, PAGE_SIZE, PROT_NONE) < 0)
		handle_error("mprotect");
}

static shbuf init_mem(void)
{
	int shmdes;
	shbuf remote;

	shmdes = shm_open("mprot_test", O_RDWR | O_CREAT, 0666);
	if (shmdes < 0)
		handle_error("shm_open");
	if (ftruncate(shmdes, PAGE_SIZE) < 0)
		handle_error("ftruncate");
	remote = mmap(NULL, PAGE_SIZE, PROT_NONE, MAP_SHARED, shmdes, 0);
	if (remote == MAP_FAILED)
		handle_error("mmap");
	close(shmdes);
	return remote;
}

int main(int argc, char *argv[])
{
	shbuf remote, local;
	struct sigaction sa;

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

	local = malloc(PAGE_SIZE);
	if (!local)
		handle_error("malloc");
	remote = init_mem();

	printf("Memory initialized, got %p\n", remote);

	while (loops < 100000) {
		get_mem(remote, local);

		/* Work on our private copy of buffer.
		 * The memory should always satisfy the rule:
		 * buffer[RESULT_OFF] == buffer[ARG0] + buffer[ARG1_OFF]
		 */
		assert(local[RESULT_OFF] == local[ARG0_OFF] + local[ARG1_OFF]);
		local[ARG0_OFF] = rand() % 0x1f;
		local[ARG1_OFF] = rand() % 0x1f;
		local[RESULT_OFF] = local[ARG0_OFF] + local[ARG1_OFF];

		/* Let's say this takes some more time */
		usleep(100);

		put_mem(remote, local);
		loops++;
	}

	printf("retries: %llu\n", retries);

	exit(EXIT_SUCCESS);
}
