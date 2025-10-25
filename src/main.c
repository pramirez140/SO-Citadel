#include "maester.h"
#include "helper.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        write_str(STDERR_FILENO, "Usage: ");
        write_str(STDERR_FILENO, argv[0]);
        write_str(STDERR_FILENO, " <config.dat> <stock.db>\n");
        return 1;
    }
    
    return maester_run(argv[1], argv[2]);
}
