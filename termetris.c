/* Simple recreation of tetris using the curses library
 * Check LICENCE for copyright and licence details */

#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define     MINLINES            38
#define     MINCOLS             82
#define     GAME_BLOCK_HEIGHT   18
#define     GAME_BLOCK_WIDTH    10
#define     BOX_CHAR            ' '
#define     NO_BLOCK            0
#define     MAX_SPEED_LEVEL     20
#define     REF_GAME(G)         drawGameBox((G)); refresh();
/* Where L is the level */
#define     GSPEED(L)           ((L)->level <= MAX_SPEED_LEVEL ? CLOCKS_PER_SEC / (L)->level : CLOCKS_PER_SEC / MAX_SPEED_LEVEL)
/* Where C is previous color */
#define     NCOLOR(C)           ((C) >= 4 ? 1 : (C)+1)

#define     POINTS_POS          4
#define     NEXT_POS            -10
#define     HOLD_POS            -20

typedef struct Tblock Tblock;
typedef struct Tetromino Tetromino;
typedef struct Menu Menu;
typedef struct Option Option;
typedef struct Game Game;
typedef struct TetroType TetroType;

typedef enum TetroEnum {
    NONE = 0,
    I = 1,
    S = 2,
    O = 3,
    T = 4,
    L = 5
} TetroEnum;

struct TetroType {
    TetroEnum type;
    int spos[4][2];         /* Start Positions */
    int ispos[4][2];        /* Inverted start positions */
    int cpos;               /* Position of the blocks's center */
};

struct Menu {
    char * options[2];
    int sel;
};

struct Tetromino {
    TetroEnum type;
    int inv;
    int color;
};

struct Tblock {
    int r, c;           /* Row and column */
    int * ptr;          /* Pointer to the block */
    Tblock * center;    /* Center of the tetromino*/
};

struct Game {
    WINDOW * win;           /* Window of the game */
    WINDOW * menuwin;       /* Window of the menu */
    int blocks[GAME_BLOCK_WIDTH + 1][GAME_BLOCK_HEIGHT + 1];
    Tblock selblocks[4];    /* Current selected blocks */
    Tetromino nt;           /* Next tetromino */
    Tetromino ct;           /* Current tetromino */
    Tetromino oh;           /* Tetromino on hold */
    unsigned int points;
    clock_t timer;
    clock_t groundtimer;
    int canhold;            /* If the player can put the current tetromino on hold */
    int level;
    int lines;              /* Number of lines deleted */
};

static void drawMenu(WINDOW * menuwin, Menu menu);
static void runGame(Game *game);
static void drawGameBox(Game *game);
static void drawGameStatus(Game *game);
static void drawTetromino(WINDOW * win, Tetromino t, int y, int x);
static Menu startMenu(WINDOW * menuwin);
static Game * createGame(WINDOW * gwin);
static WINDOW * drawGameWindow();
static WINDOW * drawMenuWindow();
static WINDOW * create_newwin(int heightm, int width, int starty, int startx);
/* Tetromino Management */
static void selectBlock(Game *game, int c, int r, int bn);
static void placeTetromino(Game * game);
static void trySpawn(Game *game, Tetromino tetro);
static void selectBlock(Game *game, int c, int r, int i);
static void moveTetromino(Game *game, int h, int v);
static void descendBlocks(Game *game, int sr);
static void deleteTetromino(Game *game);
static void deleteRow(Game *game, int rn);
static void putOnHold(Game *game);
static void showTetromino(Game *game);
static void deleteFullRows(Game *game);
static void resizeHandler();
static void tryRotate(Game *game, int d);
static void trySpawn(Game *game, Tetromino t);
static void updateDowntime(Game *game);
static int isOver(Game *game, Tetromino t);
static int checkMove(Game *game, int h, int v);
static int isSelected(Game *game, int * b);
static Tetromino gentetromino(Tetromino prev);

/* Static variables */
static WINDOW *game_window, *menu_window;
static Menu menu;
static Game * game;

/* Positions for the different types of tetrominos */
#define I_POS   {{0, 1}, {0, 2}, {0, 3}, {0, 4}}
#define O_POS   {{0, 1}, {0, 2}, {1, 1}, {1, 2}}
#define T_POS   {{-1, 1},{0, 1}, {1, 1}, {0, 2}}
#define S_POS   {{-1, 1},{0, 1}, {0, 2}, {1, 2}}
#define L_POS   {{0, 1}, {0, 2}, {0, 3}, {1, 3}}

/* Positions for the inverted tetrominos */
#define InvI_POS   I_POS
#define InvO_POS   O_POS
#define InvT_POS   {{-1, 2},{0, 1}, {1, 2}, {0, 2}}
#define InvS_POS   {{-1, 2},{0, 1}, {0, 2}, {1, 1}}
#define InvL_POS   {{0, 1}, {0, 2}, {0, 3}, {1, 1}}

/* Define the different tetromino types */
static const TetroType types[] = {
    { I, I_POS, InvI_POS, 2},
    { S, S_POS, InvS_POS, 2},
    { O, O_POS, InvO_POS, 0},
    { T, T_POS, InvT_POS, 1},
    { L, L_POS, InvL_POS, 1}
};

#define     T_COL(T, N)         ((T).inv ? types[(T).type - 1].ispos[N][0] : types[(T).type - 1].spos[N][0])
#define     T_ROW(T, N)         ((T).inv ? types[(T).type - 1].ispos[N][1] : types[(T).type - 1].spos[N][1])
#define     T_CEN(T)            (types[(T).type - 1].cpos)

/* Positions that are going to be tested (from left to right) each time the game tries to place the tetromino */
static const int totrypos[10] = {5, 6, 4, 7, 3, 8, 2, 9, 1, 10};

/* Check if a tetromino can spawn in a position */
int canSpawn(Game * game, Tetromino t, int sp) {
    int c,r;
    if (!t.type)
        return 1;
    for (int i = 0; i < 4; i++){
        c = T_COL(t,i) + sp;
        r = T_ROW(t,i);
        if (c > GAME_BLOCK_WIDTH + 1 || c < 1 || r > GAME_BLOCK_HEIGHT + 1 || r < 1)
            return 0;
        else if (game->blocks[c][r])
            return 0;
    }
    return 1;
}

/* Spawn a tetromino */
void spawnTetromino(Game *game, Tetromino t, int sp) {
    int c,r;
    for (int i = 0; i < 4; i++) {
        c = T_COL(t,i) + sp;
        r = T_ROW(t,i);
        game->blocks[c][r] = t.color;
        game->selblocks[i].ptr = &game->blocks[c][r];
        game->selblocks[i].c = c; game->selblocks[i].r = r;
        game->selblocks[i].center = &game->selblocks[T_CEN(t)];
    }
    game->ct = t;
    wrefresh(game->win);
}

/* Tries to spawn a tetrmono in all positions */
void trySpawn(Game *game, Tetromino t) {
    for (int i = 0; i < 10; i++ )
        if (canSpawn(game, t, totrypos[i])){
            spawnTetromino(game, t, totrypos[i]);
            return;
        }
}

/* Check if the game is over assuming t is the next tetromino */
int isOver(Game *game, Tetromino t) {
    if (t.type == NONE)
        return 0;
    for (int st = 1; st <= GAME_BLOCK_WIDTH; st++)
        if (canSpawn(game, t, st))
            return 0;
    return 1;
}

/* Generates a next tetromino assuming prev is the previous one */
Tetromino gentetromino(Tetromino prev) {
    Tetromino t;
    t.inv = rand() % 2;
    t.type = rand() % 5 + 1;
    t.color = NCOLOR(prev.color);
    return t;
}

/* Delete a row */
void deleteRow(Game *game, int rn) {
    for (int c = 1; c <= GAME_BLOCK_WIDTH; c++)
        game->blocks[c][rn] = COLOR_BLACK;
}

void updateDowntime(Game *game) {
    if (!checkMove(game, 0, 1) && clock() > game->groundtimer)
        game->groundtimer = clock() + CLOCKS_PER_SEC;
    else if (checkMove(game, 0, 1))
        game->groundtimer = clock();
}

/* Puts the current tetromino on hold */
void putOnHold(Game * game) {
    if (!game->canhold)
        return;
    /* If there is another piece already on hold */
    if (game->oh.type != NONE){
        Tetromino tbuf = game->ct;
        deleteTetromino(game);
        trySpawn(game, game->oh);
        game->oh = tbuf;
    /* If there isn't */
    } else {
        game->oh = game->ct;
        game->nt = gentetromino(game->ct);
        deleteTetromino(game);
        trySpawn(game, game->nt);
    }
    game->canhold = 0;
}

/* Move all rows down since sr (start row) */
void descendBlocks(Game *game, int sr) {
    for (int r = sr; r >= 1; r--) {
        for (int c = 1; c <= GAME_BLOCK_WIDTH; c++)
            game->blocks[c][r+1] = game->blocks[c][r];
        deleteRow(game, r);
    }
}

/* Delete all rows that are full and updates the game structure */
void deleteFullRows(Game *game) {
    
    int r, c;
    int del;            /* If a line was deleted */
    int dl = 0;         /* Deleted lines */

    for (r = 1; r <= GAME_BLOCK_HEIGHT; r++) {
        del = 1;
        for (c = 1; c <= GAME_BLOCK_WIDTH; c++)
            if (game->blocks[c][r] == COLOR_BLACK)
                del = 0;
        if (del) {
            /* Delete the row */
            deleteRow(game, r);
            /* Delete the row */
            descendBlocks(game, r - 1);
            dl++;
        }
    }
    /* Update the game's structure */
    if (dl) {
        switch (dl){
            case 1: game->points += (40 * game->level); break;
            case 2: game->points += (100 * game->level); break;
            case 3: game->points += (300 * game->level); break;
            default: game->points += (300 * game->level); break;
        }
    game->lines += dl;
    game->level = (int)((game->lines / 10) + 1);
    }
}

/* Shows the current tetromino placed and refreshes the screen */
void showTetromino(Game *game) {

    int db = 0;     /* Nº of blocks down */
    int *b[4];      /* Buffer for the plocks */

    /* Check how far the tetromino goes */
    for (db = 0; checkMove(game, 0, 1); moveTetromino(game, 0, 1), db++);

    for (int i = 0; i <= 3; i++)
        b[i] = game->selblocks[i].ptr;
    moveTetromino(game, 0, -db);
    for (int i = 0; i <= 3; i++)
        if (!isSelected(game, b[i]))
            *b[i] = 5;
    REF_GAME(game);
    for (int i = 0; i <= 3; i++)
        if (!isSelected(game, b[i]))
            *b[i] = 0;
}

/* Checks if a block is currently selected */
int
isSelected(Game *game, int * b) {
    for (int i = 0; i <= 3; i++ )
        if (b == game->selblocks[i].ptr)
            return 1;
    return 0;
}

/* Deletes current Tetromino */
void deleteTetromino(Game *game) {
    for (int i = 0; i <= 3; i++)
        *game->selblocks[i].ptr = COLOR_BLACK;
}

/* Check if the tetromino can move certain blocks */
int checkMove(Game *game, int h, int v) {

    int chk = 1;
    int i, c, r;
    int color = *game->selblocks[0].ptr;

    /* Delete all current selected blocks to avoid conflicts */
    for (i = 0; i <= 3; i++)
        *game->selblocks[i].ptr = COLOR_BLACK;

    /* Check if moving any block will cause conflicts */
    for (i = 0; i <= 3; i++) {
        c = game->selblocks[i].c;
        r = game->selblocks[i].r;
        if  (
            (game->blocks[c+h][r+v] != COLOR_BLACK) ||
            (  ( (c + h) > GAME_BLOCK_WIDTH ) || ( (r + v) > GAME_BLOCK_HEIGHT ) ) ||
            ( (c + h <= 0) || (r + v <= 0))
            )
            chk = 0;
    }
    /* Restore blocks */
    for (i = 0; i <= 3; i++)
        *game->selblocks[i].ptr = color;

    return chk;
}

/* Checks if the current selected blocks can rotate in directon d */
int canRotate(Game *game, int d) {
    if (game->selblocks[0].center == NULL)
        return 0;
    int i, dc, dr, nc, nr, color;
    int r = 1;                              /* Return value */
    color = *game->selblocks[0].ptr;
    // Delete all current selected blocks to avoid conflicts
    for (i = 0; i <= 3; i++)
        *game->selblocks[i].ptr = COLOR_BLACK;
    for (i = 0; i <= 3; i++){
        // Calculate the difference
        dc = game->selblocks[i].center->c - game->selblocks[i].c;
        dr = game->selblocks[i].center->r - game->selblocks[i].r;
        // Get new coordinates
        nc = game->selblocks[i].center->c - (dr * d);
        nr = game->selblocks[i].center->r + (dc * d);
        if (
            (game->blocks[nc][nr] != COLOR_BLACK) ||      /* Check if there aren't any blocks on the new coordinates */
            /* Check if new new coordinates are inside the box */
            ( ( nr < 1 ) || ( nc < 1 ) ) ||
            ( ( nr > GAME_BLOCK_HEIGHT ) || ( nc > GAME_BLOCK_WIDTH ))
            )
            r = 0;
    }
    /* Refill blocks */
    for (i = 0; i <= 3; i++)
        *game->selblocks[i].ptr = color;
    return r;
}

/* Rotates the current selected blocks */
void rotateTetromino(Game *game, int d) {

    /* If the piece has no center, stop */
    if (game->selblocks[0].center == NULL)
        return;
    
    int dc, dr, nc, nr, i, color;

    color = *game->selblocks[0].ptr;

    /* Delete all current selected blocks */
    for (i = 0; i <= 3; i++)
        *game->selblocks[i].ptr = COLOR_BLACK;

    for (i = 0; i <= 3; i++){
        // Calculate the difference
        dc = game->selblocks[i].center->c - game->selblocks[i].c;
        dr = game->selblocks[i].center->r - game->selblocks[i].r;
        // Get new coordinates
        nc = game->selblocks[i].center->c - (dr * d);
        nr = game->selblocks[i].center->r + (dc * d);
        // Save new coordinates
        game->blocks[nc][nr] = color;
        selectBlock(game, nc, nr, i);
    }
}

/* Tries to force a tetromino do rotate by moving it */
void tryRotate(Game *game, int d) {

    if (game->ct.type == O)
        return;

    /* All positions that are going to be tried */
    int pos[4] = {-1, 1, -2, 2};

    if (canRotate(game, d))
        rotateTetromino(game, d);
    else {
        for (int i = 0; i < 4;i++) {
            if (checkMove(game, pos[i],0)) {
                moveTetromino(game, pos[i], 0);
                if (canRotate(game, d)){
                    rotateTetromino(game, d);
                    return;
                }
                else {
                    moveTetromino(game, -pos[i], 0);
                }
            }
        }
        /* In case those positions don't work, try going down */
        if (checkMove(game, 0, 1)){
            moveTetromino(game, 0, 1);
            if (canRotate(game, d))
                rotateTetromino(game, d);
            else
                moveTetromino(game, 0, 1);
        }
    }
}

/* Places the tetromino in the current position */
void placeTetromino(Game *game) {
    for (int i = 0; i <= 3; i++) {
        game->selblocks[i].ptr = NO_BLOCK;
        game->selblocks[i].c = 0;
        game->selblocks[i].r = 0;
    };
}

/* Select a block */
void selectBlock(Game *game, int c, int r, int i) {
    game->selblocks[i].c = c;
    game->selblocks[i].r = r;
    game->selblocks[i].ptr = &game->blocks[c][r];
}

/* Move the selected loblocks h to the right and v vertically */
void moveTetromino(Game *game, int h, int v) {
    int i, c, r, color;
    /* Get the color */
    color = *game->selblocks[0].ptr;
    /* Delete all current selected blocks */
    for (i = 0; i <= 3; i++)
        *game->selblocks[i].ptr = COLOR_BLACK;
    /* Color all the next blocks */
    for (i = 0; i <= 3; i++) {
        c = game->selblocks[i].c;
        r = game->selblocks[i].r;
        game->blocks[c + h][r + v] = color;
        selectBlock(game, c + h, r + v, i);
    }
}

/* Initializes the color pairs that are going to be used */
void initColorPairs() {
    int colors[] = { COLOR_BLACK, COLOR_RED, COLOR_CYAN, COLOR_YELLOW, COLOR_GREEN, COLOR_WHITE };
    for (int i = 0; i < 6; i++){
        /* Pairs for tetris blocks */
        init_pair(i, COLOR_BLACK, colors[i]);
        /* Pairs for text */
        init_pair(10 + i, colors[i], -1);
    }
}

/* Draws a tetromino on a window in certain coordinates */
void drawTetromino(WINDOW * win, Tetromino t, int y, int x){
    int blocks[4][4];
    int c, r;

    for (int i = 0; i < 4; i++)
        for (int b = 0; b < 4; b++)
            blocks[i][b] = COLOR_BLACK;
    if (t.type != NONE)
        for (int i = 0; i < 4; i++) {
            c = T_COL(t, i) + 1;
            r = T_ROW(t, i) - 1;
            blocks[c][r] = t.color;
        }
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            for ( int i = 0; i <= 1; i++)
                for (int a = 0; a <= 3; a++)
                    mvwaddch(win, (r * 2 - i) + y, (c * 4 - a) + x,
                            BOX_CHAR | COLOR_PAIR(blocks[c][r]));
}

/* Deletes all characters on the window */
void clearwin(WINDOW * win){
    for (int r = 1;r < win->_maxy; r++) 
        for (int c = 1;c < win->_maxx; c++) 
            mvwaddch(win, r, c, ' ');
}

/* Draws the status of the game's menu */
void drawGameStatus(Game * game) {
    /* Show points */
    char buf[30];
    sprintf(buf, "Points: %i", game->points);
    mvwaddstr(game->menuwin, 2, 5, buf);
    /* Show level */
    sprintf(buf, "Level: %i", game->level);
    mvwaddstr(game->menuwin, 4, 5, buf);
    /* Show tetromino on hold */
    mvwaddstr(game->menuwin, game->menuwin->_maxy - 12, 5, "Next:");
    drawTetromino(game->menuwin, game->nt, game->menuwin->_maxy - 8, 9);
    /* Show next tetromino */
    mvwaddstr(game->menuwin, game->menuwin->_maxy - 24, 5, "Holding:");
    drawTetromino(game->menuwin, game->oh, game->menuwin->_maxy - 20, 9);
    wrefresh(game->menuwin);
}

/* Create a new window */
WINDOW *create_newwin(int height, int width, int starty, int startx) {
    WINDOW *local_win;

	local_win = newwin(height, width, starty, startx);
	box(local_win, 0 , 0);		/* 0, 0 gives default characters 
					             * for the vertical and horizontal
					             * lines			    */
	wrefresh(local_win);		/* Show that box 		*/
	return local_win;
}

/* Draws the menu when the game isn't being played */
void drawMenu(WINDOW * menuwin, Menu menu) {

    char selopt[15];
    char opt[15];
    int l = 0;
    int spos = (menuwin->_maxx / 2) - 7;

    /* Draw logo */
    wattron(menuwin, A_BOLD | COLOR_PAIR(11));
    mvwaddstr(menuwin, menuwin->_maxy / 11, spos + 2, "Termetris");
    wattroff(menuwin, A_BOLD | COLOR_PAIR(11));

    /* 0 means no option is selected */
    if (menu.sel != 0)
        sprintf(selopt, "> %s", menu.options[menu.sel - 1]);;
    for (int i = 1; i <= 2; i++, l++){
        if (i != menu.sel){
            sprintf(opt, "  %s", menu.options[i - 1]);;
            mvwaddstr(menuwin, menuwin->_maxy / 2 + l,
                    spos, opt);
        } else {
            wattron(menuwin, A_BOLD | COLOR_PAIR(14));
            mvwaddstr(menuwin, menuwin->_maxy / 2 + l,
                    spos, selopt);
            wattroff(menuwin, A_BOLD | COLOR_PAIR(14));
        }
    }
    wrefresh(menuwin);
}

Menu startMenu(WINDOW * menuwin) {
    Menu menu;
    menu.options[0] = "Start Game";
    menu.options[1] = "Exit Game";
    menu.sel = 1;
    return menu;
}

/* Draws the game box based on the block matrice */
void drawGameBox(Game *game) {
    for (int c = 1; c <= GAME_BLOCK_WIDTH; c++)
        for (int r = 1; r <= GAME_BLOCK_HEIGHT; r++)
            for ( int i = 0; i <= 1; i++)
                for (int a = 0; a <= 3; a++)
                    mvwaddch(game->win, r * 2 - i, c * 4 - a,
                            BOX_CHAR | COLOR_PAIR(game->blocks[c][r]));
    wrefresh(game->win);
}

/* Creates the a window for the game */
WINDOW * drawGameWindow() {
    WINDOW *my_win;
    int width, height;
	height = GAME_BLOCK_HEIGHT * 2 + 2;
	width = GAME_BLOCK_WIDTH * 4 + 2;
	my_win = create_newwin(height, width, 0, 1);
	refresh();
    return my_win;
}

/* Creates the a window for the menu */
WINDOW * drawMenuWindow() {
    WINDOW *my_win;

    int startx, starty, width, height;

	height = LINES;
	width = (COLS - GAME_BLOCK_WIDTH * 4 - 8);
	starty = 0;	                            /* Calculating for a center placement */
	startx = GAME_BLOCK_WIDTH * 4 + 6;	    /* of the window		*/
	my_win = create_newwin(height, width, starty, startx);
	refresh();
    return my_win;
}

/* Initializes the game structure */
Game * createGame(WINDOW * gwin) {

    static Game game;
    Tetromino tet;

    game.win = gwin;
    game.canhold = 1;
    game.level = 1;
    game.lines = 0;
    game.timer = 0;
    game.groundtimer = 0;

    Tetromino oh;
    oh.type = NONE;
    game.oh = oh;

    for (int i = 1; i <= GAME_BLOCK_WIDTH; i++)
        for (int a = 1; a <= GAME_BLOCK_HEIGHT; a++)
            game.blocks[i][a] = COLOR_BLACK;

    tet.inv = rand() % 2;           /* inverted */
    tet.type =  rand() % 5 + 1;     /* type */
    tet.color = 1;
    game.points = 0;
    game.nt = tet;

    return (&game);
}

void runGame(Game * game) {
    int c, d;
    int nmovemax = 0;
    char pointsstr[20], levelstr[15], linesstr[15];  
    trySpawn(game, game->nt);
    showTetromino(game);
    game->timer = clock() + GSPEED(game);
    game->groundtimer = clock();
    game->nt = gentetromino(game->ct);
    clearwin(game->menuwin);
    drawGameStatus(game);
    refresh();

    int io = 0;         /* If the game is over */

    while ((c = wgetch(game->win)) != 'q' && !io && c != 27) {
        if ( c == ERR ){
            /* The user isn't pressing any key */
            if (clock() > game->timer){
                if (checkMove(game, 0, 1)){
                    moveTetromino(game, 0, 1);
                    showTetromino(game);
                    updateDowntime(game);
                    game->timer = clock() + GSPEED(game);
                }
                else if (clock() > game->groundtimer){
                    placeTetromino(game);
                    deleteFullRows(game);
                    trySpawn(game, game->nt);
                    game->ct = game->nt;
                    game->nt = gentetromino(game->ct);
                    game->canhold = 1;
                    if (!(io = isOver(game, game->nt)))
                        showTetromino(game);
                    game->timer = clock() + GSPEED(game);
                    updateDowntime(game);
                    drawGameStatus(game);
                }
            }
        } else {
        switch (c) {
            case KEY_DOWN:
                if (checkMove(game, 0, 1))
                    moveTetromino(game, 0, 1);
                break;
            case KEY_LEFT:
            case KEY_RIGHT:
                d = (c == KEY_LEFT ? -1 : 1);
                if (!nmovemax){
                    if (checkMove(game, d, 0))
                        moveTetromino(game, d, 0);
                    break;
                } else {
                    while (checkMove(game, d, 0))
                        moveTetromino(game, d, 0);
                    nmovemax = 0;
                }
                break;
            case '<':
                nmovemax = 1;
                break;
            case ' ':
                /* Move the tetromino down */
                while (checkMove(game, 0, 1))
                    moveTetromino(game, 0, 1);
                /* Placing a tetromino */
                placeTetromino(game);
                deleteFullRows(game);
                if (!(io = isOver(game, game->nt)))
                    trySpawn(game, game->nt);
                game->ct = game->nt;
                game->nt = gentetromino(game->ct);
                game->canhold = 1;
                drawGameStatus(game);
                break;
            case 'c':
                if (!isOver(game, game->oh))
                    putOnHold(game);
                drawGameStatus(game);
                break;
            case 'z':
            case 'x':
                d = (c == 'z' ? -1 : 1);
                tryRotate(game, d);
                break;
            case KEY_RESIZE:
                resizeHandler();
                break;
        }
        updateDowntime(game);
        showTetromino(game);
        }
    }

    /* Clear everything */
    for (int i = 1; i <= GAME_BLOCK_WIDTH; i++)
        for (int a = 1; a <= GAME_BLOCK_HEIGHT; a++)
            game->blocks[i][a] =  COLOR_BLACK;

    drawGameBox(game);
    wattron(game->win, A_BOLD | COLOR_PAIR(11));
    mvwaddstr(game->win, 19, 17, "Game Over");
    wattroff(game->win, A_BOLD | COLOR_PAIR(11));
    sprintf(pointsstr, "Points: %i", game->points);
    mvwaddstr(game->win, 20, 17, pointsstr);
    sprintf(levelstr, "Level: %i", game->level);
    mvwaddstr(game->win, 21, 17, levelstr);
    sprintf(linesstr, "Lines: %i", game->lines);
    mvwaddstr(game->win, 22, 17, linesstr);

    clearwin(game->menuwin);
    wrefresh(game->win);
    refresh();
}

void resizeHandler() {
    endwin();
    refresh();

    if (LINES < MINLINES || COLS < MINCOLS){
        endwin();
        exit(1);
    }
    menu_window = drawMenuWindow();
    drawMenu(menu_window, menu);
    box(game->win, 0,0);
    drawGameBox(game);
    refresh();
}

int main(int argc, char *argv[]) {
    initscr();  /* Initialize curse's main source */

    if (!has_colors()){
        endwin();
        printf("Your terminal emulator doesn't support colors.\n");
        exit(1);
    }

    if (LINES < MINLINES || COLS < MINCOLS){
        endwin();
        printf("Not enough space\n");
        return EXIT_FAILURE;
    }

    /* Random seed */
    srand(time(0));

    cbreak();               /* One character at a time */
    noecho();               /* Disable do automatic echo of typed characters */
    keypad(stdscr, TRUE);   /* Enable the capture of special keystrokes (such as arrow keys) */

    raw();
    start_color();
    use_default_colors();
    initColorPairs();
    refresh();

    /* Initialize the menu */
    menu_window = drawMenuWindow();
    menu = startMenu(menu_window);
    drawMenu(menu_window, menu);
    wrefresh(menu_window);
    refresh();

    /* Initializes the game */
    game_window = drawGameWindow();
    game = createGame(game_window);
    game->menuwin = menu_window;
    nodelay(game->win, TRUE);   /* Don't wait for user input when playing the game */
    keypad(game->win, TRUE);    /* Enable the capture of special keystrokes (such as arrow keys) */

    /* Menu selection */
    int c;
    while ((c = getch()) != 'q'){
        if (c == KEY_UP || c == 'k'){
            menu.sel = 1;
            drawMenu(menu_window, menu);
            refresh();
        } else if (c == KEY_DOWN || c == 'j'){
            menu.sel = 2;
            drawMenu(menu_window, menu);
            refresh();
        } else if (c == 'p') {
            drawGameStatus(game);
            refresh();
        } else if (c == KEY_RESIZE){
            resizeHandler();
        } else if (c == 10){
            if (menu.sel == 1){
                menu.sel = 0;
                drawMenu(menu_window, menu);
                runGame(game);
                /* Restrat the game */
                game = createGame(game->win);
                menu.sel = 1;
                drawMenu(menu_window, menu);
                refresh();
            }
            else if (menu.sel == 2){
                endwin();
                exit(0);
            }
        }
    }
    endwin();
    exit(0);
}
