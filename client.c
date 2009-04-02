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
static unsigned readPos  (unsigned player);
static unsigned choosePos(unsigned player);


int runClient(int flags) {
	unsigned player, done = 0, winner;
	unsigned(*makeMove)(unsigned) = flags & 4 ? choosePos : readPos;

	/* What number are we */
	puts("Waiting for server to be ready...");

	down(SEM_INIT);
	player = shared->data;
	shared->data = getpid();
	up(SEM_SERVER);

	printf("You are player %s\nWaiting for game to start...\n",
	       player_numbers[player]);

	/* Initialize terminal */
	if (!(flags & 4)) {
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
			shared->data = makeMove(player);
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
	unsigned pos = 0;
	puts("\33[2J\33[1;1H  A B C D E F G H");
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
static void drawPos(unsigned pos, unsigned player) {
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

static unsigned readPos(unsigned player) {
	static unsigned pos = 0;
	(void)player;

	for(;;) {
		unsigned prevPos = pos;
		drawPos(pos, player);
		do {
			int ch = getKey();
			if (ch == EOF) exit(1);

			switch (ch) {
			case '\n':
			case '\r':
				printMap(player, shared->map);
				return pos;

			case '8':
			case 'w':
			case 'A' << 16:
				if (pos >> 3) pos -= 8;
				break;

			case '2':
			case 's':
			case 'B' << 16:
				if ((pos >> 3) < 7) pos += 8;
			break;

			case '4':
			case 'a':
			case 'D' << 16:
				if (pos) --pos;
			break;

			case '6':
			case 'd':
			case 'C' << 16:
				if (pos != 63) ++pos;
			break;
			}
		} while (prevPos == pos);
	}
}


/* Choose position at random */
static unsigned choosePos(unsigned player) {
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
