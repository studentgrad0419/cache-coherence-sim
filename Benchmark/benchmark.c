//ToDo: Run simulator in MESI, MOESI, MESIF, and other coherency modes
//ToDo: Use generated "memory access files" to test with

#include <stdio.h>
#include <stdlib.h>
#include <cstring>

int main() {
    // Open the file containing commands
    FILE *file = fopen("./sim_config.txt", "r");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    // Read and process each command from the file
    char command[256];
    while (fgets(command, sizeof(command), file) != NULL) {
        // Remove newline character if present
        char *newline = strchr(command, '\n');
        if (newline != NULL) {
            *newline = '\0';
        }

        // Use the system function to execute the command
        int result = system(command);

        // Check the result of the execution
        if (result == 0) {
            printf("Command \"%s\" executed successfully.\n", command);
        } else {
            printf("Command \"%s\" execution failed.\n", command);
        }
    }

    // Close the file
    fclose(file);

    return 0;
}