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

int seed = 0;

enum cpairs {
	CPAIR_DEFAULT = 0,
	CPAIR_RED,
	CPAIR_GREEN,
	CPAIR_BLUE,
	CPAIR_MAGENTA,
	CPAIR_INFO,
	CPAIR_WARNING,
	CPAIR_ERROR
};
#define NINES 24
#define FIRSTDRAGON 27
#define FLOWER 39
#define C_EMPTY -1
#define C_DRAGONSTACK -2

#define ROWCOUNT 15

int rows[ROWCOUNT] = {
	C_EMPTY,C_EMPTY,C_EMPTY,C_EMPTY,C_EMPTY,C_EMPTY,C_EMPTY,C_EMPTY, //tableau
	C_EMPTY,C_EMPTY,C_EMPTY, //free cells
	C_EMPTY, //flower area
	C_EMPTY,C_EMPTY,C_EMPTY}; //foundations

int selcard = C_EMPTY;


int dbgmode = 0;
bool showcardnum = false;

const char* cardvals = "123456789****@";
const char* cardsuits = "OIX";

const char* rowkeys =    NULL;
const char* qwertykeys = "wertyuio123@890";
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
WINDOW* statusbar = NULL;

struct cardll {
	int next;
};

struct cardll cards[40];

int lastcard(int row) {
	int ccard = rows[row];
	while ((ccard >= 0) && (cards[ccard].next != C_EMPTY))
		ccard = cards[ccard].next;
	return ccard;
}

int add_to_row(int pile, int newcard) {

	int ccard = lastcard(pile);
	if (ccard == C_EMPTY) rows[pile] = newcard; else cards[ccard].next = newcard;
	return 0;
};

bool card_can_be_stacked(int top, int bot) {

	if (top == C_EMPTY) return true;
	if (top == C_DRAGONSTACK) return false;
	if (bot == C_EMPTY) return false;
	if (bot == C_DRAGONSTACK) return false;
	if (top >= FIRSTDRAGON) return false;
	if (bot >= FIRSTDRAGON) return false;
	if ( ((top % 3) != (bot % 3)) && ((top / 3) == (1 + (bot / 3)) ) ) return true;
	return false;
}

int find_parent(int card) {
	for (int i=0; i < 8; i++) {
		int curcard = rows[i];
		if (curcard == card) return C_EMPTY;
		if (curcard == C_EMPTY) continue;  // if the row is empty, skip it
		while (cards[curcard].next >= 0) {
			if(cards[curcard].next == card) return curcard;
			curcard = cards[curcard].next;
		}
	}
	return C_EMPTY;
}

int stacklen(int card){
	int x = 0;
	int ccard = card;
	while (ccard >= 0) { ccard = cards[ccard].next; x++; }
	return x;
}


bool remove_card(int card) {

	if ((card < 0) || (card > FLOWER)) return false;

	int pcard = find_parent(card);
	if (pcard != C_EMPTY) cards[pcard].next = C_EMPTY;
	for (int i=0; i<11; i++)
		if (rows[i] == card) rows[i] = C_EMPTY;
	return true;
}

bool move_card(int oldrow, int movecard, int newrow) {

	if (oldrow == C_EMPTY) return false;
	if (oldrow >= 11) return false; //can't move cards from the flower row or foundations
	if (movecard < 0) return false;

	if ((newrow >= 8) && (newrow < 11) && (rows[newrow] != C_EMPTY)) return false; //can't move cards onto filled free cells, flower rows or foundations
	if ((newrow >= 8) && (cards[movecard].next != C_EMPTY)) return false; //can't move more than one card onto a free cell, flower row or foundation

	if ((newrow == 11) && (movecard != FLOWER)) return false; //can only move the flower onto the flower row
	if ((newrow < 8) && (!card_can_be_stacked(lastcard(newrow),movecard)) ) return false; //can only move stackable cards between tableau rows
	
	if ( (newrow >= 12) && ( (movecard >= FIRSTDRAGON)) ) return false; //can't move dragons onto foundations
       
	if ( (newrow >= 12) && ( rows[newrow] == C_EMPTY) && (movecard >= 3) ) return false; // can only move aces onto empty foundations
	if ( (newrow >= 12) && ( rows[newrow] != C_EMPTY) && (rows[newrow] != (movecard - 3)) ) return false; //can only move next card in same rank onto filled foundations, previous cards in foundations will just be discarded.

	//at this point, all moves are legal, so let's remove the card from its old location

	int pcard = find_parent(movecard);
	if (pcard != C_EMPTY) cards[pcard].next = C_EMPTY;
	if (rows[oldrow] == movecard) rows[oldrow] = C_EMPTY;

	//and put it into the new one

	int lcard = lastcard(newrow);
	if ( (newrow >= 12) || (lcard == C_EMPTY) ) {

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

	bool selected = ((selcard != C_EMPTY) && (selcard == ccard));
	int suit_attr = (A_BOLD | COLOR_PAIR( ((ccard == FLOWER) || (ccard == C_DRAGONSTACK)) ? CPAIR_MAGENTA : 1+(ccard%3)) );

	//draw the border

	if (selected) wattron(screen, A_REVERSE | suit_attr);
	if (ccard == C_EMPTY) wattron(screen, COLOR_PAIR(CPAIR_MAGENTA));
	mvwprintw(screen,ypos,xpos,"[   ]");
	if (ccard == C_EMPTY) wattroff(screen, COLOR_PAIR(CPAIR_MAGENTA));
	if (selected) wattroff(screen, A_REVERSE | suit_attr);
	
	//draw the card itself

	if (ccard == C_EMPTY) return;
	wattron(screen, (selected ? A_REVERSE : 0) | suit_attr);

	if (showcardnum) {
		mvwprintw(screen,ypos,xpos+1,"%3d",ccard);
	} else {

	if (ccard == C_DRAGONSTACK) {
		mvwprintw(screen,ypos,xpos+1,"-*-");
	} else if (ccard < 27) {
		mvwprintw(screen,ypos,xpos+1,"%c%c",cardvals[ccard / 3],cardsuits[ccard % 3]);
	} else if (ccard < FLOWER) {
		mvwprintw(screen,ypos,xpos+1,"-%c-",cardsuits[ccard % 3]);
	} else {
		mvwprintw(screen,ypos,xpos+1," @ ");
	}
	}
	
	wattroff(screen, (selected ? A_REVERSE : 0) | suit_attr);

}

int find_freecell() {
	for (int i=8; i<11; i++) if (rows[i] == C_EMPTY) return i;
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
		int lc = lastcard(i);
		if ((lc >= FIRSTDRAGON) && (lc < FLOWER) && ((lc%3) == suit) ) {
			remove_card(lc);
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
			if (!won_game) { if (seed >= 0) { wincount++; set_win_count(wincount); } won_game = true; }
			wattron(screen,A_BOLD | COLOR_PAIR(CPAIR_MAGENTA));
			}
 
	wattron(screen,(won_game ? A_BOLD : 0) | COLOR_PAIR(CPAIR_MAGENTA));
	mvwprintw(screen,21,68,"%5d win%c",wincount, ((wincount%10 != 1) || (wincount == 11)) ? 's' : ' ');
	wattroff(screen,(won_game ? A_BOLD : 0) | COLOR_PAIR(CPAIR_MAGENTA));

	wrefresh(screen);
}


int get_card(int row, int pos, int* o_pos) {

	if (row >= 15) return C_EMPTY;
	if (row >= 8) return rows[row];

	int curpos = 0;
	int ccard = rows[row];
	while ((ccard >= 0) && (cards[ccard].next != C_EMPTY) && (curpos != pos)) {
		ccard = cards[ccard].next;
		curpos++;
	}
	if (o_pos) *o_pos = curpos;
	return ccard;
}

int auto_move(void) {

	for (int i=0; i < 11; i++) {

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

void update_status (const char* text, int color) {
	wattron(statusbar, A_BOLD | COLOR_PAIR(color));
	wbkgd(statusbar, A_BOLD | COLOR_PAIR(color));
	werase(statusbar);
	mvwaddstr(statusbar, 0, 0, text);
	wattroff(statusbar, A_BOLD | COLOR_PAIR(color));
	wrefresh(statusbar);	
}

bool ask_string (const char* prompt, int color, char* output, unsigned int o_sz) {
	wattron(statusbar, A_BOLD | COLOR_PAIR(color));
	werase(statusbar);
	wbkgd(statusbar, A_BOLD | COLOR_PAIR(color));
	mvwprintw(statusbar, 0, 0, "%s: >", prompt);
	echo();		
	int r = wgetnstr(statusbar,output,o_sz);
	noecho();
	wattroff(statusbar, A_BOLD | COLOR_PAIR(color));	

	if (r == OK) return true; else return false;
}

bool ask_integer (const char* prompt, int color, int* output) {

	char temp[21];
	temp[0] = 0;

	while (true) {

	errno = 0;
	int str = ask_string(prompt,color,temp,20); 	
	
	if (!str) return false;

	char* endptr = temp;
	int num = strtol(temp,&endptr,0);
	if (strlen(temp) == 0) return false;

	if ( (errno != 0) || ((endptr - temp) < strlen(temp)) ) { beep(); } else { *output = num; return true; }

	}
}

bool yes_or_no (const char* text, int color) {
	
	wattron(statusbar, A_BOLD | COLOR_PAIR(color));
	werase(statusbar);
	wbkgd(statusbar, A_BOLD | COLOR_PAIR(color));
	mvwprintw(statusbar, 0, 0, "%s (y/n)? ", text);
	wattroff(statusbar, A_BOLD | COLOR_PAIR(color));	
	int c;
	while (true) {
		c = wgetch(statusbar);
		if (tolower(c) == 'y') { update_status("OK.", CPAIR_INFO); return true; }
		if (tolower(c) == 'n') { update_status("OK.", CPAIR_INFO); return false; }
	}
}

int new_seed (void) {
	unsigned int t = time(NULL);
	return rand_r(&t);
}

int init_board (int seed) {
	
	int oldstacklens[ROWCOUNT];
	for (int i=0; i < ROWCOUNT; i++) oldstacklens[i] = stacklen(rows[i]);
	
	for (int i=0; i<40; i++) cards[i].next = C_EMPTY;
	for (int i=0; i<15; i++) rows[i] = C_EMPTY;

	srand(seed);
	int randcards[NUMCARDS];
	for (int i=0; i<40; i++) randcards[i] = 39-i;
	
	if (seed != -1) {
	for (int i=0; i<39; i++) {
		int j = i + (rand() % (40-i));
		int rc = randcards[i];
		randcards[i] = randcards[j];
		randcards[j] = rc;
	}
	}

	for (int i=0; i < 40; i++) {
		add_to_row(i%8, randcards[i]);
	}

	for (int i=0; i < ROWCOUNT; i++) {
		if (stacklen(rows[i]) < oldstacklens[i]) clear_row(i, stacklen(rows[i]), oldstacklens[i] - stacklen(rows[i]));
	}

	return 0;
}

int draw_initial_screen(void) {
	wclear(screen);
	box(screen,0,0);
	update_status ("Welcome to szsol!", CPAIR_INFO);
	wattron(screen,COLOR_PAIR(CPAIR_MAGENTA));
	mvwprintw(screen,21,2,"Game #%d",seed);
	wattroff(screen,COLOR_PAIR(CPAIR_MAGENTA));
	wrefresh(screen);
	return 0;
}

int main(int argc, char** argv) {

	int opt = -1;

	seed = new_seed();

	//int dbgmode = 0;

	while ((opt = getopt(argc, argv, "dvn:DF")) != -1) {
		switch (opt) {
			case 'd':
				dbgmode = 1;
				printf("Press ENTER to start (feel free to attach a debugger to this process at this moment).\n");
				getc(stdin);
				break;
			case 'D':
				rowkeys = qwertzkeys;
				break;
			case 'F':
				rowkeys = azertykeys;
				break;
			case 'n':
				seed = atoi(optarg);
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
"Usage: %s [-d] [-D | -F] [-n number] [-v]\n"
"\t-d : Debug mode.\n"
"\t-D : Use the QWERTZ keyboard layout.\n"
"\t-F : Use the AZERTY keyboard layout.\n"
"\t-n [number] : Start with a predetermined card layout based on a seed number. \n"
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

	if (rowkeys == NULL) rowkeys = qwertykeys;

	initscr();
#ifdef NCURSES_VERSION
	set_escdelay(100);
#endif
	cbreak();
	noecho();
	start_color();

	init_pair(CPAIR_BLUE,COLOR_BLUE,COLOR_BLACK);
	init_pair(CPAIR_RED,COLOR_RED,COLOR_BLACK);
	init_pair(CPAIR_GREEN,COLOR_GREEN,COLOR_BLACK);
	init_pair(CPAIR_MAGENTA,COLOR_MAGENTA,COLOR_BLACK);
	init_pair(CPAIR_INFO,COLOR_WHITE,COLOR_BLUE);
	init_pair(CPAIR_WARNING,COLOR_WHITE,COLOR_YELLOW);
	init_pair(CPAIR_ERROR,COLOR_WHITE,COLOR_RED);
	
	if (LINES < 24 && COLS < 80) {
		endwin();
		fprintf(stderr, 
			"Terminal window is too small.\n"
			"Minimum terminal window size: 80x24.\n");
		return -1;
	}

	screen = newwin(23,80,(LINES-24)/2,(COLS-80) / 2);
	statusbar = newwin(1,80,(LINES-24)/2 + 23, (COLS-80) / 2);
	
	keypad(screen,true);

	wincount = get_win_count();
	
	for (int i=0; i<40; i++) cards[i].next = C_EMPTY;
	for (int i=0; i<15; i++) rows[i] = C_EMPTY;
	
	//make sure init_board always sees empty rows on the first run.

	init_board(seed);

	int c = 0;

	int selrow = -1;
	int selpos = -1;
	
	draw_initial_screen();
	

	bool loop = true;
	
	do {
		if ((selrow == -1) && (selpos == -1) && (selcard == C_EMPTY)) while (auto_move()) {};
		draw_cards();
		c = wgetch(screen);

		char* x = strchr(rowkeys,(char)c);
		if (x) {
			int newrow = x - rowkeys;
			if (newrow == selrow) {
				//go up in the same row

				int newcard = get_card(selrow,selpos - 1,&selpos);
				if (card_can_be_stacked(newcard,selcard)) selcard = newcard; else { update_status("Can't go further in this row.", CPAIR_ERROR); beep(); selrow = -1; selpos = -1; selcard = C_EMPTY;}
			} else {
				//pick a different row
				int newcard = get_card(newrow, -1, &selpos);
				if (selrow != -1) {
					//move card into that row
					bool r = move_card(selrow,selcard,newrow);
					if (!r) { update_status("Can't move the cards into this row.", CPAIR_ERROR); beep(); }
					if (r) { update_status("OK.", CPAIR_INFO); clear_row(selrow,stacklen(rows[selrow]),stacklen(selcard)); }
					selrow = -1;
					selpos = -1;
					selcard = C_EMPTY;
				} else if (newcard == C_EMPTY) {
					update_status("Can't select an empty row.", CPAIR_ERROR);
					beep();
				} else {
					selrow = newrow;
					selcard = newcard;
				}
			}
		} else if ((x = strchr(rowkeys,(char)(tolower(c)) )) != NULL) {
			int newrow = x - rowkeys;
			int newcard = get_card(newrow, -1, &selpos);
			int nextcard = C_EMPTY;

			int newpos = selpos;

			while ((selpos > 0) && ((nextcard = get_card(newrow, newpos-1,&newpos)) != C_EMPTY) && (card_can_be_stacked(nextcard,newcard)) ) { newcard = nextcard; selpos = newpos; }

			selrow = newrow;
			selcard = newcard;
		}

		if ((c >= '4') && (c <= '6')) {
			int r = exposed_dragons();
			int suit = c - '4';
			if (r & (1 << suit)) {
				remove_exposed_dragons(suit);
				update_status ("Dragons removed.", CPAIR_INFO);
			} else {
				update_status ("Can't remove these dragons yet.", CPAIR_ERROR);
			}
			selrow = -1; selpos = -1; selcard = C_EMPTY;
		}

		if (c == ' ') {
			update_status ("OK.", CPAIR_INFO);
			selrow = -1; selpos = -1; selcard = -1;
		}

		if ((c == 'n') && (dbgmode == 1)) showcardnum = !showcardnum;

		if ((c == 'Q') || (c == 27)) { if (yes_or_no("Quit?", CPAIR_WARNING)) loop = false; } 
		
		if ( ((rowkeys == qwertykeys) && (c == 'z')) || (c == KEY_F(3)) ) {
				
			if (yes_or_no("Restart this game?", CPAIR_WARNING)) { 
				selrow = -1; selpos = -1; selcard = -1; 
				init_board(seed); 
				draw_initial_screen(); 
			if (won_game) update_status("Winning this exact game once again won't increase your win count.", CPAIR_WARNING); } 
		}
		
		if ( (c == 'n') || (c == KEY_F(2)) ) {
			if (yes_or_no("Start a new game?", CPAIR_WARNING)) { 
				won_game = 0; seed = new_seed(); selrow = -1; selpos = -1; selcard = -1; init_board(seed); 
				draw_initial_screen();
			}
		}

		if ( (c == 'N') || (c == KEY_F(4)) ) {
			if (ask_integer("Input game # for a new game or empty string to cancel", CPAIR_WARNING, &seed)) {
			selrow = -1; selpos = -1; selcard = -1;
			init_board(seed);
			draw_initial_screen();
			} else update_status("Continuing existing game.", CPAIR_INFO);	
		}

	} while (loop);	

	endwin();
	return 0;
}
