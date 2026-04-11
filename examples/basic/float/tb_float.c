#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

// float64 arithmetic
extern double  dq_fl_add(double a, double b);
extern double  dq_fl_sub(double a, double b);
extern double  dq_fl_mul(double a, double b);
extern double  dq_fl_div(double a, double b);

// float32 arithmetic
extern float   dq_fl32_add(float a, float b);
extern float   dq_fl32_mul(float a, float b);

// round/ceil/floor on float64 -> int
extern int64_t dq_fl_round(double v);
extern int64_t dq_fl_ceil (double v);
extern int64_t dq_fl_floor(double v);

// round/ceil/floor on float32 -> int
extern int64_t dq_fl32_round(float v);
extern int64_t dq_fl32_ceil (float v);
extern int64_t dq_fl32_floor(float v);

// constant expressions
extern int64_t dq_get_const_round();
extern int64_t dq_get_const_ceil();
extern int64_t dq_get_const_floor();

static int g_pass = 0, g_fail = 0;

#define CHECK_F(label, got, expected) \
  do { \
    double _g = (double)(got), _e = (double)(expected); \
    if (_g == _e) { printf("  PASS  %-46s  %g\n", label, _g); ++g_pass; } \
    else { printf("  FAIL  %-46s  got %g, expected %g\n", label, _g, _e); ++g_fail; } \
  } while(0)

#define CHECK_I(label, got, expected) \
  do { \
    int64_t _g = (int64_t)(got), _e = (int64_t)(expected); \
    if (_g == _e) { printf("  PASS  %-46s  %"PRId64"\n", label, _g); ++g_pass; } \
    else { printf("  FAIL  %-46s  got %"PRId64", expected %"PRId64"\n", label, _g, _e); ++g_fail; } \
  } while(0)

int main()
{
  printf("Testing DQ float operations:\n\n");

  // ---- float64 arithmetic ----
  printf("--- float64 arithmetic ---\n");
  CHECK_F("dq_fl_add(1.5, 2.5)",    dq_fl_add(1.5, 2.5),    4.0);
  CHECK_F("dq_fl_add(-1.5, 2.5)",   dq_fl_add(-1.5, 2.5),   1.0);
  CHECK_F("dq_fl_sub(5.5, 2.5)",    dq_fl_sub(5.5, 2.5),    3.0);
  CHECK_F("dq_fl_sub(1.0, 3.5)",    dq_fl_sub(1.0, 3.5),   -2.5);
  CHECK_F("dq_fl_mul(2.5, 4.0)",    dq_fl_mul(2.5, 4.0),   10.0);
  CHECK_F("dq_fl_mul(-3.0, 2.5)",   dq_fl_mul(-3.0, 2.5),  -7.5);
  CHECK_F("dq_fl_div(10.0, 4.0)",   dq_fl_div(10.0, 4.0),   2.5);
  CHECK_F("dq_fl_div(-7.5, 2.5)",   dq_fl_div(-7.5, 2.5),  -3.0);

  // ---- float32 arithmetic ----
  printf("\n--- float32 arithmetic ---\n");
  CHECK_F("dq_fl32_add(1.5f, 2.5f)",  dq_fl32_add(1.5f, 2.5f),  4.0f);
  CHECK_F("dq_fl32_mul(2.5f, 4.0f)",  dq_fl32_mul(2.5f, 4.0f), 10.0f);

  // ---- round (float64) ----
  printf("\n--- round(float64) -> int ---\n");
  CHECK_I("round(1.4)",    dq_fl_round(1.4),    1);
  CHECK_I("round(1.5)",    dq_fl_round(1.5),    2);   // ties away from zero
  CHECK_I("round(1.6)",    dq_fl_round(1.6),    2);
  CHECK_I("round(2.5)",    dq_fl_round(2.5),    3);   // ties away from zero
  CHECK_I("round(-1.4)",   dq_fl_round(-1.4),  -1);
  CHECK_I("round(-1.5)",   dq_fl_round(-1.5),  -2);   // ties away from zero
  CHECK_I("round(-1.6)",   dq_fl_round(-1.6),  -2);
  CHECK_I("round(0.0)",    dq_fl_round(0.0),    0);

  // ---- ceil (float64) ----
  printf("\n--- ceil(float64) -> int ---\n");
  CHECK_I("ceil(1.0)",     dq_fl_ceil(1.0),     1);
  CHECK_I("ceil(1.1)",     dq_fl_ceil(1.1),     2);
  CHECK_I("ceil(1.9)",     dq_fl_ceil(1.9),     2);
  CHECK_I("ceil(-1.1)",    dq_fl_ceil(-1.1),   -1);
  CHECK_I("ceil(-1.9)",    dq_fl_ceil(-1.9),   -1);
  CHECK_I("ceil(0.0)",     dq_fl_ceil(0.0),     0);

  // ---- floor (float64) ----
  printf("\n--- floor(float64) -> int ---\n");
  CHECK_I("floor(1.0)",    dq_fl_floor(1.0),    1);
  CHECK_I("floor(1.1)",    dq_fl_floor(1.1),    1);
  CHECK_I("floor(1.9)",    dq_fl_floor(1.9),    1);
  CHECK_I("floor(-1.1)",   dq_fl_floor(-1.1),  -2);
  CHECK_I("floor(-1.9)",   dq_fl_floor(-1.9),  -2);
  CHECK_I("floor(0.0)",    dq_fl_floor(0.0),    0);

  // ---- round/ceil/floor (float32) ----
  printf("\n--- round/ceil/floor(float32) -> int ---\n");
  CHECK_I("round32(1.5f)",   dq_fl32_round(1.5f),   2);
  CHECK_I("round32(-1.5f)",  dq_fl32_round(-1.5f), -2);
  CHECK_I("ceil32(1.1f)",    dq_fl32_ceil(1.1f),    2);
  CHECK_I("ceil32(-1.9f)",   dq_fl32_ceil(-1.9f),  -1);
  CHECK_I("floor32(1.9f)",   dq_fl32_floor(1.9f),   1);
  CHECK_I("floor32(-1.1f)",  dq_fl32_floor(-1.1f), -2);

  // ---- constant expressions ----
  printf("\n--- constant expressions (compile-time evaluation) ---\n");
  CHECK_I("CONST_ROUND = round(3.1/1.13)", dq_get_const_round(), 3);  // round(2.7433...) = 3
  CHECK_I("CONST_CEIL  = ceil(2.1*3.0)",   dq_get_const_ceil(),  7);  // ceil(6.3) = 7
  CHECK_I("CONST_FLOOR = floor(10.0/3.0)", dq_get_const_floor(), 3);  // floor(3.333...) = 3

  printf("\n--- Summary ---\n");
  printf("Total: %d  |  PASS: %d  |  FAIL: %d\n", g_pass + g_fail, g_pass, g_fail);
  return (g_fail > 0) ? 1 : 0;
}
