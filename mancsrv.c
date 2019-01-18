#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int port = 3000;
int listenfd;

#define MAXNAME 80  /* maximum permitted name size, not including \0 */

#define NPITS 6  /* number of pits on a side, not including the end pit */

struct player {
    int fd;
    struct in_addr ipaddr;
    char name[MAXNAME + 1];  /* zero-terminated even if partial */
    int gotname;  /* boolean -- did we get the complete name? */
    int pits[NPITS+1];
    struct player *next;
} *playerlist = NULL;
struct player *whoseturn = NULL;

extern void parseargs(int argc, char **argv);
extern void makelistener();
extern void newclient(int fd, struct sockaddr_in *r);
extern void clientactivity(struct player *p);
extern void removeclient(struct player *p);
extern int compute_average_pebbles();
extern void usermove(struct player *p, int move);
extern void yourturn(struct player *p);
extern void showstate(struct player *who);  /* show the game state; NULL means send to everyone */
extern int game_is_over();  /* boolean */
extern struct player *findname(char *name);
extern void broadcast(char *s, struct player *except);
extern void send_string(int fd, char *s);


int main(int argc, char **argv)
{
    int maxfd;
    struct player *p, *n;
    char msg[MAXNAME + 50];
    fd_set fds;

    parseargs(argc, argv);
    makelistener();

    while (!game_is_over()) {
	FD_ZERO(&fds);
	FD_SET(listenfd, &fds);
	maxfd = listenfd;
	for (p = playerlist; p; p = p->next) {
	    FD_SET(p->fd, &fds);
	    if (p->fd > maxfd)
		maxfd = p->fd;
	}
	if (select(maxfd+1, &fds, NULL, NULL, NULL) < 0) {
	    perror("select");
	} else {
	    if (FD_ISSET(listenfd, &fds)) {
		struct sockaddr_in r;
		socklen_t len = sizeof r;
		int clientfd;
		if ((clientfd = accept(listenfd, (struct sockaddr *)&r, &len)) < 0)
		    perror("accept");
		else
		    newclient(clientfd, &r);
	    }
	    for (p = playerlist; p; p = n) {
		n = p->next;  /* stash the pointer now in case 'p' is deleted */
		if (FD_ISSET(p->fd, &fds))
		    clientactivity(p);
	    }
	}
    }

    broadcast("Game over!\r\n", NULL);
    printf("Game over!\n");
    broadcast("Final game state:\r\n", NULL);
    showstate(NULL);
    for (p = playerlist; p; p = p->next) {
	if (p->gotname) {
	    int points, i;
	    for (points = i = 0; i <= NPITS; i++)
		points += p->pits[i];
	    printf("%s has %d points\r\n", p->name, points);
	    snprintf(msg, sizeof msg, "%s has %d points\r\n", p->name, points);
	    broadcast(msg, NULL);
	}
    }

    return(0);
}


void parseargs(int argc, char **argv)
{
    int c, status = 0;
    while ((c = getopt(argc, argv, "p:")) != EOF) {
	switch (c) {
	case 'p':
	    port = atoi(optarg);
	    break;
	default:
	    status++;
	}
    }
    if (status || optind != argc) {
	fprintf(stderr, "usage: %s [-p port]\n", argv[0]);
	exit(1);
    }
}


void makelistener()
{
    struct sockaddr_in r;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	perror("socket");
	exit(1);
    }

    memset(&r, '\0', sizeof r);
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
	perror("bind");
	exit(1);
    };

    if (listen(listenfd, 5)) {
	perror("listen");
	exit(1);
    }
}


void newclient(int fd, struct sockaddr_in *r)
{
    struct player *p;
    static char welcome[] = "Welcome to Mancala.  What is your name?\r\n";
    int newpebbles, i;

    printf("connection from %s\n", inet_ntoa(r->sin_addr));

    int len = strlen(welcome);
    if (write(fd, welcome, len) != len) {
	close(fd);
	printf("%s didn't even listen to the greeting!\n",
		inet_ntoa(r->sin_addr));
	return;
    }

    newpebbles = compute_average_pebbles();

    if ((p = malloc(sizeof(struct player))) == NULL) {
	/* very unlikely */
	fprintf(stderr, "out of memory in adding new client!\n");
	close(fd);
	return;
    }

    p->fd = fd;
    p->ipaddr = r->sin_addr;
    p->name[0] = '\0';
    p->gotname = 0;
    for (i = 0; i < NPITS; i++)
	p->pits[i] = newpebbles;
    p->pits[NPITS] = 0;
    p->next = playerlist;
    playerlist = p;
}


void clientactivity(struct player *p)
{
    int len;
    char buf[100];
    char *q;

    if (p->gotname) {

	/* making a move */
	if ((len = read(p->fd, buf, sizeof buf - 1)) < 0)
	    perror("read");
	if (len <= 0) {
	    removeclient(p);
	    return;
	}
	buf[len] = '\0';
	for (q = buf; *q && isspace(*q); q++)
	    ;
	/* here we make use of the fact that isdigit('\0') is false */
	if (isdigit(*q)) {
	    if (p != whoseturn)
		send_string(p->fd, "It is not your move.\r\n");
	    else
		usermove(p, atoi(buf));
	}

    } else {

	char msg[MAXNAME + 50];

	/* they're sending [more of] their name */
	int prevbytes = strlen(p->name);
	if (prevbytes == sizeof(p->name) - 1) {
	    printf("client %s sends too many bytes for the name; removing\n",
		    inet_ntoa(p->ipaddr));
	    removeclient(p);
	    return;
	}
	if ((len = read(p->fd, p->name + prevbytes, sizeof p->name - prevbytes - 1)) < 0)
	    perror("read");
	if (len <= 0) {
	    removeclient(p);
	    return;
	}
	p->name[prevbytes + len] = '\0';
	if ((q = strchr(p->name, '\r')) || (q = strchr(p->name, '\n'))) {
	    *q = '\0';

	    if (!p->name[0]) {
		printf("rejecting empty name from %s\n", inet_ntoa(p->ipaddr));
		send_string(p->fd, "What is your name?\r\n");
	    } else if (findname(p->name)) {
		printf("rejecting duplicate name \"%s\" from %s\n",
			p->name, inet_ntoa(p->ipaddr));
		p->name[0] = '\0';
		send_string(p->fd, "Sorry, someone else already has that name.  Please choose another.\r\n\r\nWelcome to Mancala.  What is your name?\r\n");
	    } else {
		p->gotname = 1;
		printf("%s's name is set to %s\n",
			inet_ntoa(p->ipaddr), p->name);

		/* tell everyone that this person has joined */
		snprintf(msg, sizeof msg, "%s has joined the game.\r\n",
			p->name);
		broadcast(msg, NULL);

		/* tell this user the game state and whose move it is */
		showstate(p);
		if (whoseturn) {
		    snprintf(msg, sizeof msg, "It is %s's turn.\r\n",
			     whoseturn->name);
		    send_string(p->fd, msg);
		} else {
		    whoseturn = p;
		    send_string(p->fd, "Your move?\r\n");
		}
	    }
	}
    }
}


void removeclient(struct player *p)
{
    struct player **pp, *q, *newwhoseturn;
    int change_whose_turn = 0;
    char msg[MAXNAME + 100];

    printf("disconnecting client %s\n", inet_ntoa(p->ipaddr));
    close(p->fd);

    /* if it was their turn, now it's someone else's */
    if (p == whoseturn) {
	newwhoseturn = p->next;  /* if this is NULL, yourturn() will go to the top of the list, as desired */
	/*
	 * but don't call yourturn() yet because it sends output, whereas
	 * we want to send "%s has left the game" first
	 */
	change_whose_turn = 1;
    }

    if (p->name[0])
	snprintf(msg, sizeof msg, "%s has left the game.\r\n", p->name);
    else
	msg[0] = '\0';

    /* remove */
    for (pp = &playerlist; *pp && *pp != p; pp = &((*pp)->next))
	;
    if (*pp == NULL) {
	fprintf(stderr, "very odd -- I can't find that client\n");
	return;
    }
    *pp = (*pp)->next;
    free(p);

    /* tell everyone that this person has quit */
    if (msg[0]) {
	for (q = playerlist; q; q = q->next)
	    if (q->gotname)
		send_string(q->fd, msg);
    }

    /* maybe change whose turn it is */
    if (change_whose_turn)
	yourturn(newwhoseturn);
}


int compute_average_pebbles()  /* call this BEFORE linking the new player in to the list */
{
    struct player *p;
    int i;

    if (playerlist == NULL)
	return(4);

    int nplayers = 0, npebbles = 0;
    for (p = playerlist; p; p = p->next) {
	nplayers++;
	for (i = 0; i < NPITS; i++)
	    npebbles += p->pits[i];
    }
    return((npebbles - 1) / nplayers / NPITS + 1);  /* round up */
}


void usermove(struct player *p, int pitnum)
{
    struct player *where;
    char msg[MAXNAME + 100];
    int npebbles;

    if (pitnum < 0 || pitnum >= NPITS) {
	send_string(p->fd, "Pit numbers you can play go from 0 to 5.  Try again.\r\n");
	return;
    }
    if (p->pits[pitnum] == 0) {
	snprintf(msg, sizeof msg, "Your pit number %d is empty.  Try again.\r\n", pitnum);
	send_string(p->fd, msg);
	return;
    }

    npebbles = p->pits[pitnum];
    printf("%s takes %d pebbles from pit %d\n", p->name, npebbles, pitnum);
    snprintf(msg, sizeof msg, "You take %d pebbles from pit %d.\r\n",
	    npebbles, pitnum);
    send_string(p->fd, msg);
    snprintf(msg, sizeof msg, "%s takes %d pebbles from pit %d.\r\n",
	    p->name, npebbles, pitnum);
    broadcast(msg, p);

    where = p;
    p->pits[pitnum++] = 0;
    while (npebbles) {
	if (pitnum == NPITS) {
	    if (where == p) {
		where->pits[pitnum]++;
		npebbles--;
		if (npebbles == 0) {
		    printf("%s put the last pebble in their end pit, so they get to go again.\n", p->name);
		    snprintf(msg, sizeof msg, "%s put the last pebble in their end pit, so they get to go again.\r\n", p->name);
		    broadcast(msg, p);
		    send_string(p->fd, "You put the last pebble in your end pit, so you get to go again.\r\n");
		    showstate(p);
		    send_string(p->fd, "Your move?\r\n");
		    return;
		}
	    }
	    if ((where = where->next) == NULL)
		where = playerlist;
	    pitnum = 0;
	} else {
	    where->pits[pitnum++]++;
	    npebbles--;
	}
    }
    yourturn(p->next);
}


void yourturn(struct player *p)
{
    char msg[MAXNAME + 30];
    if ((whoseturn = p) == NULL)
	whoseturn = playerlist;

    /* but it can't be your turn if you haven't completed your name yet */
    while (whoseturn && !whoseturn->gotname)
	whoseturn = whoseturn->next;
    /* if we reached the end, let's look for the FIRST person who's completed */
    while (whoseturn && !whoseturn->gotname)
	whoseturn = whoseturn->next;
    /* ... but if we reached the end AGAIN, it's no one's turn. */

    /* Now, the main point of this function: */
    if (whoseturn) {
	printf("%s's turn\n", whoseturn->name);
	showstate(NULL);
	send_string(whoseturn->fd, "Your move?\r\n");
	snprintf(msg, sizeof msg, "It is %s's move.\r\n", whoseturn->name);
	broadcast(msg, whoseturn);
    } else {
	printf("No one's turn\n");
    }
}


void showstate(struct player *who)  /* show the game state; NULL means send to everyone */
{
    /*
     * Put each line in a big string array before write()ing, to do fewer
     * system calls.
     */
    /* an int takes fewer than 25 characters to represent as a string */
    char msg[MAXNAME + 2 + (4+25) * NPITS + 8 + 25];
    struct player *p, *q;

    for (p = playerlist; p; p = p->next) {
	if (p->gotname) {
	    snprintf(msg, sizeof msg,
		    "%s: [0]%d [1]%d [2]%d [3]%d [4]%d [5]%d [end pit]%d\r\n",
		    p->name, p->pits[0], p->pits[1], p->pits[2], p->pits[3],
		    p->pits[4], p->pits[5], p->pits[6]);
	    if (who)
		send_string(who->fd, msg);
	    else
		for (q = playerlist; q; q = q->next)
		    send_string(q->fd, msg);
	}
    }
}


int game_is_over() /* boolean */
{
    struct player *p;
    int i;
    if (!playerlist)
	return(0);  /* we haven't even started yet! */
    for (p = playerlist; p; p = p->next) {
	int is_all_empty = 1;
	for (i = 0; i < NPITS; i++)
	    if (p->pits[i])
		is_all_empty = 0;
	if (is_all_empty)
	    return(1);
    }
    return(0);
}


struct player *findname(char *name)
{
    struct player *p;
    for (p = playerlist;
	    p && (!p->gotname || strcasecmp(p->name, name));
	    p = p->next)
	;
    return(p);
}


void broadcast(char *s, struct player *except)
{
    int len = strlen(s);
    struct player *p;
    for (p = playerlist; p; p = p->next)
	if (p->gotname && p != except)
	    if (write(p->fd, s, len) != len)
		perror("write");
}


void send_string(int fd, char *s)
{
    int len = strlen(s);
    if (write(fd, s, len) != len)
	perror("write");
}

