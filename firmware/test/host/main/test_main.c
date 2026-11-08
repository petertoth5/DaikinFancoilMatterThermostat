#include <stdlib.h>
#include "unity.h"
#include "unity_test_runner.h"

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    exit(UNITY_END() == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
