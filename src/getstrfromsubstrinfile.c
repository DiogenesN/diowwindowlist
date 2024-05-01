// SPDX-License-Identifier: GPL-2.0-or-later

/* Return the string that contains the given subtring in a text file
 * if the substring is not found then a predefined noicon.svg is returned
 * uasge:
 *  const char *fullPathToConf = "/home/diogenes/.config/diowwindowlist/icons.cache";
 *  const char *substring = "org.inkscape.Inkscape";
 *  char *result = find_substring_in_file(fullPathToConf, substring);
 *  printf("Substring found: %s\n", result);
 *  free(result); // Remember to free the allocated memory
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *find_substring_in_file(const char *fullPathToConf, const char *substring) {
	const char *HOME = getenv("HOME");	
	char noIconBuff[strlen(HOME) + 35];
	snprintf(noIconBuff, sizeof(noIconBuff), "%s/.config/diowwindowlist/noicon.svg", HOME);
	
	char buffer[1024];
	FILE *pathToConfig = fopen(fullPathToConf, "r");
	while (fgets(buffer, sizeof(buffer), pathToConfig) != NULL) {
        if (buffer[strlen(buffer) - 1] == '\n') { // Check if the last character is newline
            buffer[strlen(buffer) - 1] = '\0'; // Replace newline with null terminator
        }
		if (strstr(buffer, substring) != NULL && strstr(buffer, "#") == NULL) {
			fclose(pathToConfig);
			return strdup(buffer);
		}
	}

	///printf("Substring not found!\n");
	fclose(pathToConfig);
    return strdup(noIconBuff);
}
