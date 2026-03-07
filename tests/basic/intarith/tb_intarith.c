#include <stdio.h>
#include <stdint.h>

// Signed types
extern int8_t   dq_add_i8 (int8_t  a, int8_t  b);
extern int8_t   dq_sub_i8 (int8_t  a, int8_t  b);
extern int8_t   dq_mul_i8 (int8_t  a, int8_t  b);
extern int8_t   dq_idiv_i8(int8_t  a, int8_t  b);
extern int8_t   dq_imod_i8(int8_t  a, int8_t  b);

extern int16_t  dq_add_i16 (int16_t a, int16_t b);
extern int16_t  dq_sub_i16 (int16_t a, int16_t b);
extern int16_t  dq_mul_i16 (int16_t a, int16_t b);
extern int16_t  dq_idiv_i16(int16_t a, int16_t b);
extern int16_t  dq_imod_i16(int16_t a, int16_t b);

extern int32_t  dq_add_i32 (int32_t a, int32_t b);
extern int32_t  dq_sub_i32 (int32_t a, int32_t b);
extern int32_t  dq_mul_i32 (int32_t a, int32_t b);
extern int32_t  dq_idiv_i32(int32_t a, int32_t b);
extern int32_t  dq_imod_i32(int32_t a, int32_t b);

extern int64_t  dq_add_i64 (int64_t a, int64_t b);
extern int64_t  dq_sub_i64 (int64_t a, int64_t b);
extern int64_t  dq_mul_i64 (int64_t a, int64_t b);
extern int64_t  dq_idiv_i64(int64_t a, int64_t b);
extern int64_t  dq_imod_i64(int64_t a, int64_t b);

// Unsigned types
extern uint8_t  dq_add_u8 (uint8_t  a, uint8_t  b);
extern uint8_t  dq_sub_u8 (uint8_t  a, uint8_t  b);
extern uint8_t  dq_mul_u8 (uint8_t  a, uint8_t  b);
extern uint8_t  dq_idiv_u8(uint8_t  a, uint8_t  b);
extern uint8_t  dq_imod_u8(uint8_t  a, uint8_t  b);

extern uint16_t dq_add_u16 (uint16_t a, uint16_t b);
extern uint16_t dq_sub_u16 (uint16_t a, uint16_t b);
extern uint16_t dq_mul_u16 (uint16_t a, uint16_t b);
extern uint16_t dq_idiv_u16(uint16_t a, uint16_t b);
extern uint16_t dq_imod_u16(uint16_t a, uint16_t b);

extern uint32_t dq_add_u32 (uint32_t a, uint32_t b);
extern uint32_t dq_sub_u32 (uint32_t a, uint32_t b);
extern uint32_t dq_mul_u32 (uint32_t a, uint32_t b);
extern uint32_t dq_idiv_u32(uint32_t a, uint32_t b);
extern uint32_t dq_imod_u32(uint32_t a, uint32_t b);

extern uint64_t dq_add_u64 (uint64_t a, uint64_t b);
extern uint64_t dq_sub_u64 (uint64_t a, uint64_t b);
extern uint64_t dq_mul_u64 (uint64_t a, uint64_t b);
extern uint64_t dq_idiv_u64(uint64_t a, uint64_t b);
extern uint64_t dq_imod_u64(uint64_t a, uint64_t b);

static int g_pass = 0, g_fail = 0;

#define CHECK_S(label, got, expected) \
  do { \
    int64_t _g = (int64_t)(got), _e = (int64_t)(expected); \
    if (_g == _e) { printf("  PASS  %-46s  %lld\n", label, _g); ++g_pass; } \
    else { printf("  FAIL  %-46s  got %lld, expected %lld\n", label, _g, _e); ++g_fail; } \
  } while(0)

#define CHECK_U(label, got, expected) \
  do { \
    uint64_t _g = (uint64_t)(got), _e = (uint64_t)(expected); \
    if (_g == _e) { printf("  PASS  %-46s  %llu\n", label, _g); ++g_pass; } \
    else { printf("  FAIL  %-46s  got %llu, expected %llu\n", label, _g, _e); ++g_fail; } \
  } while(0)

int main()
{
  printf("Testing DQ integer arithmetic:\n\n");

  // ---- int8 ----
  printf("--- int8 ---\n");
  CHECK_S("dq_add_i8(30, 40)",            dq_add_i8(30, 40),           70);
  CHECK_S("dq_sub_i8(50, 13)",            dq_sub_i8(50, 13),           37);
  CHECK_S("dq_mul_i8(6, 7)",              dq_mul_i8(6, 7),             42);
  CHECK_S("dq_idiv_i8(100, 5)",           dq_idiv_i8(100, 5),          20);
  CHECK_S("dq_imod_i8(17, 5)",            dq_imod_i8(17, 5),            2);
  // negative values
  CHECK_S("dq_add_i8(-10, -20)",          dq_add_i8(-10, -20),        -30);
  CHECK_S("dq_sub_i8(5, 10)",             dq_sub_i8(5, 10),            -5);
  CHECK_S("dq_mul_i8(-3, 4)",             dq_mul_i8(-3, 4),           -12);
  CHECK_S("dq_idiv_i8(-20, 3)",           dq_idiv_i8(-20, 3),          -6);  // truncates toward zero
  CHECK_S("dq_imod_i8(-17, 5)",           dq_imod_i8(-17, 5),          -2);  // sign follows dividend (SRem)
  // overflow wrap: 120+10=130, (int8_t)130 = -126
  CHECK_S("dq_add_i8(120, 10) [wrap]",    dq_add_i8(120, 10),  (int8_t)130);
  // overflow wrap: 15*10=150, (int8_t)150 = -106
  CHECK_S("dq_mul_i8(15, 10) [wrap]",     dq_mul_i8(15, 10),   (int8_t)150);

  // ---- uint8 ----
  printf("\n--- uint8 ---\n");
  CHECK_U("dq_add_u8(100, 50)",           dq_add_u8(100, 50),         150);
  CHECK_U("dq_sub_u8(200, 50)",           dq_sub_u8(200, 50),         150);
  CHECK_U("dq_mul_u8(10, 12)",            dq_mul_u8(10, 12),          120);
  CHECK_U("dq_idiv_u8(100, 4)",           dq_idiv_u8(100, 4),          25);  // values < 128: SDiv == UDiv
  CHECK_U("dq_imod_u8(200, 13)",          dq_imod_u8(200, 13),          5);  // URem: 200 mod 13 = 5
  // overflow wrap: 200+100=300, (uint8_t)300 = 44
  CHECK_U("dq_add_u8(200, 100) [wrap]",   dq_add_u8(200, 100), (uint8_t)(300u));
  // underflow wrap: 10-20=-10, (uint8_t)(-10) = 246
  CHECK_U("dq_sub_u8(10, 20) [wrap]",     dq_sub_u8(10, 20),   (uint8_t)(-10));
  // IDIV uses signed semantics: 200 as int8=-56, -56/4=-14, (uint8_t)(-14)=242
  CHECK_U("dq_idiv_u8(200, 4) [signed]",  dq_idiv_u8(200, 4),         242);

  // ---- int16 ----
  printf("\n--- int16 ---\n");
  CHECK_S("dq_add_i16(1000, 2000)",       dq_add_i16(1000, 2000),    3000);
  CHECK_S("dq_sub_i16(5000, 2000)",       dq_sub_i16(5000, 2000),    3000);
  CHECK_S("dq_mul_i16(100, 200)",         dq_mul_i16(100, 200),     20000);
  CHECK_S("dq_idiv_i16(10000, 7)",        dq_idiv_i16(10000, 7),     1428);
  CHECK_S("dq_imod_i16(10000, 7)",        dq_imod_i16(10000, 7),        4);  // 10000 - 1428*7 = 4
  // negative values
  CHECK_S("dq_add_i16(-1000, -2000)",     dq_add_i16(-1000, -2000), -3000);
  CHECK_S("dq_idiv_i16(-10000, 7)",       dq_idiv_i16(-10000, 7),   -1428);  // truncates toward zero
  CHECK_S("dq_imod_i16(-10000, 7)",       dq_imod_i16(-10000, 7),      -4);  // sign follows dividend
  // overflow wrap: 30000+5000=35000, (int16_t)35000 = -30536
  CHECK_S("dq_add_i16(30000, 5000) [wrap]", dq_add_i16(30000, 5000), (int16_t)35000);

  // ---- uint16 ----
  printf("\n--- uint16 ---\n");
  CHECK_U("dq_add_u16(40000, 20000)",     dq_add_u16(40000, 20000),  60000);
  CHECK_U("dq_sub_u16(50000, 10000)",     dq_sub_u16(50000, 10000),  40000);
  CHECK_U("dq_mul_u16(200, 200)",         dq_mul_u16(200, 200),      40000);
  CHECK_U("dq_idiv_u16(30000, 7)",        dq_idiv_u16(30000, 7),      4285);  // values < 32768: SDiv == UDiv
  CHECK_U("dq_imod_u16(60000, 7)",        dq_imod_u16(60000, 7),         3);  // URem: 60000 mod 7 = 3
  // underflow wrap: 100-200=-100, (uint16_t)(-100) = 65436
  CHECK_U("dq_sub_u16(100, 200) [wrap]",  dq_sub_u16(100, 200),  (uint16_t)(-100));
  // overflow wrap: 60000+10000=70000, (uint16_t)70000 = 4464
  CHECK_U("dq_add_u16(60000, 10000) [wrap]", dq_add_u16(60000, 10000), (uint16_t)(70000u));

  // ---- int32 ----
  printf("\n--- int32 ---\n");
  CHECK_S("dq_add_i32(1000000, 2000000)",    dq_add_i32(1000000, 2000000),     3000000);
  CHECK_S("dq_sub_i32(5000000, 2000000)",    dq_sub_i32(5000000, 2000000),     3000000);
  CHECK_S("dq_mul_i32(10000, 20000)",        dq_mul_i32(10000, 20000),       200000000);
  CHECK_S("dq_idiv_i32(1000000, 7)",         dq_idiv_i32(1000000, 7),          142857);
  CHECK_S("dq_imod_i32(1000000, 7)",         dq_imod_i32(1000000, 7),               1);  // 1000000 - 142857*7 = 1
  // negative values
  CHECK_S("dq_add_i32(-1000000, -2000000)",  dq_add_i32(-1000000, -2000000),  -3000000);
  CHECK_S("dq_idiv_i32(-1000000, 7)",        dq_idiv_i32(-1000000, 7),         -142857);  // truncates toward zero
  CHECK_S("dq_imod_i32(-1000000, 7)",        dq_imod_i32(-1000000, 7),              -1);  // sign follows dividend
  // overflow wrap: 2000000000+200000000=2200000000, (int32_t)2200000000 = -2094967296
  CHECK_S("dq_add_i32(2e9, 2e8) [wrap]",     dq_add_i32(2000000000, 200000000), (int32_t)2200000000u);

  // ---- uint32 ----
  printf("\n--- uint32 ---\n");
  CHECK_U("dq_add_u32(2000000000, 1000000000)", dq_add_u32(2000000000u, 1000000000u), 3000000000u);
  CHECK_U("dq_sub_u32(3000000000, 1000000000)", dq_sub_u32(3000000000u, 1000000000u), 2000000000u);
  CHECK_U("dq_mul_u32(50000, 60000)",           dq_mul_u32(50000u, 60000u),           3000000000u);
  CHECK_U("dq_idiv_u32(1000000000, 7)",         dq_idiv_u32(1000000000u, 7u),         142857142u);  // values < INT32_MAX: SDiv == UDiv
  CHECK_U("dq_imod_u32(3000000000, 7)",         dq_imod_u32(3000000000u, 7u),                4u);  // URem: 3e9 mod 7 = 4
  // overflow wrap: 3e9+2e9=5e9, (uint32_t)5000000000 = 705032704
  CHECK_U("dq_add_u32(3e9, 2e9) [wrap]",        dq_add_u32(3000000000u, 2000000000u), (uint32_t)(5000000000ull));
  // underflow wrap: 100-200=-100, (uint32_t)(-100) = 4294967196
  CHECK_U("dq_sub_u32(100, 200) [wrap]",         dq_sub_u32(100u, 200u),               (uint32_t)(-100));

  // ---- int64 ----
  printf("\n--- int64 ---\n");
  CHECK_S("dq_add_i64(1e12, 2e12)",      dq_add_i64(1000000000000LL, 2000000000000LL),   3000000000000LL);
  CHECK_S("dq_sub_i64(5e12, 2e12)",      dq_sub_i64(5000000000000LL, 2000000000000LL),   3000000000000LL);
  CHECK_S("dq_mul_i64(1e9, 1e9)",        dq_mul_i64(1000000000LL, 1000000000LL),         1000000000000000000LL);
  CHECK_S("dq_idiv_i64(1e18, 7)",        dq_idiv_i64(1000000000000000000LL, 7LL),        142857142857142857LL);
  CHECK_S("dq_imod_i64(1e18, 7)",        dq_imod_i64(1000000000000000000LL, 7LL),                         1LL);  // 1e18 - 142857142857142857*7 = 1
  // negative values
  CHECK_S("dq_add_i64(-1e12, -2e12)",    dq_add_i64(-1000000000000LL, -2000000000000LL), -3000000000000LL);
  CHECK_S("dq_idiv_i64(-1e18, 7)",       dq_idiv_i64(-1000000000000000000LL, 7LL),       -142857142857142857LL);
  CHECK_S("dq_imod_i64(-1e18, 7)",       dq_imod_i64(-1000000000000000000LL, 7LL),                        -1LL);  // sign follows dividend

  // ---- uint64 ----
  printf("\n--- uint64 ---\n");
  CHECK_U("dq_add_u64(1e19, 5e18)",      dq_add_u64(10000000000000000000ULL, 5000000000000000000ULL),  15000000000000000000ULL);
  CHECK_U("dq_sub_u64(15e18, 5e18)",     dq_sub_u64(15000000000000000000ULL, 5000000000000000000ULL),  10000000000000000000ULL);
  CHECK_U("dq_mul_u64(1e9, 1e9)",        dq_mul_u64(1000000000ULL, 1000000000ULL),                     1000000000000000000ULL);
  CHECK_U("dq_idiv_u64(1e18, 7)",        dq_idiv_u64(1000000000000000000ULL, 7ULL),                    142857142857142857ULL);
  CHECK_U("dq_imod_u64(1e19, 7)",        dq_imod_u64(10000000000000000000ULL, 7ULL),                                    3ULL);  // URem: 1e19 mod 7 = 3
  // overflow wrap: 1e19+1e19=2e19, 2e19 mod 2^64 = 1553255926290448384
  CHECK_U("dq_add_u64(1e19, 1e19) [wrap]", dq_add_u64(10000000000000000000ULL, 10000000000000000000ULL), 1553255926290448384ULL);
  // underflow wrap: 100-200=-100, (uint64_t)(-100) = 18446744073709551516
  CHECK_U("dq_sub_u64(100, 200) [wrap]", dq_sub_u64(100ULL, 200ULL),                                   (uint64_t)(-100LL));

  printf("\n--- Summary ---\n");
  printf("Total: %d  |  PASS: %d  |  FAIL: %d\n", g_pass + g_fail, g_pass, g_fail);
  return (g_fail > 0) ? 1 : 0;
}
