// UnitTest Suite,
// created by makeUnitTestMain.pl.
// Do NOT modify!

// System
#include <stdio.h>
#include <string.h>
// Tools
#include <UnitTest.h>

// Unit CircularBufferTest:
extern TEST_CASE test_circular_buffer();
// Unit DisableFilterTest:
extern TEST_CASE test_disable_filter();
// Unit EngineConfigTest:
extern TEST_CASE engine_config();
// Unit HTTPServerTest:
extern TEST_CASE test_http_server();
// Unit ProcessVariableTest:
extern TEST_CASE process_variable();
extern TEST_CASE pv_lock_test();
// Unit SampleMechanismTest:
extern TEST_CASE test_sample_get();
extern TEST_CASE test_sample_monitor();
extern TEST_CASE test_sample_monitor_get();
// Unit ScanListTest:
extern TEST_CASE test_scan_list();
// Unit TimeFilterTest:
extern TEST_CASE test_time_filter();
extern TEST_CASE test_repeat_filter();
// Unit TimeSlotFilterTest:
extern TEST_CASE test_time_slot_filter();

int main(int argc, const char *argv[])
{
    size_t units = 0, run = 0, passed = 0;
    const char *single_unit = 0;
    const char *single_case = 0;

    if (argc > 3  || (argc > 1 && argv[1][0]=='-'))
    {
        printf("USAGE: UnitTest { Unit { case } }\n");
        printf("\n");
        printf("Per default, all test cases in all units are executed.\n");
        printf("\n");
        return -1;
    }
    if (argc >= 2)
        single_unit = argv[1];
    if (argc == 3)
        single_case = argv[2];

    if (single_unit==0  ||  strcmp(single_unit, "CircularBufferTest")==0)
    {
        printf("======================================================================\n");
        printf("Unit CircularBufferTest:\n");
        printf("----------------------------------------------------------------------\n");
        ++units;
       if (single_case==0  ||  strcmp(single_case, "test_circular_buffer")==0)
       {
            ++run;
            printf("\ntest_circular_buffer:\n");
            if (test_circular_buffer())
                ++passed;
            else
                printf("THERE WERE ERRORS!\n");
       }
    }
    if (single_unit==0  ||  strcmp(single_unit, "ProcessVariableTest")==0)
    {
        printf("======================================================================\n");
        printf("Unit ProcessVariableTest:\n");
        printf("----------------------------------------------------------------------\n");
        ++units;
       if (single_case==0  ||  strcmp(single_case, "process_variable")==0)
       {
            ++run;
            printf("\nprocess_variable:\n");
            if (process_variable())
                ++passed;
            else
                printf("THERE WERE ERRORS!\n");
       }
       if (single_case==0  ||  strcmp(single_case, "pv_lock_test")==0)
       {
            ++run;
            printf("\npv_lock_test:\n");
            if (pv_lock_test())
                ++passed;
            else
                printf("THERE WERE ERRORS!\n");
       }
    }




    if (single_unit==0  ||  strcmp(single_unit, "RepeatFilterTest")==0)
    {
        printf("======================================================================\n");
        printf("Unit RepeatFilterTest:\n");
        printf("----------------------------------------------------------------------\n");
        ++units;
       if (single_case==0  ||  strcmp(single_case, "test_repeat_filter")==0)
       {
            ++run;
            printf("\ntest_repeat_filter:\n");
            if (test_repeat_filter())
                ++passed;
            else
                printf("THERE WERE ERRORS!\n");
       }
    }


    if (single_unit==0  ||  strcmp(single_unit, "TimeFilterTest")==0)
    {
        printf("======================================================================\n");
        printf("Unit TimeFilterTest:\n");
        printf("----------------------------------------------------------------------\n");
        ++units;
       if (single_case==0  ||  strcmp(single_case, "test_time_filter")==0)
       {
            ++run;
            printf("\ntest_time_filter:\n");
            if (test_time_filter())
                ++passed;
            else
                printf("THERE WERE ERRORS!\n");
       }
    }



    printf("======================================================================\n");
    size_t failed = run - passed;
    printf("Tested %zu unit%s, ran %zu test%s, %zu passed, %zu failed.\n",
           units,
           (units > 1 ? "s" : ""),
           run,
           (run > 1 ? "s" : ""),
           passed, failed);
    printf("Success rate: %.1f%%\n", 100.0*passed/run);

    printf("==================================================\n");
    printf("--------------------------------------------------\n");
    if (failed != 0)
    {
        printf("THERE WERE ERRORS!\n");
        return -1;
    }
    printf("All is OK\n");
    printf("--------------------------------------------------\n");
    printf("==================================================\n");

    return 0;
}

