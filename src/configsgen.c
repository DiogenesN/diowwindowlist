/* creates initial config directory and file */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cairo/cairo-svg.h>
#include "getvaluefromconf.h"
#include "generate-icon-cache.h"

void create_configs() {
	const char *HOME = getenv("HOME");
	const char *iconTheme = NULL;
	if (HOME == NULL) {
		fprintf(stderr, "Unable to determine the user's home directory.\n");
		return;
	}
	const char *dirConfig	= "/.config/diowwindowlist";
	const char *fileConfig	= "/diowwindowlist.conf";
	const char *iconsCache	= "/icons.cache";
	const char *noicon	= "/noicon.svg";
	char dirConfigBuff[strlen(HOME) + strlen(dirConfig) + 1];
	char fileConfigBuff[strlen(HOME) + strlen(dirConfig) + strlen(fileConfig) + 1];
	char iconsCacheBuff[strlen(HOME) + strlen(dirConfig) + strlen(iconsCache) + 1];
	char noiconBuff[strlen(HOME) + strlen(dirConfig) + strlen(noicon) + 1];

	snprintf(dirConfigBuff, sizeof(dirConfigBuff), "%s%s", HOME, dirConfig);
	snprintf(fileConfigBuff, sizeof(fileConfigBuff), "%s%s%s", HOME, dirConfig, fileConfig);
	snprintf(iconsCacheBuff, sizeof(iconsCacheBuff), "%s%s%s", HOME, dirConfig, iconsCache);
	snprintf(noiconBuff, sizeof(noiconBuff), "%s%s%s", HOME, dirConfig, noicon);

	DIR *confDir = opendir(dirConfigBuff);
	struct stat buffer;
	/// check if icons theme has a valid path
	if (stat(dirConfigBuff, &buffer) == 0 && stat(iconsCacheBuff, &buffer) == 0) {
		/// getting the path to the icons directory
		iconTheme = get_char_value_from_conf(fileConfigBuff, "icons_theme");
		if (strcmp(iconTheme, "none") == 0) {
			printf("No icon theme specified, skipping icon cache file creation!\n");
			free((void *)iconTheme);
			return;
		}
		else {
			printf("Icons path provided, generating the icons cache file\n");
			FILE *outputFile = fopen(iconsCacheBuff, "w");
			create_icon_cache(outputFile, ".svg", iconTheme);
			free((void *)iconTheme);
			fclose(outputFile);
			printf("Icon cache file created successfully!\n");
		}
	}	
	// cheks if the file already exists
	if (confDir &&  stat(dirConfigBuff, &buffer) == 0 && stat(iconsCacheBuff, &buffer) == 0) {
		// directory exists nothing to do
		printf("Configs exist, nothing to do!\n");
		closedir(confDir);
		return;
	}
	else if (confDir &&  stat(dirConfigBuff, &buffer) == 0 && stat(iconsCacheBuff, &buffer) != 0) {
		/// cache file wae deleted, regenerating icon cache
		printf("Regenerating icons cache file\n");
		iconTheme = get_char_value_from_conf(fileConfigBuff, "icons_theme");
		if (strcmp(iconTheme, "none") == 0) {
			printf("No icon theme specified, skipping icon cache file creation!\n");
		}
		FILE *outputFile = fopen(iconsCacheBuff, "w");
		create_icon_cache(outputFile, ".svg", iconTheme);
		free((void *)iconTheme);
		fclose(outputFile);
		printf("Icon cache file created successfully!\n");
	}
	else {
		/// creating directory
		mkdir(dirConfigBuff, 0755);
		closedir(confDir);
		/// creating config file
		FILE *config = fopen(fileConfigBuff, "w+");
		fprintf(config, "%s\n", "icons_theme=none");
		fprintf(config, "%s\n", "posx=7");
		fprintf(config, "%s\n", "posy=7");
		fprintf(config, "%s\n", "cut_string_workaround=true");
		fprintf(config, "\n%s\n", "# NOTE: Any changes here require app restart!");
		fprintf(config, "%s\n", "# Provide the full path to your icon theme, example:");
		fprintf(config, "%s\n", "# icons_theme=/usr/share/icons/Lyra-blue-dark");
		fprintf(config, "%s\n", "# cut_string_workaround is either true or false");
		fprintf(config, "%s\n", "# it cuts the long multibyte strings, if it breaks the app then set it to false");
		fclose(config);
		/// generate icons cache
		const char *iconTheme = get_char_value_from_conf(fileConfigBuff, "icons_theme");
		printf("iconTheme: %s\n", iconTheme);
		if (strcmp(iconTheme, "none") == 0) {
			printf("No icon theme specified, skipping cache file creation!\n");
		}
		FILE *outputFile = fopen(iconsCacheBuff, "w");
		create_icon_cache(outputFile, ".svg", iconTheme);

		/// creating default noicon svg
		cairo_surface_t *surface = cairo_svg_surface_create(noiconBuff, 100, 100);
		cairo_t *cr = cairo_create(surface);
		// Clear the inside of the circle (transparent fill)
		cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0); // Transparent fill: RGBA(0, 0, 0, 0)
		cairo_fill(cr);
		// Draw the circle outline
		cairo_set_source_rgb(cr, 1.0, 0.0, 0.0); // Red outline: RGB(1, 0, 0)
		cairo_set_line_width(cr, 10.0); // Set line width to 10 (adjust as needed)
		cairo_arc(cr, 50, 50, 40, 0, 2 * M_PI); // Center (50, 50), radius 40
		cairo_stroke(cr); // Draw the outline using stroke instead of fill
		// Draw the diagonal lines for close icon effect
		cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); // White line: RGB(1, 1, 1)
		cairo_set_line_width(cr, 5.0); // Set line width to 5 (adjust as needed)
		cairo_move_to(cr, 30, 30); // Move to the starting point of the line
		cairo_line_to(cr, 70, 70); // Draw a line to the ending point
		cairo_move_to(cr, 30, 70); // Move to another starting point
		cairo_line_to(cr, 70, 30); // Draw another line to another ending point
		cairo_stroke(cr); // Draw the lines using stroke
		cairo_destroy(cr);
		cairo_surface_destroy(surface);

		/// freeing resources
		free((void *)iconTheme);
		fclose(outputFile);
		printf("All config files created successfully!\n");
	}
	return;
}
