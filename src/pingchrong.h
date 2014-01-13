#ifndef PINGCHRONG_H
#define PINGCHRONG_H

// This is a tradeoff between sluggish and too fast to see
#define MAX_BALL_SPEED 1 // note this is in vector arithmetic

// The radius of the ball (in pixels)
#define BALL_RADIUS 2

// The length of one animation frame in ms
#define ANIM_FRAME_TIME 50

// If the angle is too shallow or too narrow, the game is boring
#define MIN_BALL_ANGLE 20

// Paddle size (in pixels) and max speed for AI
#define PADDLE_H 20
#define PADDLE_W 3
#define PADDLE_MARGIN 2 // horizontal space between paddles and edge of screen
#define MAX_PADDLE_SPEED 10

// How thick the top and bottom lines are in pixels
#define BAR_HEIGHT 2

// How far from the screen edge the top and bottom lines are
#define BAR_MARGIN 2

static void anim_layer_update_callback(Layer * const me, GContext * ctx);
static uint8_t calculate_keepout(float theball_x, float theball_y, float theball_dx, float theball_dy, uint8_t *keepout1, uint8_t *keepout2);
static uint16_t crand(uint8_t type);
static void deinit(void);
static void encipher(void);
static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed);
static void init(void);
static void init_crand(void);
static uint8_t intersectrect(uint8_t x1, uint8_t y1, uint8_t w1, uint8_t h1, uint8_t x2, uint8_t y2, uint8_t w2, uint8_t h2);
static float random_angle_rads(void);
static int safe_cos(float angle);
static int safe_sin(float angle);
static void set_score(void);
static void table_layer_update_callback(Layer * const me, GContext * ctx);
static void timer_callback(void *data);
static void window_load(Window *window);
static void window_unload(Window *window);

#endif /* PINGCHRONG_H */
