#include "helper.h"
#include "maester.h"
#include <signal.h>
#include <errno.h>

// Global variables for signal handling
static Maester* global_maester = NULL;
static volatile sig_atomic_t should_exit = 0;

// Signal handler - ONLY sets a flag (async-signal-safe)
void handle_sigint(int sig) {
    (void)sig;
    should_exit = 1;
    // Using write() is safe in signal handlers (unlike printf)
    write(STDOUT_FILENO, "\nReceived SIGINT. Shutting down gracefully...\n", 46);
}

// Helper function to parse command tokens
static int parse_command(char* input, char* tokens[], int max_tokens) {
    int count = 0;
    int i = 0;
    int in_token = 0;

    while (input[i] != '\0' && count < max_tokens) {
        if (input[i] == ' ' || input[i] == '\t') {
            if (in_token) {
                input[i] = '\0';
                in_token = 0;
            }
        } else {
            if (!in_token) {
                tokens[count++] = &input[i];
                in_token = 1;
            }
        }
        i++;
    }

    return count;
}

// Process user commands
static void process_command(Maester* maester, char* input) {
    char* tokens[10];
    int token_count = parse_command(input, tokens, 10);

    if (token_count == 0) {
        return; // Empty command
    }

    // LIST commands
    if (token_count >= 1 && my_strcasecmp(tokens[0], "LIST") == 0) {
        if (token_count == 1) {
            // Just "LIST" without arguments
            write_str(STDOUT_FILENO, "Did you mean to list something? Please review syntax.\n");
            write_str(STDOUT_FILENO, "Usage: LIST REALMS or LIST PRODUCTS [<REALM>]\n");
            return;
        }

        // LIST REALMS
        if (my_strcasecmp(tokens[1], "REALMS") == 0) {
            cmd_list_realms(maester);
            return;
        }

        // LIST PRODUCTS (with or without realm)
        if (my_strcasecmp(tokens[1], "PRODUCTS") == 0) {
            if (token_count == 2) {
                // LIST PRODUCTS (no realm)
                cmd_list_products(maester, NULL);
            } else {
                // LIST PRODUCTS <REALM>
                cmd_list_products(maester, tokens[2]);
            }
            return;
        }

        // LIST with unknown subcommand
        write_str(STDOUT_FILENO, "Unknown LIST command. Try: LIST REALMS or LIST PRODUCTS\n");
        return;
    }

    // PLEDGE <REALM> <sigil.jpg>
    if (token_count >= 1 && my_strcasecmp(tokens[0], "PLEDGE") == 0) {
        // Check if it's PLEDGE RESPOND
        if (token_count >= 2 && my_strcasecmp(tokens[1], "RESPOND") == 0) {
            // PLEDGE RESPOND <REALM> ACCEPT/REJECT
            if (token_count >= 4) {
                cmd_pledge_respond(maester, tokens[2], tokens[3]);
            } else {
                write_str(STDOUT_FILENO, "Did you mean to respond to a pledge? Please review syntax.\n");
                write_str(STDOUT_FILENO, "Usage: PLEDGE RESPOND <REALM> ACCEPT|REJECT\n");
            }
        } else if (token_count >= 2 && my_strcasecmp(tokens[1], "STATUS") == 0) {
            // PLEDGE STATUS
            cmd_pledge_status(maester);
        } else if (token_count >= 3) {
            // PLEDGE <REALM> <sigil>
            cmd_pledge(maester, tokens[1], tokens[2]);
        } else {
            write_str(STDOUT_FILENO, "Did you mean to send a pledge? Please review syntax.\n");
            write_str(STDOUT_FILENO, "Usage: PLEDGE <REALM> <sigil.jpg>\n");
        }
        return;
    }

    // START TRADE <REALM>
    if (token_count >= 1 && my_strcasecmp(tokens[0], "START") == 0) {
        if (token_count == 1) {
            // Just "START" without arguments
            write_str(STDOUT_FILENO, "Did you mean to start something? Please review syntax.\n");
            write_str(STDOUT_FILENO, "Usage: START TRADE <REALM>\n");
            return;
        }

        if (token_count >= 2 && my_strcasecmp(tokens[1], "TRADE") == 0) {
            if (token_count >= 3) {
                cmd_start_trade(maester, tokens[2]);
            } else {
                write_str(STDOUT_FILENO, "Did you mean to start a trade? Please review syntax.\n");
                write_str(STDOUT_FILENO, "Usage: START TRADE <REALM>\n");
            }
            return;
        }

        // START with unknown subcommand
        write_str(STDOUT_FILENO, "Unknown START command. Try: START TRADE <REALM>\n");
        return;
    }

    // ENVOY STATUS
    if (token_count >= 1 && my_strcasecmp(tokens[0], "ENVOY") == 0) {
        if (token_count >= 2 && my_strcasecmp(tokens[1], "STATUS") == 0) {
            cmd_envoy_status(maester);
            return;
        } else {
            write_str(STDOUT_FILENO, "Did you mean to check envoy status? Please review syntax.\n");
            write_str(STDOUT_FILENO, "Usage: ENVOY STATUS\n");
            return;
        }
    }

    // EXIT
    if (token_count == 1 && my_strcasecmp(tokens[0], "EXIT") == 0) {
        should_exit = 1;
        return;
    }

    // Unknown command
    write_str(STDOUT_FILENO, "Unknown command\n");
}

int main(int argc, char *argv[]){
    if (argc != 3){
        write_str(STDERR_FILENO, "Usage: ");
        write_str(STDERR_FILENO, argv[0]);
        write_str(STDERR_FILENO, " <config.dat> <stock.db>\n");
        return 1;
    }

    // Setup signal handler for CTRL+C
    signal(SIGINT, handle_sigint);

    // Load Maester configuration
    Maester* maester = load_maester_config(argv[1], argv[2]);
    if (maester == NULL){
        die("Error: Could not load maester configuration\n");
    }

    // Store in global for safe cleanup
    global_maester = maester;

    // Display Maester initialization message
    write_str(STDOUT_FILENO, "Maester of ");
    write_str(STDOUT_FILENO, maester->realm_name);
    write_str(STDOUT_FILENO, " initialized. The board is set.\n\n");

    // Main command loop
    char input[256];
    while (!should_exit) {
        write_str(STDOUT_FILENO, "$ ");

        // Read user input
        int bytes_read = read(STDIN_FILENO, input, sizeof(input) - 1);

        // Check if read was interrupted by signal
        if (bytes_read < 0) {
            if (errno == EINTR) {
                // Interrupted by signal (CTRL+C) - check flag and exit if needed
                if (should_exit) {
                    break;
                }
                continue;  // Otherwise continue reading
            }
            // Other error
            break;
        }

        if (bytes_read == 0 || should_exit) {
            break;
        }

        input[bytes_read] = '\0';

        // Remove newline
        for (int i = 0; i < bytes_read; i++) {
            if (input[i] == '\n' || input[i] == '\r') {
                input[i] = '\0';
                break;
            }
        }

        // Process the command
        if (my_strlen(input) > 0) {
            process_command(maester, input);
        }
    }

    // Cleanup when exiting (safe - NOT in signal handler)
    write_str(STDOUT_FILENO, "\nCleaning up resources...\n");
    free_maester(global_maester);
    global_maester = NULL;

    write_str(STDOUT_FILENO, "Maester process terminated. Farewell.\n");
    return 0;
}
