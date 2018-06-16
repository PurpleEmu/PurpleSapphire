#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>
#include <functional>
#include <string>
#include <cstdlib>
#include <iostream>
#include <climits>
#include <cmath>
#include <cassert>

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;

#define printf(...) do{ printf(__VA_ARGS__); fflush(stdout);} while(0)