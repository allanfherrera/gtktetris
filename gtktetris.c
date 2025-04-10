#include <gtk/gtk.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

// Game constants
#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20
#define BLOCK_SIZE 30
#define LEVEL_THRESHOLD 5000
#define PREVIEW_SIZE 5
#define MAX_LEVEL 10  // Added maximum level to prevent integer overflow
#define SCORE_TEXT_SIZE 50

// Secure structure for tetromino data
typedef struct {
    int shape[4][2];
    double color[3];
} Tetromino;

static const Tetromino tetrominoes[7] = {
    {{{0,0}, {0,1}, {1,0}, {1,1}}, {1.0, 1.0, 0.0}}, // Square
    {{{0,0}, {0,1}, {0,2}, {0,3}}, {0.0, 1.0, 1.0}}, // Line
    {{{0,0}, {0,1}, {1,1}, {1,2}}, {1.0, 0.0, 0.0}}, // Z
    {{{0,1}, {0,2}, {1,0}, {1,1}}, {0.0, 1.0, 0.0}}, // S
    {{{0,0}, {0,1}, {0,2}, {1,1}}, {1.0, 0.0, 1.0}}, // T
    {{{0,0}, {1,0}, {2,0}, {2,1}}, {1.0, 0.5, 0.0}}, // L
    {{{0,1}, {1,1}, {2,0}, {2,1}}, {0.0, 0.0, 1.0}}  // J
};

// Game state structure
typedef struct {
    int board[BOARD_HEIGHT][BOARD_WIDTH];
    int board_colors[BOARD_HEIGHT][BOARD_WIDTH];
    int current_x, current_y;
    int current_piece[4][2];
    int current_type, next_type;
    int score;
    int level;
    int game_speed;
    char score_text[SCORE_TEXT_SIZE];
    bool game_over;
    bool paused;
    guint timeout_id;
} GameState;

static GameState game = {0};

// Widget pointers
static struct {
    GtkWidget *drawing_area;
    GtkWidget *score_label;
    GtkWidget *new_game_button;
    GtkWidget *pause_button;
    GtkWidget *preview_area;
} widgets;

// Function prototypes with added security attributes
static void new_piece(void) __attribute__((nonnull));
static gboolean game_loop(gpointer data) __attribute__((warn_unused_result));
static void secure_strcpy(char *dest, size_t dest_size, const char *src);

// Initialize game state securely
static void init_game_state(void) {
    memset(&game, 0, sizeof(GameState));
    game.game_speed = 500;
    game.level = 1;
}

// Secure random number generator
static int secure_rand(int max) {
    return (int)((double)rand() / (RAND_MAX + 1.0) * max);
}

void start_new_game(GtkButton *button, gpointer data) {
    init_game_state();
    if (game.timeout_id) {
        g_source_remove(game.timeout_id);
    }
    game.timeout_id = g_timeout_add(game.game_speed, game_loop, NULL);
    
    // Use snprintf for buffer overflow protection
    snprintf(game.score_text, SCORE_TEXT_SIZE, "Score: %d  Level: %d", 
             game.score, game.level);
    gtk_label_set_text(GTK_LABEL(widgets.score_label), game.score_text);
    gtk_button_set_label(GTK_BUTTON(widgets.pause_button), "Pause");
    new_piece();
    gtk_widget_queue_draw(widgets.drawing_area);
}

void toggle_pause(GtkButton *button, gpointer data) {
    if (game.game_over) return;
    game.paused = !game.paused;
    gtk_button_set_label(GTK_BUTTON(widgets.pause_button), 
                        game.paused ? "Resume" : "Pause");
}

void new_piece(void) {
    game.current_type = game.next_type;
    game.next_type = secure_rand(7);
    game.current_x = BOARD_WIDTH / 2 - 2;
    game.current_y = 0;
    
    memcpy(game.current_piece, tetrominoes[game.current_type].shape, 
           sizeof(game.current_piece));
    gtk_widget_queue_draw(widgets.preview_area);
}

// Optimized drawing functions with boundary checking
gboolean draw_preview(GtkWidget *widget, cairo_t *cr, gpointer data) {
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_paint(cr);
    
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_rectangle(cr, 5, 5, PREVIEW_SIZE * BLOCK_SIZE/2 - 10, 
                   PREVIEW_SIZE * BLOCK_SIZE/2 - 10);
    cairo_stroke(cr);
    
    const double *color = tetrominoes[game.next_type].color;
    cairo_set_source_rgb(cr, color[0], color[1], color[2]);
    
    for (int i = 0; i < 4; i++) {
        int x = tetrominoes[game.next_type].shape[i][0] + 1;
        int y = tetrominoes[game.next_type].shape[i][1] + 1;
        cairo_rectangle(cr, x * BLOCK_SIZE/2, y * BLOCK_SIZE/2,
                       BLOCK_SIZE/2 - 1, BLOCK_SIZE/2 - 1);
        cairo_fill(cr);
    }
    return TRUE;
}

gboolean draw_callback(GtkWidget *widget, cairo_t *cr, gpointer data) {
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_paint(cr);

    // Draw board
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (game.board[y][x]) {
                int color_idx = game.board_colors[y][x] - 1;
                if (color_idx >= 0 && color_idx < 7) {
                    const double *color = tetrominoes[color_idx].color;
                    cairo_set_source_rgb(cr, color[0], color[1], color[2]);
                    cairo_rectangle(cr, x * BLOCK_SIZE, y * BLOCK_SIZE, 
                                  BLOCK_SIZE - 1, BLOCK_SIZE - 1);
                    cairo_fill(cr);
                }
            }
        }
    }

    // Draw current piece
    const double *current_color = tetrominoes[game.current_type].color;
    cairo_set_source_rgb(cr, current_color[0], current_color[1], current_color[2]);
    for (int i = 0; i < 4; i++) {
        int x = game.current_x + game.current_piece[i][0];
        int y = game.current_y + game.current_piece[i][1];
        if (y >= 0 && x >= 0 && x < BOARD_WIDTH) {
            cairo_rectangle(cr, x * BLOCK_SIZE, y * BLOCK_SIZE,
                          BLOCK_SIZE - 1, BLOCK_SIZE - 1);
            cairo_fill(cr);
        }
    }

    // Draw game over screen
    if (game.game_over) {
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.9);
        cairo_rectangle(cr, BOARD_WIDTH * BLOCK_SIZE/2 - 100, 
                       BOARD_HEIGHT * BLOCK_SIZE/2 - 40, 200, 80);
        cairo_fill(cr);
        
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_rectangle(cr, BOARD_WIDTH * BLOCK_SIZE/2 - 100, 
                       BOARD_HEIGHT * BLOCK_SIZE/2 - 40, 200, 80);
        cairo_stroke(cr);
        
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, 
                             CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 40);
        cairo_move_to(cr, BOARD_WIDTH * BLOCK_SIZE/2 - 90, 
                     BOARD_HEIGHT * BLOCK_SIZE/2 + 15);
        cairo_show_text(cr, "GAME OVER");
    }
    return TRUE;
}

static bool can_move(int dx, int dy) {
    for (int i = 0; i < 4; i++) {
        int new_x = game.current_x + game.current_piece[i][0] + dx;
        int new_y = game.current_y + game.current_piece[i][1] + dy;
        if (new_x < 0 || new_x >= BOARD_WIDTH || new_y >= BOARD_HEIGHT ||
            (new_y >= 0 && game.board[new_y][new_x])) {
            return false;
        }
    }
    return true;
}

static void land_piece(void) {
    for (int i = 0; i < 4; i++) {
        int x = game.current_x + game.current_piece[i][0];
        int y = game.current_y + game.current_piece[i][1];
        if (y >= 0 && x >= 0 && x < BOARD_WIDTH && y < BOARD_HEIGHT) {
            game.board[y][x] = 1;
            game.board_colors[y][x] = game.current_type + 1;
        }
    }
}

static void clear_lines(void) {
    int lines = 0;
    for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (!game.board[y][x]) {
                full = false;
                break;
            }
        }
        if (full) {
            lines++;
            for (int yy = y; yy > 0; yy--) {
                memcpy(game.board[yy], game.board[yy-1], 
                       sizeof(game.board[yy]));
                memcpy(game.board_colors[yy], game.board_colors[yy-1],
                       sizeof(game.board_colors[yy]));
            }
            memset(game.board[0], 0, sizeof(game.board[0]));
            memset(game.board_colors[0], 0, sizeof(game.board_colors[0]));
            y++;
        }
    }
    
    // Prevent integer overflow
    game.score = (game.score + lines * 100 * game.level > INT_MAX) ? 
                 INT_MAX : game.score + lines * 100 * game.level;
    
    snprintf(game.score_text, SCORE_TEXT_SIZE, "Score: %d  Level: %d", 
             game.score, game.level);
    gtk_label_set_text(GTK_LABEL(widgets.score_label), game.score_text);
    
    if (game.score >= game.level * LEVEL_THRESHOLD && game.level < MAX_LEVEL) {
        game.level++;
        game.game_speed = 500 / game.level;
        g_source_remove(game.timeout_id);
        game.timeout_id = g_timeout_add(game.game_speed, game_loop, NULL);
    }
}

gboolean game_loop(gpointer data) {
    if (game.paused || game.game_over) return TRUE;
    
    if (can_move(0, 1)) {
        game.current_y++;
    } else {
        land_piece();
        clear_lines();
        new_piece();
        if (!can_move(0, 0)) {
            game.game_over = true;
            gtk_widget_queue_draw(widgets.drawing_area);
            return FALSE;
        }
    }
    gtk_widget_queue_draw(widgets.drawing_area);
    return TRUE;
}

gboolean key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    if (game.game_over || !event) return TRUE;
    if (event->keyval == GDK_KEY_p) toggle_pause(NULL, NULL);
    
    if (game.paused) return TRUE;
    
    switch (event->keyval) {
        case GDK_KEY_Left:
            if (can_move(-1, 0)) game.current_x--; 
            break;
        case GDK_KEY_Right:
            if (can_move(1, 0)) game.current_x++; 
            break;
        case GDK_KEY_Down:
            if (can_move(0, 1)) game.current_y++; 
            break;
        case GDK_KEY_Up:
            int temp_piece[4][2];
            memcpy(temp_piece, game.current_piece, sizeof(temp_piece));
            for (int i = 0; i < 4; i++) {
                int x = game.current_piece[i][0];
                int y = game.current_piece[i][1];
                game.current_piece[i][0] = y;
                game.current_piece[i][1] = -x;
            }
            if (!can_move(0, 0)) {
                memcpy(game.current_piece, temp_piece, sizeof(game.current_piece));
            }
            break;
    }
    gtk_widget_queue_draw(widgets.drawing_area);
    return TRUE;
}

int main(int argc, char *argv[]) {
    // Secure random seed initialization
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        srand(time(NULL));
    } else {
        srand(ts.tv_nsec);
    }
    
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Tetris");
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(key_press), NULL);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    widgets.drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(widgets.drawing_area, 
                              BOARD_WIDTH * BLOCK_SIZE, 
                              BOARD_HEIGHT * BLOCK_SIZE);
    gtk_box_pack_start(GTK_BOX(main_box), widgets.drawing_area, FALSE, FALSE, 0);
    g_signal_connect(widgets.drawing_area, "draw", G_CALLBACK(draw_callback), NULL);

    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(main_box), right_box, FALSE, FALSE, 0);

    widgets.preview_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(widgets.preview_area, 
                              PREVIEW_SIZE * BLOCK_SIZE/2, 
                              PREVIEW_SIZE * BLOCK_SIZE/2);
    gtk_box_pack_start(GTK_BOX(right_box), widgets.preview_area, FALSE, FALSE, 0);
    g_signal_connect(widgets.preview_area, "draw", G_CALLBACK(draw_preview), NULL);

    widgets.new_game_button = gtk_button_new_with_label("New Game");
    g_signal_connect(widgets.new_game_button, "clicked", 
                    G_CALLBACK(start_new_game), NULL);
    gtk_box_pack_start(GTK_BOX(right_box), widgets.new_game_button, FALSE, FALSE, 0);

    widgets.pause_button = gtk_button_new_with_label("Pause");
    g_signal_connect(widgets.pause_button, "clicked", 
                    G_CALLBACK(toggle_pause), NULL);
    gtk_box_pack_start(GTK_BOX(right_box), widgets.pause_button, FALSE, FALSE, 0);

    widgets.score_label = gtk_label_new("Score: 0  Level: 1");
    gtk_box_pack_start(GTK_BOX(right_box), widgets.score_label, FALSE, FALSE, 0);

    init_game_state();
    game.next_type = secure_rand(7);
    new_piece();
    game.timeout_id = g_timeout_add(game.game_speed, game_loop, NULL);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
