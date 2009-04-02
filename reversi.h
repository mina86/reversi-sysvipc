#ifndef MN_REVERSI_H
#define MN_REVERSI_H


/* For inline keyword */
#if defined __cplusplus || __STDC_VERSION__ >= 199901L || defined inline
                                 /* We already have inline */
#elif defined __GNUC__
#  define inline __inline__      /* GCC's alternate keyword */
#else
#  define inline                 /* No inline support */
#endif


/* If cond is met prints error message end exists(1) */
#define DIE(cond, msg) if (cond) { perror(msg); exit(1); } else (void)0


/* Possible flags */
enum {
	FL_SERVER    = 1,
	FL_DAEMONIZE = 2
};


/* Pointer to shared memory */
extern struct shared {
	unsigned char map[8 * 8];

	enum {
		MSG_MAKE_MOVE,
		MSG_READ_MAP,
		MSG_GAME_OVER
	} message;

	unsigned data;
} *shared;

/* Server and client codes run from main() */
int runServer(int flags);
int runClient(int flags);


/* Semaphores numbers */
enum {
	SEM_SERVER,  /* Semaphore server sleeps on */
	SEM_INIT,    /* Semaphore uninitialized client sleeps on */
	SEM_PLAYERS, /* Start of two semaphores players sleep on */
	SEM_PLAYER_0 = SEM_PLAYERS, /* Player one sleeps on */
	SEM_PLAYER_1,               /* Player two sleeps on */
	SEM_COUNT    /* Total number of semaphores used */
};


/* Array where first element is "one", and second "two". */
extern const char *const player_numbers[2];


/* Semaphore operations. */
int sem_do(int num, int op, int timeout);
static inline void down      (int num) {        sem_do(num, -1, 0); }
static inline int  timed_down(int num) { return sem_do(num, -1, 1); }
static inline void up        (int num) {        sem_do(num,  1, 0); }


/* Tries to walk in all directions on the map. */
int  doDirections(const unsigned pos, const unsigned player,
                  unsigned char *map,
                  void(*callback)(unsigned pos, int step, unsigned n,
                                  unsigned player, unsigned char *map));
/* Tries to perform a move. */
int  doMove (unsigned pos, unsigned player, unsigned char *map);


#endif
