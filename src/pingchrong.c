/**
 * PingChrong watchface for the Pebble Smartwatch
 *
 * @version 2.0.0
 * @license New BSD License (please see LICENSE file)
 * @repo https://github.com/rexmac/pebble-pingchrong
 * @author Rex McConnell <rex@rexmac.com>
 */
#include <pebble.h>
#include <math.h>
#include "pingchrong.h"

#define DEBUGGING 0

// Settings (bit) flags
enum {
  SETTING_12H_TIME = 1 << 0,
  SETTING_INVERTED = 1 << 1
};

// Settings AppSync keys; correspond to appKeys in appinfo.json
enum {
  SETTING_SYNC_KEY_12H_TIME = 0,
  SETTING_SYNC_KEY_INVERTED = 1
};

static Window *window;
static TextLayer *score_layer; /**< The layer that displays the current score/time */
static Layer *table_layer; /**< The layer onto which the table is drawn */
static Layer *anim_layer; /**< The layer onto which the animation is drawn */
static AppTimer *timer; /**< Time used to schedule animation updates */
static AppSync settings_sync; /**< Keeps settings in sync between phone and watch */
static uint8_t settings_sync_buffer[32]; /**< Buffer used by settings sync */
static uint8_t settings; /**< Current settings (as bit flags) */

static uint8_t left_paddle_x; /**< Horizontal position of left paddle */
static uint8_t right_paddle_x; /**< Horizontal position of right paddle */
static int16_t left_paddle_y; /**< Vertical position of left paddle */
static int16_t right_paddle_y; /**< Vertical position of right paddle */
static int16_t left_paddle_prev_y; /**< Vertical position of left paddle in previous animation frame */
static int16_t right_paddle_prev_y; /**< Vertical position of right paddle in previous animation frame */
static GSize table_size; /**< Size (in pixels) of table layer */
static GPoint ball_pos; /**< Position of ball's center */
static GPoint ball_prev_pos; /**< Position of ball's center in previous animation frame */
static float ball_dx; /**< Horizontal vector of ball */
static float ball_dy; /**< Vertical vector of ball */
static uint8_t minute_changed, hour_changed; /**< Booleans used to denote when a unit of time has changed */
static char score[8]; /**< String to hold the current score for display */
static struct tm *current_time; /**< The current time (updated once a minute) */
static uint8_t failed; /**< Boolean used for debugging. Indicates AI failure */

// Use by the PRNG
static uint32_t rval[2]={0,0};
static uint32_t key[4];

/**
 * Wrapper around the cos_lookup function provided by the Pebble SDK.
 *
 * @param float angle Angle in radians
 * @return int The cosine of the given angle
 */
static int safe_cos(float angle) {
  uint16_t pebble_angle = (int) ((TRIG_MAX_ANGLE * angle) / (2 * 3.1416));
  return (cos_lookup(pebble_angle) * (2 * 3.1416)) / TRIG_MAX_ANGLE;
}

/**
 * Wrapper around the sin_lookup function provided by the Pebble SDK.
 *
 * @param float angle Angle in radians
 * @return int The sine of the given angle
 */
static int safe_sin(float angle) {
  uint16_t pebble_angle = (int) ((TRIG_MAX_ANGLE * angle) / (2 * 3.1416));
  return (sin_lookup(pebble_angle) * (2 * 3.1416)) / TRIG_MAX_ANGLE;
}

/**
 * Test if two recntagles intersect
 *
 * @param
 * @return
 */
static uint8_t intersectrect(uint8_t x1, uint8_t y1, uint8_t w1, uint8_t h1,
                      uint8_t x2, uint8_t y2, uint8_t w2, uint8_t h2) {
  // check x coord first
  if (x1+w1 < x2)
    return 0;
  if (x2+w2 < x1)
    return 0;

  // check the y coord second
  if (y1+h1 < y2)
    return 0;
  if (y2+h2 < y1)
    return 0;

  return 1;
}

/**
 * ???
 *
 */
static uint8_t calculate_keepout(float theball_x, float theball_y, float theball_dx, float theball_dy, uint8_t *keepout1, uint8_t *keepout2) {
//ticksremaining = calculate_keepout(ball_pos.x, ball_pos.y, ball_dx, ball_dy, &right_bouncepos, &right_endpos);

  // "simulate" the ball bounce...its not optimized (yet)
  float sim_ball_y = theball_y;
  float sim_ball_x = theball_x;
  float sim_ball_dy = theball_dy;
  float sim_ball_dx = theball_dx;

  uint8_t tix = 0, collided = 0;

  //while ((sim_ball_x < (right_paddle_x + PADDLE_W)) && ((sim_ball_x + BALL_RADIUS) > left_paddle_x)) {
  while (((sim_ball_x + BALL_RADIUS + 1) < (right_paddle_x + PADDLE_W)) && ((sim_ball_x + BALL_RADIUS) > left_paddle_x)) {
    float old_sim_ball_x = sim_ball_x;
    float old_sim_ball_y = sim_ball_y;
    sim_ball_y += sim_ball_dy;
    sim_ball_x += sim_ball_dx;

    // bouncing off bottom wall
    if (sim_ball_y  > (table_size.h - BAR_MARGIN - BAR_HEIGHT - BALL_RADIUS - 1)) {
      sim_ball_y = table_size.h - BAR_MARGIN - BAR_HEIGHT - BALL_RADIUS - 1;
      sim_ball_dy *= -1;
    }

    // bouncing off top wall
    if (sim_ball_y <  (BAR_MARGIN + BAR_HEIGHT + BALL_RADIUS)) {
      sim_ball_y = BAR_MARGIN + BAR_HEIGHT + BALL_RADIUS;
      sim_ball_dy *= -1;
    }

    if (((sim_ball_x + BALL_RADIUS + 1) >= right_paddle_x) && ((old_sim_ball_x + BALL_RADIUS + 1) < right_paddle_x)) {
      // check if we collided with the right paddle

      // first determine the exact position at which it would collide
      float dx = right_paddle_x - (old_sim_ball_x + BALL_RADIUS + 1);
      // now figure out what fraction that is of the motion and multiply that by the dy
      float dy = (dx / sim_ball_dx) * sim_ball_dy;

      if (DEBUGGING) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "RCOLL@ (%d, %d)", (int) round(old_sim_ball_x + dx), (int) round(old_sim_ball_y + dy));
      }

      *keepout1 = old_sim_ball_y + dy;
      collided = 1;
    } else if ((sim_ball_x <= (left_paddle_x + PADDLE_W)) && (old_sim_ball_x > (left_paddle_x + PADDLE_W))) {
      // check if we collided with the left paddle

      // first determine the exact position at which it would collide
      float dx = (left_paddle_x + PADDLE_W) - old_sim_ball_x;
      // now figure out what fraction that is of the motion and multiply that by the dy
      float dy = (dx / sim_ball_dx) * sim_ball_dy;

      if (DEBUGGING) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "LCOLL@ (%d,%d)", (int) round(old_sim_ball_x + dx), (int) round(old_sim_ball_y + dy));
      }

      *keepout1 = old_sim_ball_y + dy;
      collided = 1;
    }
    if (!collided) {
      tix++;
    }

    if (DEBUGGING > 2) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "\tSIMball @ [%d,%d]", (int) round(sim_ball_x), (int) round(sim_ball_y));
    }
  }
  *keepout2 = sim_ball_y;

  return tix;
}

/**
 * ???
 *
 */
static void encipher(void) {  // Using 32 rounds of XTea encryption as a PRNG.
  unsigned int i;
  uint32_t v0=rval[0], v1=rval[1], sum=0, delta=0x9E3779B9;
  for (i=0; i < 32; i++) {
    v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
    sum += delta;
    v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11) & 3]);
  }
  rval[0]=v0; rval[1]=v1;
}

/**
 * Initialize the PRNG
 *
 */
static void init_crand(void) {
  uint32_t temp;
  time_t now = time(NULL);
  struct tm *time = localtime(&now);

  key[0]=0x2DE9716E;  //Initial XTEA key. Grabbed from the first 16 bytes
  key[1]=0x993FDDD1;  //of grc.com/password.  1 in 2^128 chance of seeing
  key[2]=0x2A77FB57;  //that key again there.
  key[3]=0xB172E6B0;
  rval[0]=0;
  rval[1]=0;
  encipher();
  temp = time->tm_hour;
  temp<<=8;
  temp |= time_ms(&now, NULL);
  temp<<=8;
  temp |= time->tm_min;
  temp<<=8;
  temp |= time->tm_sec;
  key[0]^=rval[1]<<1;
  encipher();
  key[1]^=temp<<1;
  encipher();
  key[2]^=temp>>1;
  encipher();
  key[3]^=rval[1]>>1;
  encipher();
  temp = time_ms(&now, NULL);
  temp<<=8;
  temp|= time->tm_sec;
  temp<<=8;
  temp|= time->tm_hour;
  temp<<=8;
  temp|= time->tm_sec;
  key[0]^=temp<<1;
  encipher();
  key[1]^=rval[0]<<1;
  encipher();
  key[2]^=rval[0]>>1;
  encipher();
  key[3]^=temp>>1;
  rval[0]=0;
  rval[1]=0;
  encipher();        //And at this point, the PRNG is now seeded, based on power on/date/time reset.
}

/**
 * Generate a psuedo-random integer.
 *
 * @param uint8_t type ????
 * @return uint16_t Psuedo-random integer
 */
static uint16_t crand(uint8_t type) {
  if ((type == 0) || (type > 2)) {
    encipher();
    return (rval[0]^rval[1]) & RAND_MAX;
  } else if (type == 1) {
    return ((rval[0]^rval[1]) >> 15) & 3;
  } else if (type == 2) {
    return ((rval[0]^rval[1]) >> 17) & 1;
  }
  return 0;
}

/**
 * ???
 *
 */
static float random_angle_rads(void) {
  // Create random vector MEME seed it ok???
  float angle = crand(0);

  // angle = 31930; // MEME DEBUG
  if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "rand = %d", (int)angle);}
  //angle = (angle * (90.0 - MIN_BALL_ANGLE*2)  / RAND_MAX) + MIN_BALL_ANGLE;
  angle = (angle * (90.0 - MIN_BALL_ANGLE*2)  / TRIG_MAX_ANGLE) + MIN_BALL_ANGLE;

  // Pick the quadrant
  uint8_t quadrant = (crand(1)) % 4;
  //quadrant = 2; // MEME DEBUG

  if (DEBUGGING) { APP_LOG(APP_LOG_LEVEL_DEBUG, "quad = %d", quadrant); }
  angle += quadrant * 90;
  if (DEBUGGING) { APP_LOG(APP_LOG_LEVEL_DEBUG, "new ejection angle = %d", (int) angle); }

  angle *= 3.1416;
  angle /= 180;
  return angle;
}

/**
 * Set the score (i.e., the time)
 *
 */
static void set_score(void) {
  if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "set_score");}
  uint8_t hour = current_time->tm_hour;
  uint8_t min = current_time->tm_min;
  //char score[8];

  if (settings & SETTING_12H_TIME) {
    if (hour == 0) { hour = 12; }
    else if (hour > 12) { hour -= 12; }
  }

  snprintf(score, 8, "%02d  %02d", hour, min);
  //text_layer_set_text(score_layer, score);
  //layer_mark_dirty(score_layer);
}

/**
 * Draw the animation.
 *
 * @param me  Pointer to layer to be rendered
 * @param ctx The destination graphics context to draw into
 */
static void anim_layer_update_callback(Layer * const me, GContext * ctx) {
  graphics_context_set_fill_color(ctx, (settings & SETTING_INVERTED) > 0 ? GColorBlack : GColorWhite);
  graphics_context_set_stroke_color(ctx, (settings & SETTING_INVERTED) > 0 ? GColorBlack : GColorWhite);

  if (failed) {
    // Draw the ball
    graphics_fill_circle(ctx, ball_pos, BALL_RADIUS);
    // Draw the paddles
    graphics_fill_rect(ctx, GRect(left_paddle_x, left_paddle_y, PADDLE_W, PADDLE_H), 1, GCornersAll);
    graphics_fill_rect(ctx, GRect(right_paddle_x, right_paddle_y, PADDLE_W, PADDLE_H), 1, GCornersAll);
    return;
  }
  GRect bounds = layer_get_bounds(me);
  //ball_pos = GPoint(bounds.size.w / 2, bounds.size.h / 2);

  // Save old ball location so we can do some vector stuff 
  ball_prev_pos = GPoint(ball_pos.x, ball_pos.y);

  // The keepout is used to know where to -not- put the paddle
  // the 'bouncepos' is where we expect the ball's y-coord to be when
  // it intersects with the paddle area
  static uint8_t right_keepout_top, right_keepout_bot, right_bouncepos, right_endpos;
  static uint8_t left_keepout_top, left_keepout_bot, left_bouncepos, left_endpos;
  static int16_t right_dest, left_dest;
  static uint8_t ticksremaining;

  // Move the ball according to the vector
  ball_pos.x += ball_dx;
  ball_pos.y += ball_dy;

  // bouncing off bottom wall, reverse direction
  if (ball_pos.y  > (table_size.h - BAR_MARGIN - BAR_HEIGHT - BALL_RADIUS - 1)) {
    if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "Bottom wall bounce");}
    ball_pos.y = table_size.h - BAR_MARGIN - BAR_HEIGHT - BALL_RADIUS - 1;
    ball_dy *= -1;
  }
  // bouncing off top wall, reverse direction
  if (ball_pos.y  < (BAR_MARGIN + BAR_HEIGHT + BALL_RADIUS - 1)) {
    if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "Top wall bounce");}
    ball_pos.y = BAR_MARGIN + BAR_HEIGHT + BALL_RADIUS - 1;
    ball_dy *= -1;
  }

  // For debugging, print the ball location
  if (DEBUGGING > 2) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "ball @ (%d, %d)", ball_pos.x, ball_pos.y);
  }

  // If the ball hits the left or right wall, then the reset the ball and paddles
  if ((ball_pos.x  > (table_size.w - BALL_RADIUS - 1)) || (ball_pos.x <= BALL_RADIUS)) {
    if (DEBUGGING) {
      if (ball_pos.x <= BALL_RADIUS) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Left wall collide");
        if (!minute_changed) {
          APP_LOG(APP_LOG_LEVEL_DEBUG, "...on accident");
          failed = 1;
          return;
        } else {
          APP_LOG(APP_LOG_LEVEL_DEBUG, "...on purpose");
        }
      } else {
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Right wall collide");
        if (!hour_changed) {
          APP_LOG(APP_LOG_LEVEL_DEBUG, "...on accident");
          failed = 1;
          return;
        } else {
          APP_LOG(APP_LOG_LEVEL_DEBUG, "...on purpose");
        }
      }
    }

    // Reset ball position (center of screen)
    ball_pos = GPoint(bounds.size.w / 2, bounds.size.h / 2);
    float angle = random_angle_rads();
    ball_dx = MAX_BALL_SPEED * safe_cos(angle);
    ball_dy = MAX_BALL_SPEED * safe_sin(angle);

    // Reset paddle positions
    graphics_fill_rect(ctx, GRect(2, left_paddle_y, PADDLE_W, PADDLE_H), 1, GCornersAll); // left paddle
    graphics_fill_rect(ctx, GRect(bounds.size.w - PADDLE_W - 2, right_paddle_y, PADDLE_W, PADDLE_H), 1, GCornersAll); // right paddle

    // Reset scoring variables
    right_keepout_top = right_keepout_bot = 0;
    left_keepout_top = left_keepout_bot = 0;
    minute_changed = hour_changed = 0;
    set_score();
  }

  // Save the old paddle positions
  left_paddle_prev_y = left_paddle_y;
  right_paddle_prev_y = right_paddle_y;

  // Check if we are bouncing off right paddle
  if (ball_dx > 0) {
    if (((ball_pos.x + BALL_RADIUS + 1) >= right_paddle_x) && ((ball_prev_pos.x + BALL_RADIUS + 1) <= right_paddle_x)) {
      // check if we collided
      if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "coll?");}
      // determine the exact position at which it would collide
      float dx = right_paddle_x - (ball_prev_pos.x + BALL_RADIUS + 1);
      // now figure out what fraction that is of the motion and multiply that by the dy
      float dy = (dx / ball_dx) * ball_dy;

      if (intersectrect((ball_pos.x + dx - BALL_RADIUS + 1), (ball_prev_pos.y + dy - BALL_RADIUS + 1), BALL_RADIUS*2, BALL_RADIUS*2,
                      right_paddle_x, right_paddle_y, PADDLE_W, PADDLE_H)) {
        if (DEBUGGING) {
          APP_LOG(APP_LOG_LEVEL_DEBUG, "nosect");
          if (hour_changed) {
            APP_LOG(APP_LOG_LEVEL_DEBUG, "FAILED to miss");
            if (ticksremaining > 1) failed = 1;
          }

          APP_LOG(APP_LOG_LEVEL_DEBUG, "RCOLLISION  ball @ (%d, %d) & paddle @ (%d, %d)",
            (int) round(ball_prev_pos.x + dx), (int) round(ball_prev_pos.y + dy),
            right_paddle_x, right_paddle_y
          );
        }

        // set the ball right up against the paddle
        ball_pos.x = ball_prev_pos.x + dx;
        ball_pos.y = ball_prev_pos.y + dy;
        // bounce it
        ball_dx *= -1;

        right_bouncepos = right_dest = right_keepout_top = right_keepout_bot = 0;
        left_bouncepos = left_dest = left_keepout_top = left_keepout_bot = 0;
      }
      // otherwise, it didn't bounce...will probably hit the right wall
      if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, " tix = %d", ticksremaining);}
    }

    if ((ball_dx > 0) && ((ball_pos.x + BALL_RADIUS + 1) < right_paddle_x)) {
      // ball is coming towards the right paddle

      if (right_keepout_top == 0) {
        ticksremaining = calculate_keepout(ball_pos.x, ball_pos.y, ball_dx, ball_dy, &right_bouncepos, &right_endpos);
        if (DEBUGGING) {
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Expect bounce @ %d -> %d in %d tix", right_bouncepos, right_endpos, ticksremaining);
        }
        if (right_bouncepos > right_endpos) {
          right_keepout_top = right_endpos;
          right_keepout_bot = right_bouncepos + BALL_RADIUS - 1;
        } else {
          right_keepout_top = right_bouncepos;
          right_keepout_bot = right_endpos + BALL_RADIUS - 1;
        }
        if (DEBUGGING) {
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Keepout from %d to %d" , right_keepout_top, right_keepout_bot);
        }

        // Now we can calculate where the paddle should go
        if (!hour_changed) {
          // we want to hit the ball, so make it centered
          right_dest = right_bouncepos + BALL_RADIUS - (PADDLE_H/2);
          if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "hitR -> %d", right_dest);}
        } else {
          // we lost the round so make sure we -dont- hit the ball
          if (right_keepout_top <= (BAR_MARGIN + BAR_HEIGHT + PADDLE_H)) {
            // the ball is near the top so make sure it ends up right below it
            if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "at the top");}
            right_dest = right_keepout_bot + 2;
          } else if (right_keepout_bot >= (table_size.h - BAR_MARGIN - BAR_HEIGHT - PADDLE_H - 2)) {
            // the ball is near the bottom so make sure it ends up right above it
            if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "at the bottom");}
            right_dest = right_keepout_top - PADDLE_H - 2;
          } else {
            if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "in the middle");}
            if ( ((uint8_t)crand(2)) & 0x1)
              right_dest = right_keepout_top - PADDLE_H - 2;
            else
              right_dest = right_keepout_bot + 2;
          }
          if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "missR -> %d", right_dest); }
        }
      } else {
        ticksremaining--;
      }

      // draw the keepout area (for debugging)
      if (DEBUGGING) {
        graphics_draw_rect(ctx, GRect(right_paddle_x, right_keepout_top, PADDLE_W, right_keepout_bot - right_keepout_top));
      }

      int16_t distance = right_paddle_y - right_dest;

      if (DEBUGGING > 1) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "\tdest dist: %d", abs(distance));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "\ttix: %d", ticksremaining);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "\tmax travel: %d", ticksremaining * MAX_PADDLE_SPEED);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "\tright paddle @ %d\n", right_paddle_y);
      }

      // if we have just enough time, move the paddle!
      if (abs(distance) > (ticksremaining-1) * MAX_PADDLE_SPEED) {
        if (DEBUGGING > 1) {
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Right paddle should begin moving now!");
        }
        distance = abs(distance);
        if (right_dest > right_paddle_y ) {
          if (DEBUGGING > 1) {
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Right paddle should begin moving down!");
          }
          if (distance > MAX_PADDLE_SPEED)
            right_paddle_y += MAX_PADDLE_SPEED;
          else
            right_paddle_y += distance;
        }
        if (right_dest < right_paddle_y) {
          if (DEBUGGING > 1) {
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Right paddle should begin moving up!");
          }
          if (distance > MAX_PADDLE_SPEED)
            right_paddle_y -= MAX_PADDLE_SPEED;
          else
            right_paddle_y -= distance;
        }
      }
    }
  } else {
    // check if we are bouncing off left paddle
    if ((ball_pos.x <= (left_paddle_x + PADDLE_W + BALL_RADIUS)) && (ball_prev_pos.x >= (left_paddle_x + PADDLE_W + BALL_RADIUS))) {
      // check if we collided
      if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "coll?");}
      // determine the exact position at which it would collide
      float dx = (left_paddle_x + PADDLE_W + BALL_RADIUS) - ball_prev_pos.x;
      // now figure out what fraction that is of the motion and multiply that by the dy
      float dy = (dx / ball_dx) * ball_dy;

      if (intersectrect((ball_prev_pos.x + dx - BALL_RADIUS), (ball_prev_pos.y + dy - BALL_RADIUS), BALL_RADIUS*2, BALL_RADIUS*2,
                      left_paddle_x, left_paddle_y, PADDLE_W, PADDLE_H)) {
        if (DEBUGGING) {
          APP_LOG(APP_LOG_LEVEL_DEBUG, "nosect");
          if (minute_changed) {
            APP_LOG(APP_LOG_LEVEL_DEBUG, "FAILED to miss");
            if (ticksremaining > 1) failed = 1;
          }
          APP_LOG(APP_LOG_LEVEL_DEBUG, "LCOLLISION ball @ (%d, %d) & paddle @ (%d, %d)",
            (int) round(ball_prev_pos.x + dx), (int) round(ball_prev_pos.y + dy), left_paddle_x, left_paddle_y);
        }

        // bounce it
        ball_dx *= -1;

        if (ball_pos.x != left_paddle_x + PADDLE_W + BALL_RADIUS) {
          // set the ball right up against the paddle
          ball_pos.x = ball_prev_pos.x + dx;
          ball_pos.y = ball_prev_pos.y + dy;
        }
        left_bouncepos = left_dest = left_keepout_top = left_keepout_bot = 0;
      }
      // otherwise, it didn't bounce...will probably hit the left wall
      if (DEBUGGING) { APP_LOG(APP_LOG_LEVEL_DEBUG, " tix = %d", ticksremaining); }
    }

    if ((ball_dx < 0) && (ball_pos.x > (left_paddle_x + BALL_RADIUS))) {
      // ball is coming towards the left paddle

      if (left_keepout_top == 0 ) {
        ticksremaining = calculate_keepout(ball_pos.x, ball_pos.y, ball_dx, ball_dy, &left_bouncepos, &left_endpos);
        if (DEBUGGING) {
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Expect bounce @ %d -> %d in %d tix", left_bouncepos, left_endpos, ticksremaining);
        }

        if (left_bouncepos > left_endpos) {
          left_keepout_top = left_endpos;
          left_keepout_bot = left_bouncepos + BALL_RADIUS;
        } else {
          left_keepout_top = left_bouncepos;
          left_keepout_bot = left_endpos + BALL_RADIUS;
        }
        if (DEBUGGING) {
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Keepout from %d to %d", left_keepout_top, left_keepout_bot);
        }

        // Now we can calculate where the paddle should go
        if (!minute_changed) {
          // we want to hit the ball, so make it centered
          left_dest = left_bouncepos + BALL_RADIUS - (PADDLE_H / 2);
          if (DEBUGGING) {
            APP_LOG(APP_LOG_LEVEL_DEBUG, "hitL -> %d", left_dest);
          }
        } else {
          // we lost the round so make sure we -dont- hit the ball
          if (left_keepout_top <= (BAR_MARGIN + BAR_HEIGHT + PADDLE_H)) {
            // the ball is near the top so make sure it ends up right below it
            if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "at the top");}
            left_dest = left_keepout_bot + 2;
          } else if (left_keepout_bot >= (table_size.h - BAR_MARGIN - BAR_HEIGHT - PADDLE_H - 2)) {
            // the ball is near the bottom so make sure it ends up right above it
            if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "at the bottom");}
            left_dest = left_keepout_top - PADDLE_H - 2;
          } else {
            if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "in the middle");}
            if (((uint8_t)crand(2)) & 0x1)
              left_dest = left_keepout_top - PADDLE_H - 2;
            else
              left_dest = left_keepout_bot + 2;
          }
          if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "missL -> %d", left_dest);}
        }
      } else {
        ticksremaining--;
      }
      // draw the keepout area (for debugging)
      if (DEBUGGING) {
        graphics_draw_rect(ctx, GRect(left_paddle_x, left_keepout_top, PADDLE_W, left_keepout_bot - left_keepout_top));
      }

      int16_t distance = abs(left_paddle_y - left_dest);

      if (DEBUGGING > 1) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "\tdest dist: %d", abs(distance));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "\ttix: %d", ticksremaining);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "\tmax travel: %d", ticksremaining * MAX_PADDLE_SPEED);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "\tleft paddle @ %d", left_paddle_y);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "\thitL -> %d\n", left_dest);
      }

      // if we have just enough time, move the paddle!
      if (abs(distance) > ((ticksremaining - 1) * MAX_PADDLE_SPEED)) {
        if (DEBUGGING > 1) {
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Left paddle should begin moving now!");
        }
        distance = abs(distance);
        if (left_dest > left_paddle_y) {
          if (DEBUGGING > 1) {
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Left paddle should begin moving down!");
          }
          if (distance > MAX_PADDLE_SPEED)
            left_paddle_y += MAX_PADDLE_SPEED;
          else
            left_paddle_y += distance;
        }
        if (left_dest < left_paddle_y) {
          if (DEBUGGING > 1) {
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Left paddle should begin moving up!");
          }
          if (distance > MAX_PADDLE_SPEED)
            left_paddle_y -= MAX_PADDLE_SPEED;
          else
            left_paddle_y -= distance;
        }
        if (DEBUGGING > 1) {
          APP_LOG(APP_LOG_LEVEL_DEBUG, "\tleft paddle now @ %d", left_paddle_y);
        }
      }
    }
  }

  // make sure the paddles dont hit the top or bottom
  if (left_paddle_y < BAR_MARGIN + BAR_HEIGHT - 1)
    left_paddle_y = BAR_MARGIN + BAR_HEIGHT - 1;
  if (right_paddle_y < BAR_MARGIN + BAR_HEIGHT - 1)
    right_paddle_y = BAR_MARGIN + BAR_HEIGHT - 1;

  if (left_paddle_y > (table_size.h - PADDLE_H - BAR_MARGIN - BAR_HEIGHT - 1))
    left_paddle_y = (table_size.h - PADDLE_H - BAR_MARGIN - BAR_HEIGHT - 1);
  if (right_paddle_y > (table_size.h - PADDLE_H - BAR_MARGIN - BAR_HEIGHT - 1))
    right_paddle_y = (table_size.h - PADDLE_H - BAR_MARGIN - BAR_HEIGHT - 1);

  // Draw the ball
  graphics_fill_circle(ctx, ball_pos, BALL_RADIUS);

  // Draw the paddles
  graphics_fill_rect(ctx, GRect(left_paddle_x, left_paddle_y, PADDLE_W, PADDLE_H), 1, GCornersAll);
  graphics_fill_rect(ctx, GRect(right_paddle_x, right_paddle_y, PADDLE_W, PADDLE_H), 1, GCornersAll);
}

/**
 * Draw the table.
 *
 * @param me  Pointer to layer to be rendered
 * @param ctx The destination graphics context to draw into
 */
static void table_layer_update_callback(Layer * const me, GContext * ctx) {
  graphics_context_set_fill_color(ctx, (settings & SETTING_INVERTED) > 0 ? GColorBlack : GColorWhite);
  GRect bounds = layer_get_bounds(me);

  // Draw the top and bottom lines
  graphics_fill_rect(ctx, GRect(0, BAR_MARGIN, bounds.size.w, BAR_HEIGHT), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(0, bounds.size.h - BAR_HEIGHT - BAR_MARGIN, bounds.size.w, BAR_HEIGHT), 0, GCornerNone);

  // Draw the center line
  uint8_t i;
  uint8_t mid_x = bounds.size.w / 2;
  uint8_t stipple_height = (bounds.size.h - BAR_MARGIN*2 - BAR_HEIGHT*2) / 8; // screen height minus height of top and bottom lines divided by eight
  uint8_t half_stipple_gap_height = stipple_height / 4; // half of the space between stipples
  stipple_height = stipple_height / 2;
  uint8_t y = BAR_MARGIN + BAR_HEIGHT; // begin immediately after top line

  for (i = 0; i < 8; i++) {
    y += half_stipple_gap_height;
    graphics_fill_rect(ctx, GRect(mid_x, y, 1, stipple_height), 0, GCornerNone);
    y += stipple_height + half_stipple_gap_height;
  }
}

/**
 * Called once per minute to update time display.
 *
 * @param tick_time     The time at which the tick event was triggered
 * @param units_changes Which unit change triggered this tick event
 */
static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  current_time = tick_time;

  // The animation layer callback needs to know which unit of time changed to determine which side should lose the point
  if ((units_changed & HOUR_UNIT) == HOUR_UNIT) {
    if (DEBUGGING) { APP_LOG(APP_LOG_LEVEL_DEBUG, "\n\n!!! hour changed !!!\n\n"); }
    hour_changed = 1;
  } else if ((units_changed & MINUTE_UNIT) == MINUTE_UNIT) {
    if (DEBUGGING) { APP_LOG(APP_LOG_LEVEL_DEBUG, "\n\n!!! minute changed !!!\n\n"); }
    minute_changed = 1;
  }
}

/**
 * Animation timer. Update animation layer.
 *
 */
static void timer_callback(void *data) {
  // Update animation layer
  layer_mark_dirty(anim_layer);

  // Schedule the next update
  const uint32_t timeout_ms = ANIM_FRAME_TIME;
  timer = app_timer_register(timeout_ms, timer_callback, NULL);
}

/**
 * Called when there is a settings sync error.
 *
 * @see https://developer.getpebble.com/2/api-reference/group___app_sync.html#ga144a1a8d8050f8f279b11cfb5d526212
 */
static void settings_sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
}

/**
 * Called when a settings tuple has changed.
 *
 * @todo only update if new_tuple != old_tuple?
 *
 * @see https://developer.getpebble.com/2/api-reference/group___app_sync.html#ga448af36883189f6345cc7d5cd8a3cc29
 */
static void settings_sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
  switch (key) {
    case SETTING_SYNC_KEY_12H_TIME:
      if (0 == ((uint8_t) new_tuple->value->uint8)) settings = settings & ~SETTING_12H_TIME;
      else settings = settings | SETTING_12H_TIME;

      time_t now = time(NULL);
      current_time = localtime(&now);
      set_score();

      break;
    case SETTING_SYNC_KEY_INVERTED:
      if (0 == ((uint8_t) new_tuple->value->uint8)) settings = settings & ~SETTING_INVERTED;
      else settings = settings | SETTING_INVERTED;

      text_layer_set_text_color(score_layer, (settings & SETTING_INVERTED) > 0 ? GColorBlack : GColorWhite);
      window_set_background_color(window, (settings & SETTING_INVERTED) > 0 ? GColorWhite : GColorBlack);

      break;
  }
}

/**
 * Called when the window is pushed to the screen when it's not loaded.
 *
 * Create layout.
 *
 * @param window Pointer to Window object
 */
static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Initialize score layer
  failed = 0;
  time_t now = time(NULL);
  current_time = localtime(&now);
  set_score();
  score_layer = text_layer_create(GRect(0, 0, bounds.size.w, 20));
  text_layer_set_text(score_layer, score);
  text_layer_set_text_alignment(score_layer, GTextAlignmentCenter);
  text_layer_set_background_color(score_layer, GColorClear);
  text_layer_set_text_color(score_layer, (settings & SETTING_INVERTED) > 0 ? GColorBlack : GColorWhite);
  layer_add_child(window_layer, text_layer_get_layer(score_layer));

  // Initialize a graphics layer for the table
  table_layer = layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  table_size = GSize(bounds.size.w, bounds.size.h);
  if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "table_layer bounds = %d, %d", bounds.size.w, bounds.size.h);}
  if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "table_size = %d, %d", table_size.w, table_size.h);}
  layer_set_update_proc(table_layer, table_layer_update_callback);
  layer_add_child(window_layer, table_layer);

  // Initialize a graphics layer for the animation
  minute_changed = 0;
  hour_changed = 0;
  left_paddle_x = PADDLE_MARGIN;
  right_paddle_x = bounds.size.w - PADDLE_W - PADDLE_MARGIN;
  left_paddle_y = (bounds.size.h - PADDLE_H) / 2;
  right_paddle_y = (bounds.size.h - PADDLE_H) / 2;
  anim_layer = layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));

  float angle = random_angle_rads();
  ball_pos = GPoint(bounds.size.w / 2, bounds.size.h / 2);
  ball_dx = MAX_BALL_SPEED * safe_cos(angle);
  ball_dy = MAX_BALL_SPEED * safe_sin(angle);
  if (DEBUGGING) {APP_LOG(APP_LOG_LEVEL_DEBUG, "Initial (dx,dy) = (%d,%d)", (int) ball_dx, (int) ball_dy);}
  layer_set_update_proc(anim_layer, anim_layer_update_callback);
  layer_add_child(window_layer, anim_layer);

  // Subscribe to tick timer service to update watchface every minute
  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
}

/**
 * Called when the window is de-initialized.
 *
 * Perform clean-up.
 *
 * @param window Pointer to Window object
 */
static void window_unload(Window *window) {
  text_layer_destroy(score_layer);
  layer_destroy(anim_layer);
  layer_destroy(table_layer);
}

/**
 * Initialize the app
 *
 */
static void init(void) {
  settings = 0;

  // Initialize PRNG
  init_crand();

  // Initialize window
  window = window_create();
  window_set_background_color(window, (settings & SETTING_INVERTED) > 0 ? GColorWhite : GColorBlack);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(window, true);

  // Load settings and init sync with JS app on phone
  Tuplet initial_settings[] = {
    TupletInteger(SETTING_SYNC_KEY_12H_TIME, 0),
    TupletInteger(SETTING_SYNC_KEY_INVERTED, 0)
  };
  app_sync_init(&settings_sync, settings_sync_buffer, sizeof(settings_sync_buffer), initial_settings, ARRAY_LENGTH(initial_settings),
    settings_sync_tuple_changed_callback, settings_sync_error_callback, NULL
  );
  app_message_open(64, 64);

  // Schedule animation update
  const uint32_t timeout_ms = ANIM_FRAME_TIME;
  timer = app_timer_register(timeout_ms, timer_callback, NULL);
}

/**
 * De-initialize the app
 *
 */
static void deinit(void) {
  //app_sync_deinit(&settings_sync);
  tick_timer_service_unsubscribe();
  window_destroy(window);
}

/**
 * App entry point.
 *
 */
int main(void) {
  init();

  if (DEBUGGING) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);
  }

  app_event_loop();
  deinit();
}
