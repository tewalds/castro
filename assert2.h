
#pragma once

#include <cassert>
#include "string.h"

#define assert2(expr, str) \
  ((expr)								\
   ? __ASSERT_VOID_CAST (0)						\
   : __assert_fail ((string(__STRING(expr)) + "; " + (str)).c_str(), __FILE__, __LINE__, __ASSERT_FUNCTION))

