#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>

#include "reversi.h"

static void terminal_init(void);
static void terminal_done(void);

static void     printMap (unsigned player, unsigned char *map);
static unsigned readPos  (unsigned player, struct shared *shared);
static unsigned choosePos(unsigned player, struct shared *shared);


int runClient(int flags, struct shared *shared) {
	unsigned player, done = 0, winner;
	unsigned(*makeMove)(unsigned, struct shared*) =
		flags & 4 ? choosePos : readPos;

	/* What number are we */
	puts("Waiting for server to be ready...");

	down(SEM_INIT);
	player = shared->data;
	shared->data = getpid();
	up(SEM_SERVER);

	printf("You are player %s\nWaiting for game to start...\n",
	       player_numbers[player]);

	/* Initialize terminal */
	if (!(flags & FL_CLIENT_NCP)) {
		terminal_init();
	}

	/* Recieve messages till it's not end of game */
	do {
		down(SEM_PLAYERS + player); /* Wait for message */

		switch (shared->message) {
		case MSG_READ_MAP:          /* Update map */
			if (!(flags & FL_DAEMONIZE)) {
				printMap(player, shared->map);
			}
			break;

		case MSG_MAKE_MOVE:         /* Read move from user */
			shared->data = makeMove(player, shared);
			break;

		case MSG_GAME_OVER:         /* Game ends */
			winner = shared->data;
			done = 1;
			break;
		}

		up(SEM_SERVER);             /* Let server know we've read the msg */
	} while (!done);

	/* Print result */
	if (winner == player) {
		puts("\33[1;42mYou win!\33[0m");
	} else if (winner == 2) {
		puts("\33[1mNo one wins.\33[0m");
	} else {
		puts("\33[1;31mYou lose!\33[0m");
	}

	return 0;
}


/* Marks to use when printing the map */
static const char *const marks[2][3] = {
	{
		"\33[1;32mX\33[0m", /* Player one : green X */
		"\33[1;36mO\33[0m", /* Player two : cyan  O */
		"\33[1;30m.\33[0m", /* Empty space: gray  . */
	},
	{
		"\33[5;1;32;44mX\33[0m", /* Player one : green X */
		"\33[5;1;36;44mO\33[0m", /* Player two : cyan  O */
		"\33[5;1;30;44m.\33[0m", /* Empty space: gray  . */
	}
};

/* Print the map */
static void     printMap(unsigned player, unsigned char *map) {
	static int cleared = 0;
	unsigned pos = 0;

	if (!cleared) {
		printf("\33[2J");
		cleared = 1;
	}
	puts("\33[1;1H  A B C D E F G H");
	while (pos < 64) {
		int x;
		printf("%c", '1' + (pos >> 3));
		for (x = 8; x; --x, ++pos) {
			printf(" %s", marks[0][map[pos]]);
		}
		printf(" %c\n", '0' + (pos >> 3));
	}

	printf("  A B C D E F G H\n\n\nYou are %s\n\n", marks[0][player]);
}


/* Read move from user */
static void drawPos(unsigned pos, unsigned player, struct shared *shared) {
	const unsigned x = pos & 7;
	const unsigned y = pos >> 3;
	unsigned char tmp_map[64];

	memcpy(tmp_map, shared->map, sizeof tmp_map);
	doMove(pos, player, tmp_map);
	printMap(player, tmp_map);

	printf("\33[%d;%dH%s\n", y + 2, x * 2 + 3, marks[1][tmp_map[pos]]);
}

static int getKey(void) {
	int ch = getc(stdin);
	if (ch != '\33') {
		return ch;
	}

	ch = getc(stdin);
	if (ch == EOF) {
		return EOF;
	} else if (ch != '[') {
		return ch << 8;
	}

	ch = getc(stdin);
	return ch != EOF ? ch << 16 : EOF;
}

static unsigned readPos(unsigned player, struct shared *shared) {
	static unsigned pos = 0;
	(void)player;

	for(;;) {
		unsigned prevPos = pos;
		drawPos(pos, player, shared);
		do {
			int ch = getKey();
			if (ch == EOF) exit(1);

			switch (ch & ~('a' ^ 'A')) {
				/* Return */
			case '\n':
			case '\r':
			case ' ':
				printMap(player, shared->map);
				return pos;

				/* Up */
			case '8':
			case 'w':
			case 'A' << 16:
			case 'p':
			case 'p' & 31:
			case 'k':
				pos -= 8;
				break;

				/* Down */
			case '2':
			case 's':
			case 'B' << 16:
			case 'n':
			case 'n' & 31:
			case 'j':
				pos += 8;
				break;

				/* Left */
			case '4':
			case 'a':
			case 'D' << 16:
			case 'b':
			case 'b' & 31:
			case 'h':
				--pos;
				break;

				/* Right */
			case '6':
			case 'd':
			case 'C' << 16:
			case 'f':
			case 'f' & 31:
			case 'l':
				++pos;
				break;
			}
			pos &= 63;
		} while (prevPos == pos);
	}
}


/* Choose position at random */
static unsigned choosePos(unsigned player, struct shared *shared) {
	unsigned char pos[64];
	unsigned i;

	for (i = 0; i < 64; ++i) {
		pos[i] = i;
	}

	i = 64;
	do {
		unsigned p = rand() % i;
		--i;
		if (p != i) {
			unsigned char tmp = pos[i];
			pos[i] = pos[p];
			pos[p] = tmp;
		}
	} while (i);

	i = 64;
	do {
		--i;
		if (doDirections(pos[i], player, shared->map, 0)) {
			return pos[i];
		}
	} while (i);

	exit(1);
}


static struct termios stored_settings;

static void terminal_init(void) {
	struct termios new_settings;

	tcgetattr(0,&stored_settings);
	new_settings = stored_settings;

	/* Disable canonical mode and echo, set buffer size to 1 byte */
	new_settings.c_lflag     &= ~(ICANON | ECHO);
	new_settings.c_cc[VTIME]  = 0;
	new_settings.c_cc[VMIN]   = 1;

	atexit(terminal_done);
	tcsetattr(0, TCSANOW, &new_settings);
}

static void terminal_done(void) {
	tcsetattr(0,TCSANOW,&stored_settings);
}
