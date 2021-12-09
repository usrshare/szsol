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

#define GAMENAME "kabusol"

char animdash[4] = {'-','/','|','\\'};

bool won_game = 0;
unsigned int wincount = 0;

enum difficulty_levels {
	D_EASY = 0,
	D_NORMAL = 1,
	D_HARD = 2,
	D_EXPERT = 3,
	D_COUNT = 4
};

enum difficulty_levels cur_diff = D_EASY;
enum difficulty_levels next_diff = D_EASY;

const char* difficulty_names[D_COUNT] = {
	"Easy", "Normal", "Hard", "Expert"
};

int curtheme = 0;

int seed = 0;

bool use_automoves = true;

enum cpairs {
	CPAIR_NONE = 0,
	CPAIR_DEFAULT,
	CPAIR_BORDER,
	CPAIR_CARD,
	CPAIR_BLACK,
	CPAIR_RED,
	CPAIR_SEL_BLACK,
	CPAIR_SEL_RED,
	CPAIR_LABEL,
	CPAIR_INFO,
	CPAIR_WARNING,
	CPAIR_ERROR,
	CPAIR_COUNT
};

struct themeinfo {
	int colors[CPAIR_COUNT][2];
	int card_attr;
	int cardborder_attr;
	int selected_attr;
};

struct themeinfo themes[] = {
	{
		.colors = {
			{COLOR_WHITE,COLOR_BLACK}, //default
			{COLOR_WHITE,COLOR_BLACK}, //border
			{8+COLOR_WHITE,COLOR_BLACK}, //default card
			{8+COLOR_WHITE,COLOR_BLACK}, //black card
			{8+COLOR_RED,COLOR_BLACK}, //red card
			{COLOR_BLACK,8+COLOR_WHITE}, //selected black card
			{8+COLOR_WHITE,COLOR_RED}, //selected red card
			{COLOR_CYAN,COLOR_BLACK}, //labels
			{8+COLOR_WHITE,COLOR_BLUE}, //info statusbar
			{8+COLOR_WHITE,COLOR_YELLOW}, //question statusbar
			{8+COLOR_WHITE,COLOR_RED}, //error statusbar
		},
		.card_attr = A_BOLD,
		.cardborder_attr = 0,
		.selected_attr = A_BOLD,
	},
	{
		.colors = {
			{COLOR_WHITE,COLOR_CYAN}, //default
			{COLOR_BLACK,COLOR_CYAN}, //border
			{8+COLOR_BLACK,COLOR_WHITE}, //default card
			{8+COLOR_BLACK,COLOR_WHITE}, //black card
			{8+COLOR_RED,COLOR_WHITE}, //red card
			{COLOR_WHITE,8+COLOR_BLACK}, //selected black card
			{8+COLOR_WHITE,COLOR_RED}, //selected red card
			{COLOR_BLACK,COLOR_CYAN}, //labels
			{8+COLOR_WHITE,COLOR_BLUE}, //info statusbar
			{8+COLOR_WHITE,COLOR_YELLOW}, //question statusbar
			{8+COLOR_WHITE,COLOR_RED}, //error statusbar
		},
		.card_attr = 0,
		.cardborder_attr = A_BOLD,
		.selected_attr = A_BOLD,
	},
};

#define COLOR(card) ((card / 4) == 0)
#define VALUE(card) (card / 4)

#define C_EMPTY -1
#define C_STACK -2
#define C_LOCKED -3

#define ROWCOUNT 12

int rows[ROWCOUNT] = {
	C_EMPTY,C_EMPTY,C_EMPTY,C_EMPTY,C_EMPTY,C_EMPTY,C_EMPTY,C_EMPTY, //tableau
	C_EMPTY,C_EMPTY,C_EMPTY,C_EMPTY}; //free cell

#define R_FREECELL 8

#define LONGEST_CHAIN 8

int selcard = C_EMPTY;

int dbgmode = 0;
bool showcardnum = false;

const char* cardvals[] = {" 1"," 2"," 3"," 4"," 5"," 6"," 7"," 8"," 9","10"};

const char* rowkeys =    NULL;
const char* qwertykeys = "wertyuio2345";
const char* qwertzkeys = "wertzuio2345";
const char* azertykeys = "zertyuio2345";

unsigned int set_win_count(unsigned int value) {
	char* homedir = getenv("HOME");
	if (!homedir) return 0;

	char configname[PATH_MAX];
	strcpy(configname,homedir);
	strcat(configname,"/." GAMENAME);

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
	strcat(configname,"/." GAMENAME "/wincount");

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

struct cardll cards[NUMCARDS];

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
	if (top == C_STACK) return false;
	if (bot == C_EMPTY) return false;
	if (bot == C_STACK) return false;
	if (top == C_LOCKED) return false;
	if (bot == C_LOCKED) return false;

	return (VALUE(top) == VALUE(bot));
}

int find_parent(int card) {
	for (int i=0; i < R_FREECELL; i++) {
		int curcard = rows[i];
		if (curcard == card) return C_EMPTY;
		if (curcard < 0) continue;  // if the row is empty or starts with a card that ends a stack, skip it
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

	if ((card < 0)) return false;

	int pcard = find_parent(card);
	if (pcard != C_EMPTY) cards[pcard].next = C_EMPTY;
	for (int i=0; i< ROWCOUNT; i++)
		if (rows[i] == card) rows[i] = C_EMPTY;
	return true;
}

bool remove_card_stack(int card) {

	if ((card < 0)) return false;

	if (cards[card].next != C_EMPTY) 
		remove_card_stack(cards[card].next);

	return remove_card(card);

}

bool move_is_legal(int oldrow, int movecard, int newrow) {

	if (oldrow == C_EMPTY) return false;
	if (oldrow >= ROWCOUNT) return false; 
	if (movecard < 0) return false;

	if ((newrow >= R_FREECELL) && (newrow < ROWCOUNT) && (rows[newrow] != C_EMPTY)) {
		// you can only move cards onto another cards on free cells if you drop 3 stackable cards onto one to make a full stack
		if (!card_can_be_stacked(lastcard(newrow),movecard)) return false; //card must be stackable
		if (stacklen(movecard) != 3) return false; //can only move 3 cards onto a non-empty stackable card
		return true;
	}

	if ((newrow >= R_FREECELL) && (cards[movecard].next != C_EMPTY)) return (stacklen(movecard) == 4); 
	  //can't move more than one card onto a free cell, flower row or foundation, unless you're moving 4 stackable cards to make a full stack
	if ((newrow < R_FREECELL) && (!card_can_be_stacked(lastcard(newrow),movecard)) ) return false; //can only move stackable cards between tableau rows

	return true;
}

bool move_card(int oldrow, int movecard, int newrow) {

	if (!move_is_legal(oldrow,movecard,newrow)) return false;

	//at this point, all moves are legal, so let's remove the card from its old location

	int pcard = find_parent(movecard);
	if (pcard != C_EMPTY) cards[pcard].next = C_EMPTY;
	if (rows[oldrow] == movecard) rows[oldrow] = C_EMPTY;

	//and put it into the new one

	int lcard = lastcard(newrow);
	if ( lcard == C_EMPTY ) {

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
	if (row < R_FREECELL) { xpos = 11 + row * 8; ypos = 6; }
	if ((row >= R_FREECELL)) { xpos = 29 + (row-R_FREECELL) * 8; ypos = 3; }
	for (int iy=0; iy < num; iy++)
		mvwhline(screen,ypos+offset+iy,xpos,' ',5);
	wrefresh(screen);
	return 0;
}

void draw_card(int ccard, int xpos, int ypos) {

	bool selected = ((selcard != C_EMPTY) && (selcard == ccard));
	int suit_attr = ((selected ? 0 : themes[curtheme].card_attr) | COLOR_PAIR ( selected ? CPAIR_SEL_BLACK + COLOR(ccard) : CPAIR_BLACK + COLOR(ccard) ) );
	if (ccard < 0) suit_attr = COLOR_PAIR(CPAIR_LABEL);

	//draw the border

	if (selected) wattron(screen, themes[curtheme].selected_attr | suit_attr);
	else if (ccard == C_EMPTY) wattron(screen, COLOR_PAIR(CPAIR_LABEL));
	else wattron(screen,themes[curtheme].cardborder_attr | COLOR_PAIR(CPAIR_CARD));
	mvwprintw(screen,ypos,xpos,"[   ]");
	if (ccard == C_EMPTY) wattroff(screen, COLOR_PAIR(CPAIR_LABEL));
	else if (selected) wattroff(screen, themes[curtheme].selected_attr | suit_attr);
	else wattroff(screen,themes[curtheme].cardborder_attr | COLOR_PAIR(CPAIR_CARD));

	//draw the card itself

	if (ccard == C_EMPTY) return;
	wattron(screen, (selected ? themes[curtheme].selected_attr : 0) | suit_attr);

	if (showcardnum) {
		mvwprintw(screen,ypos,xpos+1,"%3d",ccard);
	} else {

		if (ccard >= 0) {
			mvwprintw(screen,ypos,xpos+1,"%s ",cardvals[VALUE(ccard)]);
		} else if (ccard == C_STACK) {
			mvwprintw(screen,ypos,xpos+1,"-*-");
		} else if (ccard == C_LOCKED) {
			mvwprintw(screen,ypos,xpos+1,"###");
		}
	}

	wattroff(screen, (selected ? themes[curtheme].selected_attr : 0) | suit_attr);

}

int find_freecell() {
	if (rows[R_FREECELL] == C_EMPTY) return R_FREECELL;
	return -1;
}

unsigned int collapse_face_stacks() {
	unsigned int r=0;
	for (int row=0; row < ROWCOUNT; row++) {

		int facecard = rows[row];
		int facevalue = VALUE(rows[row]);
		int facestack = 0;

		while ((facecard >= 0) && (VALUE(facecard) == facevalue)) {
			facecard = cards[facecard].next;
			facestack++;
		}

		if ((facestack == 4) && (facecard == C_EMPTY)) {
			//collapse the card stack
			remove_card_stack(rows[row]);
			clear_row(row, stacklen(rows[row]),4);
			rows[row] = C_STACK;
			//unlock a previously locked free cell, if one is available
			for (int i=R_FREECELL; i<ROWCOUNT; i++) {
				if (rows[i] == C_LOCKED) { rows[i] = C_EMPTY; break; }
			}
			r++;
		}
	}
	return r;
}

unsigned int winning_rows() {
	//find the number of rows that satisfy the victory conditions. if it's 10, then you win!
	unsigned int r = 0;
	for (int row=0; row < ROWCOUNT; row++) {
		if (rows[row] == C_STACK) { r++; continue; }
	}
	return r;
}

void draw_cards() {

	for (int i=0; i < R_FREECELL; i++) {
		wattron(screen, COLOR_PAIR(CPAIR_LABEL));
		mvwaddch(screen, 5, 13 + i*8, rowkeys[i]);
		wattroff(screen, COLOR_PAIR(CPAIR_LABEL));
		int ccard = rows[i];
		int ypos = 0;
		do {
			draw_card(ccard, 11 + i*8, 6 + ypos);
			ypos++;
			if (ccard >= 0) ccard = cards[ccard].next;
		} while (ccard >= 0); 
	}
	
	for (int i=R_FREECELL; i < ROWCOUNT; i++) {
		wattron(screen, COLOR_PAIR(CPAIR_LABEL));
		mvwaddch(screen, 2, 29 + (i-R_FREECELL)*8, rowkeys[i]);
		wattroff(screen, COLOR_PAIR(CPAIR_LABEL));
		int ccard = rows[i];
		int ypos = 0;
		do {
			draw_card(ccard, 27 + (i-R_FREECELL)*8, 3 + ypos);
			ypos++;
			if (ccard >= 0) ccard = cards[ccard].next;
		} while (ccard >= 0); 
	}

	if (collapse_face_stacks()) draw_cards();
	if (winning_rows() == 10) {

		wattron(screen,A_BOLD | COLOR_PAIR(CPAIR_LABEL));
		mvwprintw(screen,13,32,"Y O U   W I N !");
		if (!won_game) { if ((seed >= 0) && (cur_diff == D_EXPERT)) { 
			wincount++; set_win_count(wincount); } won_game = true; }
		wattron(screen,A_BOLD | COLOR_PAIR(CPAIR_LABEL));
	}

	wattron(screen,(won_game ? A_BOLD : 0) | COLOR_PAIR(CPAIR_LABEL));
	mvwprintw(screen,21,68,"%5d win%c",wincount, (wincount != 1) ? 's' : ' ');
	wattroff(screen,(won_game ? A_BOLD : 0) | COLOR_PAIR(CPAIR_LABEL));

	wrefresh(screen);
}


int get_card(int row, int pos, int* o_pos) {

	if (row >= ROWCOUNT) return C_EMPTY;
	if (row >= R_FREECELL) return rows[row];

	int curpos = 0;
	int ccard = rows[row];
	while ((ccard >= 0) && (cards[ccard].next >= 0) && (curpos != pos)) {
		ccard = cards[ccard].next;
		curpos++;
	}
	if (o_pos) *o_pos = curpos;
	if (ccard == C_STACK) return C_EMPTY;
	return ccard;
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

	for (int i=0; i<NUMCARDS; i++) cards[i].next = C_EMPTY;
	for (int i=0; i<ROWCOUNT; i++) rows[i] = C_EMPTY;

	srand(seed);
	int randcards[NUMCARDS];
	for (int i=0; i<NUMCARDS; i++) randcards[i] = (NUMCARDS-1)-i;

	if (seed != -1) {
		for (int i=0; i<(NUMCARDS-1); i++) {
			int j = i + (rand() % (NUMCARDS-i));
			int rc = randcards[i];
			randcards[i] = randcards[j];
			randcards[j] = rc;
		}
	}

	for (int i=0; i < NUMCARDS; i++) {
		add_to_row(i % R_FREECELL, randcards[i]);
	}

	for (int i=0; i < ROWCOUNT; i++) {
		if (stacklen(rows[i]) < oldstacklens[i]) clear_row(i, stacklen(rows[i]), oldstacklens[i] - stacklen(rows[i]));
	}

	for (int i=0; i < cur_diff; i++) {
		rows[ROWCOUNT - 1 - i] = C_LOCKED;
	}

	return 0;
}

int draw_initial_screen(void) {
	wbkgdset(screen, COLOR_PAIR(CPAIR_DEFAULT));
	wclear(screen);
	wattron(screen, COLOR_PAIR(CPAIR_BORDER));
	box(screen,0,0);
	wattroff(screen, COLOR_PAIR(CPAIR_BORDER));
	update_status ("Welcome to " GAMENAME "!", CPAIR_INFO);
	wattron(screen,COLOR_PAIR(CPAIR_LABEL));
	mvwprintw(screen,21,2,"Game #%d (%s)",seed, difficulty_names[cur_diff]);
	wattroff(screen,COLOR_PAIR(CPAIR_LABEL));
	wrefresh(screen);
	return 0;
}

int main(int argc, char** argv) {

	int opt = -1;

	seed = new_seed();

	//int dbgmode = 0;

	while ((opt = getopt(argc, argv, "d:xvn:t:DF")) != -1) {
		switch (opt) {
			case 'd': {
				int dl = atoi(optarg);
				if ((dl >= 0) && (dl < D_COUNT)) {
				  cur_diff = next_diff = dl;
				} 
				break;
			}
			case 'x':
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
			case 't':
				curtheme = atoi(optarg);
				if ( (curtheme < 0) || (curtheme >= ( (sizeof themes) / (sizeof themes[0]) ) ) ) { curtheme = 0; }
				break;
			case 'v':
				printf( 
						"exasol v0.1 -- a quick and dirty ncurses clone of @zachtronics'\n"
						"ПАСЬЯНС, a solitaire game provided with EXAPUNKS\n"
						"\n"
						"Send any bug reports, comments and legal threats regarding this clone to\n"
						"@usr_local_share or at github.com/usrshare/szsol\n");
				return 0;
			case '?':
			default: /* '?' */
				fprintf(stderr, 
"Usage: %s [-d level] [-x] [-D | -F] [-n number] [-v]\n"
"\t-d [level] : Set the difficulty level (from 0 to 3).\n"
"\t-x : Debug mode.\n"
"\t-D : Use the QWERTZ keyboard layout.\n"
"\t-F : Use the AZERTY keyboard layout.\n"
"\t-n [number] : Start with a predetermined card layout based on a seed number. \n"
"\t-v : Version / credits.\n"
"\n"
"How to play:\n"
"The goal is to transform a random arrangement of 40 cards into ten stacks\n"
"consisting of 4 cards of the same value each.\n"
"Cards can be stacked if their values match.\n"
"When four cards of the same value are stacked on the same row, they\n"
"collapse and become unmovable and unstackable.\n"
"When ten collapsed stacks are formed on the tableau and free cells,\n"
"the game is won.\n"
"There are also free cells, which can store one card at any moment.\n"
"In addition, multiple cards can be dropped onto a free cell, but only\n"
"if they will form a stack (4 identical cards on an empty cell or 3\n"
"identical cards on a cell containing the fourth one). If there are\n"
"locked free cells, completing a stack will unlock one.\n"
"\n"
"Controls:\n"
"W,E,R,T,Y,U,I,O -- select tableau rows\n"
"(press multiple times to move more than one card from a row)\n"
"Shift-W,E,R,T,Y,U,I,O -- select highest possible card in that tableau row\n"
"2,3,4,5 -- select one of the free cells\n"
"[space bar] -- remove selection\n"
"D -- change difficulty (applies on next game)\n"
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

	for (int i=0; i < (CPAIR_COUNT-1); i++) {
		init_pair(i+1,themes[curtheme].colors[i][0],themes[curtheme].colors[i][1]);
	}

	if (LINES < 24 || COLS < 80) {
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

	for (int i=0; i<NUMCARDS; i++) cards[i].next = C_EMPTY;
	for (int i=0; i<ROWCOUNT; i++) rows[i] = C_EMPTY;

	//make sure init_board always sees empty rows on the first run.

	init_board(seed);

	int c = 0;

	int selrow = -1;
	int selpos = -1;

	draw_initial_screen();
	draw_cards();

	bool loop = true;

	do {
		if ((selrow == -1) && (selpos == -1) && (selcard == C_EMPTY)) {

		}
		mvwaddch(screen,1,COLS-2,' ');
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
					collapse_face_stacks();
					selrow = -1;
					selpos = -1;
					selcard = C_EMPTY;
				} else if (newcard == C_EMPTY) {
					update_status("Can't select an empty row.", CPAIR_ERROR);
					beep();
				} else if (newcard == C_STACK) {
					update_status("Can't select a full stack.", CPAIR_ERROR);
					beep();
				} else if (newcard == C_LOCKED) {
					update_status("Can't select a locked free cell.", CPAIR_ERROR);
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

		if (c == ' ') {
			update_status ("OK.", CPAIR_INFO);
			selrow = -1; selpos = -1; selcard = -1;
		}

                if ((c == 'd')) {
			next_diff = (next_diff + 1) % 4;
			char status[256];
			snprintf(status,255,"Next game will be at %s difficulty (%d locked free cells).",difficulty_names[next_diff],next_diff);
			update_status (status, CPAIR_INFO);
		}

		if ((c == 'n') && (dbgmode == 1)) showcardnum = !showcardnum;

		if ((c == 'X') || (c == KEY_F(10) )) { if (yes_or_no("Quit?", CPAIR_WARNING)) loop = false; } 

		if ( ((rowkeys == qwertykeys) && (c == 'z')) || (c == KEY_F(3)) ) {

			if (yes_or_no("Restart this game?", CPAIR_WARNING)) { 
				selrow = -1; selpos = -1; selcard = -1; 
				init_board(seed); 
				draw_initial_screen(); 
				if (won_game) update_status("Winning this exact game once again won't increase your win count.", CPAIR_WARNING); } 
		}

		if ( (c == 'n') || (c == KEY_F(2)) ) {
			if (yes_or_no("Start a new game?", CPAIR_WARNING)) { 
				won_game = 0; seed = new_seed(); selrow = -1; selpos = -1; selcard = -1; cur_diff = next_diff; init_board(seed); 
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

