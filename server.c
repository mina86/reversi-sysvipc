#define _POSIX_C_SOURCE 1

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "reversi.h"


static unsigned char private_map[8 * 8];
static pid_t pids[2];


/* Sends message to user and wait for reply */
static inline void doSend(unsigned num);
/* Sends a READ_MAP message to both users */
static        void doReadMap(void);

/* Is there any possible move for user */
extern int  canMove(unsigned player);


/* Main loop */
static void doInit (void); /* Init game */
static void doPlay (void); /* Play the game */
static void doEnd  (void); /* Announce the result */


/* On signal exit gracefully */
static void handleSignal(int sig) {
	printf("Got signal %d, terminating\n", sig);
	exit(0);
}


/* Server */
int runServer(int flags) {
	(void)flags;

	signal(SIGINT , handleSignal);
	signal(SIGQUIT, handleSignal);
	signal(SIGTERM, handleSignal);

	for (;;) {
		doInit();
		doPlay();
		doEnd();
	}
}


/* Intialize the game */
static void doInit(void) {
	/* Wait for players */
	puts("Waiting for players...");
	pids[0] = pids[1] = 0;

	shared->data = 0;
	doSend(SEM_INIT);
	pids[0] = shared->data;
	puts("Got player one...");

	shared->data = 1;
	doSend(SEM_INIT);
	pids[1] = shared->data;
	puts("Got player two...");

	/* Prepare the map */
	memset(private_map, 2, sizeof private_map);
	private_map[3 + 3 * 8] = private_map[4 + 4 * 8] = 0;
	private_map[4 + 3 * 8] = private_map[3 + 4 * 8] = 1;

	/* Make them read the map */
	doReadMap();
}


/* Play the game */
static void doPlay(void) {
	unsigned player;

	player = rand() & 1;
	while (canMove(player ^= 1) || canMove(player ^= 1)) {
		printf("Waiting for player %s...\n", player_numbers[player]);
		do {
			shared->message = MSG_MAKE_MOVE;
			doSend(SEM_PLAYERS + player);
		} while (!doMove(shared->data, player, private_map));

		doReadMap();
	}
}


static void doEnd(void) {
	unsigned count[] = { 0, 0, 0 }, i;

	for (i = 0; i < 64; ++i) {
		++count[private_map[i]];
	}

	if (count[0] > count[1]) {
		puts("Player one wins");
		shared->data = 0;
	} else if (count[0] < count[1]) {
		puts("Player two wins");
		shared->data = 1;
	} else {
		puts("No one wins");
		shared->data = 2;
	}

	shared->message = MSG_GAME_OVER;
	up(SEM_PLAYER_0);
	up(SEM_PLAYER_1);
	down(SEM_SERVER);
	down(SEM_SERVER);
	putchar('\n');
}



int  canMove(unsigned player) {
	unsigned pos;
	for (pos = 0; pos < 64; ++pos) {
		if (doDirections(pos, player, private_map, 0)) {
			return 1;
		}
	}
	return 0;
}


static void doDown(unsigned times) {
	while (times > 0) {
		while (!timed_down(SEM_SERVER)) {
			int i = 0;
			do {
				if (pids[i] && kill(pids[i], 0) < 0) {
					printf("Player %s dies\n", player_numbers[i]);
					exit(0);
				}
			} while (++i < 2);
		}
		--times;
	}
}

static inline void doSend(unsigned num) {
	up(num);
	doDown(1);
}

static void doReadMap(void) {
	memcpy(shared->map, private_map, sizeof shared->map);
	shared->message = MSG_READ_MAP;
	up(SEM_PLAYER_0);
	up(SEM_PLAYER_1);
	doDown(2);
}
