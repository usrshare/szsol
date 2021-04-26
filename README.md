# szsol, exasol and kabusol - quick, dirty and very unauthorized ncurses clones of Shenzhen Solitaire, ПАСЬЯНС and Kabufuda Solitaire

This package contains three ncurses-based versions of solitaire games originally 
developed by [Zachtronics](https://twitter.com/zachtronics) and distributed
as part of 
[SHENZHEN I/O](http://www.zachtronics.com/shenzhen-io/),
[EXAPUNKS](http://www.zachtronics.com/exapunks/) and
[Eliza](https://www.zachtronics.com/eliza/).

The SHENZHEN I/O solitaire game is also available as a standalone product on [Steam](http://store.steampowered.com/app/570490/SHENZHEN_SOLITAIRE/) and the
[App Store](https://itunes.apple.com/tw/app/shenzhen-solitaire/id1206037778).

![Screenshot of szsol](https://i.imgur.com/AJv0zcs.png)
![Screenshot of exasol](https://i.imgur.com/rvUyv7z.png)

# Compiling

If the `ncursesw` library (version 5) is available, compiling this game should
only require running the `make` command. Otherwise, modifications to the
`Makefile` and source code may be required.

# Running

The game requires a console capable of rendering at least 80 columns and 24
rows. If a larger screen is used, the game will create a box in the center of
the terminal.

The controls are simple: first, you have to select the row from which to move
cards, and then the destination row. 

## How to play szsol

Shenzhen Solitaire (and thus szsol) is played with a card deck of 40 cards, based around a mahjong tileset.

27 of the cards are divided into three suits: circles (also known as tǒngzi or pinzu), bamboos (suǒzi or souzu) and characters (wànzi or manzu). Each of these suits has 9 tiles, with numeric values from 1 to 9.

12 of the cards are called "dragons" and are also divided into three groups. The "red dragon" (zhōng or chun) cards feature a red Han character for "center". (中) The "green dragon" (fācái or hatsu) has a green traditional Chinese character. (發) The "white dragon" (báibǎn or haku) cards feature a border around them.

In szsol, circle cards use the character O, bamboo cards use the character I, and character cards use the character X. A card with a numeric value will be displayed with that value ("6X", "2I", etc.). Dragon cards are displayed as "-r-", "-g-" or "-w-" respectively, based on the first letter of the English name of their color.

There's also a "flower" card, represented in szsol with an "@" symbol, but it has no bearing on gameplay and is removed from play as soon as it becomes possible.

The goal of szsol is to clear the tableau (the central area) by moving all the
cards onto the foundations (top right) and the free cells (top left).

Cards valued 1 to 9 stack on the foundations by suit (or color) in increasing
order (1->2->3). They can also be stacked on the tableau in decreasing
order (9->8->7), but only if the suits alternate.

Dragons (labeled with dashes) can't be stacked -- but when all four dragons
of a single suit are exposed, they can be moved into a single free cell.

The free cells are empty areas where one of any card can be stored. (But, as
stated before, four dragons of the same color can also be moved into a single 
cell when possible.)

### Controls
 * 1,2,3: select free cells
 * W,E,R,T,Y,U,I,O: select tableau rows
   (press multiple times to move more than one card from a row)
   (hold shift to select as many cards as possible from a row)
 * 8,9,0: select foundations
 * [space bar]: remove selection
 * 4,5,6: move dragon cards onto a free cell (when available)
 * Shift+X or F10: quit.
 * Z (QWERTY mode only) or F3: restart the game
   (all cards will move to the same positions they were in
   at the start of the game)
 * N or F2: start a new game
 * Shift+N or F4: start a specifically-numbered game
 * A (QWERTY mode only) or F5: toggle automoves on/off
 
## How to play exasol

ПАСЬЯНС is played with a 36-card deck of the type popular in Russian card games. It is similar to a 52- or 54-card deck common in the West, but does not contain joker cards or cards of ranks 2,3,4 or 5.

In exasol, the cards are all represented with their numeric value, followed by a letter denoting their suit. A six of hearts is "6H", a 10 of clubs is "10C". In addition, since face cards (jacks, queens, kings and aces) are used differently, their values are written in lowercase (e.g. a jack of diamonds is "jD").

The goal is to arrange the 36 cards, which are initially spread randomly and evenly across 9 rows of the tableau, into 8 separate stacks. Four stacks contain cards from 10 to 6, stacked in alternating colors (spades and clubs are black (displayed as white in exasol), diamonds and hearts are red). Another four stacks contain face cards, in the same suit.

Cards can be stacked on top of each other, and moved as a stack, if they follow the rules described above: stacks of number cards have to fall down by 1 in value (9->8->7) and alternate colors (black-\>red-\>black). Face cards have to be stacked on top of other face cards of the same suit, but the order is irrelevant. 

A "free cell" is available, which can store any one card at any moment.

In addition, when four face cards of one suit are stacked on an empty row, they are "collapsed" and become immovable. Nothing can be stacked on top of them, as well.

### Controls
 * Q,W,E,R,T,Y,U,I,O: select tableau rows
   (press multiple times to move more than one card from a row)
   (hold shift to select as many cards as possible from a row)
 * P: select free cell
 * [space bar]: remove selection
 * Shift+X or F10: quit.
 * Z (QWERTY mode only) or F3: restart the game
   (all cards will move to the same positions they were in
   at the start of the game)
 * N or F2: start a new game
 * Shift+N or F4: start a specifically-numbered game

## How to play kabusol

Kabufuda solitaire is played with a 40-card kabufuda deck. The deck features cards valued between 1 and 10, four of each value.

In kabusol, all of the cards are represented with their numeric value. A "one" card is colored red, to represent the color of its card.

The goal is to arrange the 40 cards, which are initially spread randomly and evenly across 8 rows of the tableau, into 10 separate stacks. Cards form a stack whenever four cards of the same value (and no other cards) occupy a tableau row or a free cell.

Cards can be stacked if they have the same value. A free cell can only contain one card, but it's also possible to drop 4 equal-value cards onto an empty free cell (forming a stack) or to drop 3 equal-value cards onto a free cell containing the 4th card of the same value (also forming a stack).

When a stack is formed, it becomes immovable: no cards can be placed on it, and it can't be moved anywhere. In addition, if there are locked free cells (which occur when the game is started at a difficulty level other than "Easy"), one locked free cell is unlocked whenever a stack is formed.

**WARNING**: the game only counts victories when it's played on the "Expert" difficulty.

### Controls
 * W,E,R,T,Y,U,I,O: select tableau rows
   (press multiple times to move more than one card from a row)
   (hold shift to select as many cards as possible from a row)
 * 2,3,4,5: select one of the free cells
 * [space bar]: remove selection
 * Shift+X or F10: quit.
 * Z (QWERTY mode only) or F3: restart the game
   (all cards will move to the same positions they were in
   at the start of the game)
 * D: change the difficulty at which the next game will be played.
 * N or F2: start a new game
 * Shift+N or F4: start a specifically-numbered game
