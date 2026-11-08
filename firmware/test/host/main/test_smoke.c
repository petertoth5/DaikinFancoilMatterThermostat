#include "unity.h"

TEST_CASE("host runner executes tests", "[smoke]")
{
    TEST_ASSERT_EQUAL_INT(4, 2 + 2);
}
