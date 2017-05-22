# szsol - a quick, dirty and very unauthorized ncurses clone of Shenzhen Solitaire

This is a simple ncurses-based version of the solitaire game originally
developed by [Zachtronics](https://twitter.com/zachtronics) and distributed
both as part of
[SHENZHEN I/O](http://store.steampowered.com/app/504210/SHENZHEN_IO/) and as a
separate game on
[Steam](http://store.steampowered.com/app/570490/SHENZHEN_SOLITAIRE/) and the
[App Store](https://itunes.apple.com/tw/app/shenzhen-solitaire/id1206037778).

![Screenshot](http://i.imgur.com/H0e961d.png)

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

## How to play

The goal is to clear the tableau (the central area) by moving all the cards onto
the foundations (top right) and the free cells (top left).

Cards valued 1 to 9 stack on the foundations by suit (or color) in increasing
order (1->2->3). They can also be stacked on the tableau in decreasing
order (9->8->7), but only if the suits alternate.

Dragons (labeled with dashes) can't be stacked -- but when all four dragons
of a single suit are exposed, they can be moved into a single free cell.

The free cells are empty areas where one of any card can be stored. (With the
exception of the dragons, which, as stated before, can be moved into a single
cell when possible.)

The flower (labeled '@') is a purely decorative card and will be moved to its
dedicated area automatically as soon as possible.

## Controls
 * 1,2,3: select free cells
 * W,E,R,T,Y,U,I,O: select tableau rows
   (press multiple times to move more than one card from a row)
   (hold shift to select as many cards as possible from a row)
 * 8,9,0: select foundations
 * [space bar]: remove selection
 * 4,5,6: move dragon cards onto a free cell (when available)
 * Shift+Q or Ctrl+C -- quit.
