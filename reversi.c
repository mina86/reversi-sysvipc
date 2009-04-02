#define _SVID_SOURCE 1 /* for IPC */
#define _GNU_SOURCE  1 /* for semtimedop */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "reversi.h"


struct shared *shared = 0;

const char *const player_numbers[] = { "one", "two" };


static struct {
	int server;
	int sem;
	int shm;
} config = { -1, -1, -1 };


static const struct mode {
	int (*const func)(int flags);
	unsigned flags;
	const char *const name;
	const char *const desc;
} modes[] = {
	{ runServer, FL_SERVER,
	  "server", "server running in foreground" },
	{ runServer, FL_SERVER | FL_DAEMONIZE,
	  "daemon", "server running as a daemon in background" },
	{ runClient, 0,
	  "client", "client controlled by human" },
	{ runClient, 4,
	  "ncp"   , "client controlled by computer" },
	{ runClient, 4 | FL_DAEMONIZE,
	  "ncpd"  , "client controlled by computer running in background" },
	{ 0, 0, 0, 0 }
};


static void shared_done(void);


int main(int argc, char **argv) {
	const struct mode *mode = modes;
	char *end;
	int   i;
	long  key;

	/* Check number of arguments */
	if (argc != 3) {
	usage:
		puts("usage: reversi <mode> <key>\n"
		     "<mode> is one of:");
		for (; mode->func; ++mode) {
			printf("  %-10s %s\n", mode->name, mode->desc);
		}
		return 1;
	}

	/* Read first argument */
	while (mode->func && strcmp(argv[1], mode->name)) {
		++mode;
	}
	if (!mode->func) {
		goto usage;
	}
	config.server = mode->flags & FL_SERVER;

	/* Read second argument */
	errno = 0;
	key = strtol(argv[2], &end, 0);
	if (errno || *end || key <= 0 || key >= INT_MAX) {
		goto usage;
	}

	/* Init shared resources */
	i = (config.server ? IPC_CREAT | IPC_EXCL : 0) | 0666;
	atexit(shared_done);

	/* Get semaphores */
	config.sem = semget(key, SEM_COUNT, i);
	DIE(config.sem < 0, "semget");

	/* Get shared memory */
	config.shm = shmget(key, sizeof *shared, i);
	DIE(config.shm < 0, "shmget");

	/* Attach shared memory */
	shared = shmat(config.shm, 0, 0);
	DIE(!shared, "shmat");


	/* Daemonize if requested */
	if (mode->flags & FL_DAEMONIZE) {
		/* Fork into background */
		switch (fork()) {
		case -1: DIE(1, "fork");
		case  0: break;
		default: _exit(0);
		}

		/* Change dir */
		DIE(chdir("/"), "chdir: /");

		/* Start our own session -- loose controlling terminal */
		DIE(setsid() < 0, "setsid");

		/* Fork once again so we won't be able to regain controlling
		   terminal */
		switch (fork()) {
		case -1: DIE(1, "fork");
		case  0: break;
		default: _exit(0);
		}

		/* Close all fds and open /dev/null */
		for (i = sysconf(_SC_OPEN_MAX); i > 3; close(--i)) {
			/* nop */
		}
		DIE(close(0) < 0, "close: stdin");
		DIE(open("/dev/null", O_RDONLY) < 0, "open: /dev/null");
		DIE(close(1) < 0, "close: stdout");
		DIE(open("/dev/null", O_WRONLY) < 0, "open: /dev/null");
		DIE(dup2(1, 2) < 0, "dup: stdout, stderr");
	}


	/* Start the app */
	srand(time(0));
	return mode->func(mode->flags);
}



static void shared_done(void) {
	/* Deattach shared memory */
	if (shared) {
		shmdt(shared);
		shared = 0;
	}

	/* If we are not server tere's nothing more to be done */
	if (config.server != 1) {
		return;
	}

	/* Destroy shared memory */
	if (config.shm >= 0) {
		if (shmctl(config.shm, IPC_RMID, 0) < 0) {
			perror("shmctl");
		}
		config.shm = -1;
	}

	/* Destroy semaphores */
	if (config.sem >= 0) {
		if (semctl(config.sem, IPC_RMID, 0) < 0) {
			perror("semctl");
		}
		config.sem = -1;
	}
}


int  sem_do(int num, int op, int tout) {
	struct sembuf buf;
	int ret;

	/* Init operation */
	buf.sem_num = num;
	buf.sem_op  = op < 0 ? -1 : 1;
	buf.sem_flg = 0;

	if (tout) {
		/* If it is timed operation init timeout */
		struct timespec timespec;
		timespec.tv_sec  = tout;
		timespec.tv_nsec = 0;
		/* Do the operation */
		ret = semtimedop(config.sem, &buf, 1, &timespec);
	} else {
		/* Do the operation */
		ret = semop(config.sem, &buf, 1);
	}

	DIE(ret < 0 && errno != EAGAIN, "semop");
	return ret == 0;
}



int  doDirections(const unsigned pos, const unsigned player,
                  unsigned char *map,
                  void(*callback)(unsigned pos, int step, unsigned n,
                                  unsigned player, unsigned char *map)) {
	unsigned min_horiz[3], min_vert[3];
	int x, y, ret = 0;

	if (map[pos] != 2) {
		return 0;
	}

	min_horiz[0] = pos & 7;
	min_horiz[1] = 7;
	min_horiz[2] = 7 - min_horiz[0];

	min_vert[0] = pos >> 3;
	min_vert[1] = 7;
	min_vert[2] = 7 - min_vert[0];

	for (x = -1; x <= 1; ++x) {
		const unsigned min_x = min_horiz[1 + x];
		if (min_x < 2) {
			continue;
		}

		for (y = -1; y <= 1; y += 1) {
			const unsigned min_y = min_vert[1 + y];
			const int step  = x + y * 8;
			unsigned n, p, d;

			if (min_y < 2 || step == 0) continue;

			n = min_x < min_y ? min_x : min_y;
			for (p = pos, d = 0; n && map[p+=step] == 1-player; ++d, --n) { }
			if (!n || !d || map[p] != player) continue;

			ret = 1;
			if (callback) {
				callback(pos, step, d, player, map);
			} else {
				goto done;
			}
		}
	}

 done:
	return ret;
}


static void doMove_callback(unsigned pos, int step, unsigned n,
                            unsigned player, unsigned char *map) {
	map[pos] = player;
	do {
		map[pos += step] = player;
	} while (--n);
}

int  doMove (unsigned pos, unsigned player, unsigned char *map) {
	return doDirections(pos, player, map, doMove_callback);
}
