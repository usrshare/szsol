#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>

#define NUMCARDS 40

bool won_game = 0;
unsigned int wincount = 0;

enum cpairs {
	CPAIR_DEFAULT = 0,
	CPAIR_RED,
	CPAIR_GREEN,
	CPAIR_BLUE,
	CPAIR_MAGENTA,
};
int rows[15] = {
	-1,-1,-1,-1,-1,-1,-1,-1, //tableau
	-1,-1,-1, //free cells
	-1, //flower area
	-1,-1,-1}; //foundations

int selcard = -1;

#define NINES 24
#define FIRSTDRAGON 27
#define FLOWER 39
#define DRAGONSTACK -2

const char* cardvals = "123456789****@";
const char* cardsuits = "OIX";
const char* rowkeys =    "wertyuio123@890";
const char* qwertzkeys = "wertzuio123@890";
const char* azertykeys = "zertyuio123@890";


unsigned int set_win_count(unsigned int value) {
	char* homedir = getenv("HOME");
	if (!homedir) return 0;
	
	char configname[PATH_MAX];
	strcpy(configname,homedir);
	strcat(configname,"/.szsol");

	struct stat configdir;
	int r = stat(configname,&configdir);
	if (r == -1) {

		if (errno == ENOENT) {
			r = mkdir(configname, 0755);
			if (r == -1) return 0;
		}
	} else {
	
		if (!(S_ISDIR(configdir.st_mode))) return 0;
	}

	strcat(configname,"/wincount");

	FILE* cffile = fopen(configname,"wb");
	if (!cffile) return 0;

	r = fwrite(&value,4,1,cffile);
	fclose(cffile);
	if (r == 0) return 0; else return wincount;

}
unsigned int get_win_count() {

	char* homedir = getenv("HOME");
	if (!homedir) return 0;

	char configname[PATH_MAX];
	strcpy(configname,homedir);
	strcat(configname,"/.szsol/wincount");

	FILE* cffile = fopen(configname,"rb");
        if (!cffile) return 0;

	unsigned int value = 0;
	int r = fread(&value,4,1,cffile);
	fclose(cffile);
	if (r == 0) return 0; else return value;
}

WINDOW* screen = NULL;

struct cardll {
	int next;
};

struct cardll cards[40];

int lastcard(int row) {
	int ccard = rows[row];
	while ((ccard >= 0) && (cards[ccard].next != -1))
		ccard = cards[ccard].next;
	return ccard;
}

int add_to_pile(int pile, int newcard) {

	int ccard = lastcard(pile);
	if (ccard == -1) rows[pile] = newcard; else cards[ccard].next = newcard;
	return 0;
};

bool card_can_be_stacked(int top, int bot) {

	if (top == -1) return true;
	if (bot == -1) return false;
	if (top >= FIRSTDRAGON) return false;
	if (bot >= FIRSTDRAGON) return false;
	if ( ((top % 3) != (bot % 3)) && ((top / 3) == (1 + (bot / 3)) ) ) return true;
	return false;
}

int find_parent(int card) {
	for (int i=0; i < 8; i++) {
		int curcard = rows[i];
		if (curcard == card) return -1;
		while (cards[curcard].next >= 0) {
			if(cards[curcard].next == card) return curcard;
			curcard = cards[curcard].next;
		}
	}
	return -1;
}

int stacklen(int card){
	int x = 0;
	int ccard = card;
	while (ccard != -1) { ccard = cards[ccard].next; x++; }
	return x;
}


bool remove_card(int card) {

	if ((card < 0) || (card > FLOWER)) return false;

	int pcard = find_parent(card);
	if (pcard != -1) cards[pcard].next = -1;
	for (int i=0; i<11; i++)
		if (rows[i] == card) rows[i] = -1;
	return true;
}

bool move_card(int oldrow, int movecard, int newrow) {

	if (oldrow == -1) return false;
	if (oldrow >= 11) return false; //can't move cards from the flower row or foundations
	if (movecard < 0) return false;

	if ((newrow >= 8) && (newrow < 11) && (rows[newrow] != -1)) return false; //can't move cards onto filled free cells, flower rows or foundations
	if ((newrow >= 8) && (cards[movecard].next != -1)) return false; //can't move more than one card onto a free cell, flower row or foundation

	if ((newrow == 11) && (movecard != FLOWER)) return false; //can only move the flower onto the flower row
	if ((newrow < 8) && (!card_can_be_stacked(lastcard(newrow),movecard)) ) return false; //can only move stackable cards between tableau rows
	
	if ( (newrow >= 12) && ( (movecard >= FIRSTDRAGON)) ) return false; //can't move dragons onto foundations
       
	if ( (newrow >= 12) && ( rows[newrow] == -1) && (movecard >= 3) ) return false; // can only move aces onto empty foundations
	if ( (newrow >= 12) && ( rows[newrow] != -1) && (rows[newrow] != (movecard - 3)) ) return false; //can only move next card in same rank onto filled foundations, previous cards in foundations will just be discarded.

	//at this point, all moves are legal, so let's remove the card from its old location

	int pcard = find_parent(movecard);
	if (pcard != -1) cards[pcard].next = -1;
	if (rows[oldrow] == movecard) rows[oldrow] = -1;

	//and put it into the new one

	int lcard = lastcard(newrow);
	if ( (newrow >= 12) || (lcard == -1) ) {

		//empty row or foundation
		rows[newrow] = movecard; return true;

	} else {
		//full row -- find last card and append

		cards[lcard].next = movecard;
		return true;
	}
}

int clear_row(int row, int offset, int num) {

	int xpos = 0;
	int ypos = 0;
	if (row < 8) { xpos = 6 + row * 9; ypos = 5; }
	if ((row >= 8) && (row < 11)) { xpos = 2 + (row-8) * 7; ypos = 2; }
	if (row == 11) { xpos = 38; ypos = 2; }
	if (row >= 12) { xpos = 58 + (row-12) * 7; ypos = 2; }
	for (int iy=0; iy < num; iy++)
		mvwhline(screen,ypos+offset+iy,xpos,' ',5);
	wrefresh(screen);
	return 0;
}

void draw_card(int ccard, int xpos, int ypos) {

	if ((selcard == ccard) && (selcard != -1))
		wattron(screen, A_REVERSE);
	if (ccard == -1) wattron(screen, COLOR_PAIR(CPAIR_MAGENTA));
	mvwprintw(screen,ypos,xpos,"[   ]");
	if (ccard == -1) wattroff(screen, COLOR_PAIR(CPAIR_MAGENTA));
	if (ccard == -1) return;
	wattron(screen,A_BOLD | COLOR_PAIR( ((ccard == FLOWER) || (ccard == DRAGONSTACK)) ? CPAIR_MAGENTA : 1+(ccard%3) ));

	if (ccard == DRAGONSTACK) {
		mvwprintw(screen,ypos,xpos+1,"-*-");
	} else if (ccard < 27) {
		mvwprintw(screen,ypos,xpos+1,"%c%c",cardvals[ccard / 3],cardsuits[ccard % 3]);
	} else if (ccard < FLOWER) {
		mvwprintw(screen,ypos,xpos+1,"-%c-",cardsuits[ccard % 3]);
	} else {
		mvwprintw(screen,ypos,xpos+1," @ ");
	}
	wattroff(screen,A_BOLD | COLOR_PAIR(1+(ccard%3)));

	if ((selcard == ccard) && (selcard != -1))
		wattroff(screen, A_REVERSE);
}

int find_freecell() {
	for (int i=8; i<11; i++) if (rows[i] == -1) return i;
	return -1;
}

int find_foundation(int card) {
	for (int i=12; i < 15; i++) if (rows[i] == card) return i;
	return -1;
}

int exposed_dragons() {
	int dragoncount[3];
	bool dragoncell[3];

	memset(dragoncount, 0, sizeof(int)*3);
	memset(dragoncell, 0, sizeof(bool)*3);

	for (int i=0; i < 11; i++) {
		int lc = lastcard(i);
		if ((lc >= FIRSTDRAGON) && (lc < FLOWER)) {
			dragoncount[lc % 3]++;
			if (i >= 8) dragoncell[lc % 3] = true;
		}
	}

	int result = 0;

	for (int i=0; i < 3; i++) 
		if ( ((find_freecell() != -1) || dragoncell[i]) && (dragoncount[i] == 4) ) result |= (1 << i);

	return result;
}


int remove_exposed_dragons(int suit) {

	for (int i=0; i < 11; i++) {
		if ((lastcard(i) >= FIRSTDRAGON) && (lastcard(i) < FLOWER) && ((lastcard(i)%3 == suit)) ) {
			remove_card(lastcard(i));
			clear_row(i,stacklen(rows[i]),1);
		}
	}
	rows[find_freecell()] = -2;
	return true;
}

void draw_cards() {

	for (int i=0; i < 3; i++) {
		draw_card(rows[8+i], 2 + i*7, 2);
		draw_card(rows[12+i], 58 + i*7, 2);
		wattron(screen, COLOR_PAIR(CPAIR_MAGENTA));
		mvwaddch(screen, 1, 4 + i*7, rowkeys[8+i]);
		mvwaddch(screen, 1, 60 + i*7, rowkeys[12+i]);
		wattroff(screen, COLOR_PAIR(CPAIR_MAGENTA));
	}
	draw_card(rows[11], 38, 2);

	int r = exposed_dragons();
	for (int i=0; i < 3; i++) {
		wattron(screen, COLOR_PAIR(CPAIR_MAGENTA));
		mvwprintw(screen, 1, 23 + (i*4), "%d", 4 + i);
		wattroff(screen, COLOR_PAIR(CPAIR_MAGENTA));
		
		wattron(screen, ( (r & (1 << i)) ? A_BOLD : 0 ) | COLOR_PAIR(i+1));
		mvwprintw(screen, 2, 22 + (i*4), "(%c)", (r & (1 << i)) ? cardsuits[i] : ' ');
		wattroff(screen, ( (r & (1 << i)) ? A_BOLD : 0 ) | COLOR_PAIR(i+1));
	}

	for (int i=0; i < 8; i++) {
		wattron(screen, COLOR_PAIR(CPAIR_MAGENTA));
		mvwaddch(screen, 4, 8 + i*9, rowkeys[i]);
		wattroff(screen, COLOR_PAIR(CPAIR_MAGENTA));
		int ccard = rows[i];
		int ypos = 0;
		do {
			draw_card(ccard, 6 + i*9, 5 + ypos);
			ypos++;
			if (ccard >= 0) ccard = cards[ccard].next;
		} while (ccard >= 0); 
	}

	if ((rows[8] == -2) && (rows[9] == -2) && (rows[10] == -2) && (rows[12] >= NINES) && (rows[13] >= NINES) && (rows[14] >= NINES)) {

			wattron(screen,A_BOLD | COLOR_PAIR(CPAIR_MAGENTA));
			mvwprintw(screen,13,32,"Y O U   W I N !");
			if (!won_game) { wincount++; set_win_count(wincount); won_game = true; }
			wattron(screen,A_BOLD | COLOR_PAIR(CPAIR_MAGENTA));
			}
 
	wattron(screen,(won_game ? A_BOLD : 0) | COLOR_PAIR(CPAIR_MAGENTA));
	mvwprintw(screen,22,68,"%5d win%c",wincount, ((wincount%10 != 1) || (wincount == 11)) ? 's' : ' ');
	wattroff(screen,(won_game ? A_BOLD : 0) | COLOR_PAIR(CPAIR_MAGENTA));

	wrefresh(screen);
}


int get_card(int row, int pos, int* o_pos) {

	if (row >= 15) return -1;
	if (row >= 8) return rows[row];

	int curpos = 0;
	int ccard = rows[row];
	while ((ccard >= 0) && (cards[ccard].next != -1) && (curpos != pos)) {
		ccard = cards[ccard].next;
		curpos++;
	}
	if (o_pos) *o_pos = curpos;
	return ccard;
}

int auto_move(void) {

	for (int i=0; i < 8; i++) {

		int lc = lastcard(i);
		int fc = -1; //stores the appropriate foundation

		if (lc == FLOWER) {
			int r = move_card(i,FLOWER,11);
			if (r) clear_row(i,stacklen(rows[i]),1);
			return r;
		}

		if ( (lc >= 0) && (lc <= 2) && ((fc = find_foundation(-1)) != -1) ) {
			//empty foundation and an ace

			int r = move_card(i,lc,fc);
			if (r) clear_row(i,stacklen(rows[i]),1);
			return r;

		} else if ( (lc >= 3) && (lc < FIRSTDRAGON) && ((fc = find_foundation(lc-3)) != -1) ) {
			bool no_lower_cards = true;

			for (int i=0; i < 3; i++)
				if ((rows[12+i] / 3) < ((lc/3) - 1)) no_lower_cards = false;

			if (no_lower_cards) {
			//a suitable card for the foundation and no lower cards of other suits
			int r = move_card(i,lc,fc);
			if (r) clear_row(i,stacklen(rows[i]),1);
			return r;
			}
		}
	}
	return false;
}



int main(int argc, char** argv) {

	for (int i=0; i<40; i++) cards[i].next = -1;

	int opt = -1;

	//int dbgmode = 0;

	while ((opt = getopt(argc, argv, "dvDF")) != -1) {
		switch (opt) {
			case 'd':
				//dbgmode = 1;
				printf("Press ENTER to start (feel free to attach a debugger to this process at this moment).\n");
				getc(stdin);
				break;
			case 'D':
				rowkeys = qwertzkeys;
				break;
			case 'F':
				rowkeys = azertykeys;
				break;
			case 'v':
				printf( 
"szsol v0.1 -- a quick and dirty ncurses clone of @zachtronics'\n"
"SHENZHEN SOLITAIRE, a solitaire game provided with SHENZHEN I/O\n"
"(also available as a separate game for Windows, Mac OS, Linux and iOS)\n"
"\n"
"Send any bug reports, comments and legal threats regarding this clone to\n"
"@usr_local_share or at github.com/usrshare/szsol\n");
				return 0;
			case '?':
			default: /* '?' */
				fprintf(stderr, 
"Usage: %s [-d] [-v]\n"
"\t-d : Debug mode.\n"
"\t-v : Version / credits.\n"
"\n"
"How to play:\n"
"The goal is to clear the tableau by moving all the cards onto\n"
"the foundations and the free cells.\n\n"
"Cards valued 1 to 9 stack on the foundations by suit in increasing\n"
"order (1->2->3). They can also be stacked on the tableau in decreasing\n"
"order (9->8->7), but only if the suits alternate.\n"
"Dragons (labeled '*') can't be stacked -- but when all four dragons\n"
"of a single color are exposed, they can be moved into a single free cell.\n"
"Otherwise, each free cell can only store one card.\n"
"\n"
"Controls:\n"
"1,2,3 -- select free cells\n"
"W,E,R,T,Y,U,I,O -- select tableau rows\n"
"Shift-W,E,R,T,Y,U,I,O -- select highest possible card in that tableau row\n"
"(press multiple times to move more than one card from a row\n"
"8,9,0 -- select foundations\n"
"[space bar] -- remove selection\n"
"4,5,6 -- move dragons onto a free cell (when available)\n"
"Shift+Q or Ctrl+C -- exit\n",argv[0]);
				return 0;
		}
	}

	initscr();
	cbreak();
	noecho();
	start_color();

	init_pair(CPAIR_BLUE,COLOR_BLUE,COLOR_BLACK);
	init_pair(CPAIR_RED,COLOR_RED,COLOR_BLACK);
	init_pair(CPAIR_GREEN,COLOR_GREEN,COLOR_BLACK);
	init_pair(CPAIR_MAGENTA,COLOR_MAGENTA,COLOR_BLACK);

	screen = newwin(24,80,(LINES-24)/2,(COLS-80) / 2);
	box(screen,0,0);
	wrefresh(screen);

	wincount = get_win_count();

	srand(time(NULL));
	int randcards[NUMCARDS];
	for (int i=0; i<40; i++) randcards[i] = 39-i;
	for (int i=0; i<39; i++) {
		int j = i + (rand() % (40-i));
		int rc = randcards[i];
		randcards[i] = randcards[j];
		randcards[j] = rc;
	}

	for (int i=0; i < 40; i++) {
		add_to_pile(i%8, randcards[i]);
	}

	int c = 0;

	int selrow = -1;
	int selpos = -1;
	
	do {
		if ((selrow == -1) && (selpos == -1) && (selcard == -1)) while (auto_move()) {};
		draw_cards();
		c = wgetch(screen);

		char* x = strchr(rowkeys,(char)c);
		if (x) {
			int newrow = x - rowkeys;
			if (newrow == selrow) {
				//go up in the same row

				int newcard = get_card(selrow,selpos - 1,&selpos);
				if (card_can_be_stacked(newcard,selcard)) selcard = newcard; else {beep(); selrow = -1; selpos = -1; selcard = -1;}
			} else {
				//pick a different row
				int newcard = get_card(newrow, -1, &selpos);
				if (selrow != -1) {
					//move card into that row
					bool r = move_card(selrow,selcard,newrow);
					if (!r) beep();
					if (r) clear_row(selrow,stacklen(rows[selrow]),stacklen(selcard));
					selrow = -1;
					selpos = -1;
					selcard = -1;
				} else {
					selrow = newrow;
					selcard = newcard;
				}
			}
		} else if ((x = strchr(rowkeys,(char)(tolower(c)) )) != NULL) {
			int newrow = x - rowkeys;
			int newcard = get_card(newrow, -1, &selpos);
			int nextcard = -1;

			while ((selpos > 0) && ((nextcard = get_card(newrow, selpos-1,&selpos)) != -1) && (card_can_be_stacked(nextcard,newcard)) ) newcard = nextcard;

			selrow = newrow;
			selcard = newcard;
		}

		if ((c >= '4') && (c <= '6')) {
			int r = exposed_dragons();
			int suit = c - '4';
			if (r && (1 << suit)) remove_exposed_dragons(suit);
			selrow = -1; selpos = -1; selcard = -1;
		}

		if (c == ' ') {
			selrow = -1; selpos = -1; selcard = -1;
		}

	} while (c != 'Q' && c != 27);	

	endwin();
	return 0;
}
