/* Creating a cache file with all the paths to svgs from the given theme directory
   Usage:
   const char *directoryPath = "/usr/share/icons/Lyra-blue-dark";
   const char *substring = ".svg";
   FILE *outputFile = fopen("/home/diogenes/icons.cache", "w");   
   create_icon_cache(outputFile, substring, directoryPath);
   fclose(outputFile);
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

void create_icon_cache(FILE *outputFile, const char *substring, const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("Error opening directory");
        return;
    }

    struct dirent *entry;
    struct stat statbuf;

    while ((entry = readdir(dir)) != NULL) {
        // Ignore current and parent directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        if (stat(fullpath, &statbuf) == -1) {
            // Ignore "No such file or directory" errors
            if (errno == ENOENT) {
                continue;
            }
            perror("Error getting file status");
            continue;
        }
        if (S_ISDIR(statbuf.st_mode)) {
            // Recursive call for directories
            create_icon_cache(outputFile, substring, fullpath);
        }
        else if (S_ISREG(statbuf.st_mode)) {
        	// Check if the substring is present in the filename
        	if (strstr(entry->d_name, substring) != NULL) {
				fprintf(outputFile, "%s/%s\n", path, entry->d_name);
			}
		}
	}
    closedir(dir);
}
