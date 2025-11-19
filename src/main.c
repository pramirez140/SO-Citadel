#include "maester.h"
#include "helper.h"

// Global variable for signal handling (allowed only in file with main())
volatile sig_atomic_t g_should_exit = 0;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        write_str(STDERR_FILENO, "Usage: ");
        write_str(STDERR_FILENO, argv[0]);
        write_str(STDERR_FILENO, " <config.dat> <stock.db>\n");
        return 1;
    }
    
    return maester_run(argv[1], argv[2]);
}
