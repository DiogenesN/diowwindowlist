// SPDX-License-Identifier: GPL-2.0-or-later

/* diowwindowlist
 * Left click on the button toggles the windowlist
 * Right click on the button terminates the application
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "configsgen.c"
#include "create-shm.h"
#include <wayland-client.h>
#include <wayland-cursor.h>
#include "getvaluefromconf.h"
#include <wayland-client-core.h>
#include "getstrfromsubstrinfile.h"
#include <librsvg-2.0/librsvg/rsvg.h>
#include "xdg-shell-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

char *config = NULL;
char *cacheFilePath = NULL;
int endPoint = 40;
int nrOfItems = 0;
const int fontSize = 25;
const int startPoint = 70;
const int frameThickness = 1;
static int wheelMoves = 0;
static int xdg_serial = 0;
static int parentAppId = 0;
static int preventInfitineLoopOnZeroApps = 0;
static bool appLock = FALSE;
static bool parentEntered = FALSE;
static bool popupEntered = FALSE;
static bool panelClicked = FALSE;
static bool firstTimeOpened = TRUE;

enum pointer_event_mask {
	POINTER_EVENT_ENTER = 1 << 0,
	POINTER_EVENT_LEAVE = 1 << 1,
	POINTER_EVENT_MOTION = 1 << 2,
	POINTER_EVENT_BUTTON = 1 << 3,
	POINTER_EVENT_AXIS_DISCRETE = 1 << 7,
};

struct pointer_event {
	uint32_t event_mask;
	wl_fixed_t surface_x;
	wl_fixed_t surface_y;
	uint32_t button;
	uint32_t state;
	uint32_t time;
	uint32_t serial;
	struct {
		bool valid;
		wl_fixed_t value;
		int32_t discrete;
	} axes[2];
};

enum touch_event_mask {
	TOUCH_EVENT_DOWN = 1 << 0,
	TOUCH_EVENT_UP = 1 << 1,
	TOUCH_EVENT_MOTION = 1 << 2,
	TOUCH_EVENT_CANCEL = 1 << 3,
	TOUCH_EVENT_SHAPE = 1 << 4,
	TOUCH_EVENT_ORIENTATION = 1 << 5,
};

struct touch_point {
	bool valid;
	int32_t id;
	uint32_t event_mask;
	wl_fixed_t surface_x;
	wl_fixed_t surface_y;
	wl_fixed_t major;
	wl_fixed_t minor;
	wl_fixed_t orientation;
};

struct touch_event {
	uint32_t event_mask;
	uint32_t time;
	uint32_t serial;
	struct touch_point points[10];
};

// main state struct
struct client_state {
	/* Globals */
	struct wl_shm *wl_shm;
	struct wl_display *wl_display;
	struct wl_registry *wl_registry;
    struct xdg_wm_base *xdg_wm_base;
	struct wl_compositor *wl_compositor;
	struct wl_seat *wl_seat;
	struct zwlr_foreign_toplevel_manager_v1 *zwlr_foreign_toplevel_manager_v1;
	/* Objects */
	struct wl_surface *wl_surface;
	struct wl_surface *wl_surface_popup;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
	struct zwlr_layer_surface_v1 *layer_surface;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct wl_pointer *wl_pointer;
	struct wl_cursor *wl_cursor;
	struct wl_buffer *wl_cursor_buffer;
	struct wl_cursor_theme *wl_cursor_theme;
	struct wl_surface *wl_cursor_surface;
	struct wl_cursor_image *wl_cursor_image;
	/* State */
	struct pointer_event pointer_event;
	struct touch_event touch_event;
	int32_t width;
	int32_t height;
	int32_t width_popup;
	int32_t height_popup;
	int32_t x_popup;
	int32_t y_popup;
	int32_t x_motion;
	int32_t y_motion;
	char title[2048];
	bool closed;
};

// linked list to store titles and handles for toplevels
struct Node {
	char title[2048];
	const char *iconPath;
    struct zwlr_foreign_toplevel_handle_v1 *toplevel;
	struct Node *link;
};

// Declaring linked list head
struct Node *head = NULL;

struct Node *create_new_node(char *title, char *app_id, const char *iconPath,
												struct zwlr_foreign_toplevel_handle_v1 *toplevel) {
	struct Node *newNode = malloc(sizeof(struct Node));
	///newNode->title = NULL;
    snprintf(newNode->title, 2048, "[ %s ] - %s", app_id, title);
    newNode->iconPath = NULL;
    newNode->iconPath = strdup(iconPath);
    newNode->toplevel = NULL;
    newNode->toplevel = toplevel;
    newNode->link = NULL;
    return newNode;
}

// Inserting an item to the beginning of the linked list
void insert_node_at_beginning(struct Node **head, char *title, char *app_id,
							const char *iconPath, struct zwlr_foreign_toplevel_handle_v1 *toplevel) {
	struct Node *newNode = create_new_node(title, app_id, iconPath, toplevel);
	newNode->link = *head;
	*head = newNode;
}

// Function to free the memory allocated for the linked list
void free_linked_list(struct Node **head) {
    struct Node *current = *head;
    struct Node *next;
    while (current != NULL) {
        next = current->link;
		free((void *)current->iconPath);
		current->iconPath = NULL;
		zwlr_foreign_toplevel_handle_v1_destroy(current->toplevel);
		current->toplevel = NULL;
		free(current);
		current = next;
    }
    *head = NULL; // Reset the head pointer
}

// dummy empty funtions just to make non null pointer in wl_pointer_listener
static void noop() {}
static const struct wl_registry_listener wl_registry_listener;
static const struct xdg_surface_listener xdg_surface_listener;
static const struct wl_callback_listener wl_surface_frame_listener;
static struct wl_buffer *draw_frame_popup(struct client_state *state);
static const struct zwlr_layer_surface_v1_listener layer_surface_listener;
static void zwlr_layer_surface_close(void *data, struct zwlr_layer_surface_v1 *surface);
static const struct zwlr_foreign_toplevel_manager_v1_listener foreign_toplevel_listener;

// get pointer events
static void wl_pointer_enter(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
						struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	(void)wl_pointer;
	struct client_state *client_state = data;

	/// disable evens for panel button and make it only work on the child (toplevel)
	int popupAppId = wl_proxy_get_id((struct wl_proxy *)surface);
	if (parentAppId == 0) {
		parentAppId = wl_proxy_get_id((struct wl_proxy *)surface);
	}

	if (popupAppId > parentAppId) {
		///printf("Entered Popup\n");
		popupEntered = TRUE;
		/* requestion server to notify the client when it's good time to draw a new frame
		 * by emitting 'done' event
		 */
		struct wl_callback *cb = wl_display_sync(client_state->wl_display);
		wl_callback_add_listener(cb, &wl_surface_frame_listener, client_state);
	}
	else {
		///printf("Entered parent\n");
		parentEntered = TRUE;
	}

	client_state->pointer_event.event_mask |= POINTER_EVENT_ENTER;
	client_state->pointer_event.serial = serial;
	client_state->pointer_event.surface_x = surface_x,
	client_state->pointer_event.surface_y = surface_y;
	/// Set our pointer               
	wl_pointer_set_cursor(wl_pointer,
						  client_state->pointer_event.serial,
						  client_state->wl_cursor_surface,
						  client_state->wl_cursor_image->hotspot_x,
						  client_state->wl_cursor_image->hotspot_y);
}

static void wl_pointer_leave(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
																struct wl_surface *surface) {
	(void)surface;
	(void)wl_pointer;
	popupEntered = FALSE;
	parentEntered = FALSE;
	struct client_state *client_state = data;
	client_state->pointer_event.serial = serial;
	client_state->pointer_event.event_mask |= POINTER_EVENT_LEAVE;
}

static void wl_pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time,
												wl_fixed_t surface_x, wl_fixed_t surface_y) {
	(void)wl_pointer;
	struct client_state *client_state = data;
	client_state->pointer_event.event_mask |= POINTER_EVENT_MOTION;
	client_state->pointer_event.time = time;
	client_state->pointer_event.surface_x = surface_x,
	client_state->pointer_event.surface_y = surface_y;
}

static void wl_pointer_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
               								uint32_t time, uint32_t button, uint32_t state) {
	(void)wl_pointer;
	struct client_state *client_state = data;
	client_state->pointer_event.event_mask |= POINTER_EVENT_BUTTON;
	client_state->pointer_event.time = time;
	client_state->pointer_event.serial = serial;
	client_state->pointer_event.button = button,
	client_state->pointer_event.state = state;
}

static void wl_pointer_axis_discrete(void *data, struct wl_pointer *wl_pointer,
													uint32_t axis, int32_t discrete) {
	(void)wl_pointer;
	struct client_state *client_state = data;
	client_state->pointer_event.event_mask |= POINTER_EVENT_AXIS_DISCRETE;
	client_state->pointer_event.axes[axis].valid = true;
	client_state->pointer_event.axes[axis].discrete = discrete;
}

static void initial_popup_open(struct client_state *state) {
	xdg_surface_set_window_geometry(state->xdg_surface, state->x_popup, 30,
														state->width_popup, state->height_popup);
	wl_surface_commit(state->wl_surface_popup);
}

static void show_popup(struct client_state *state) {
	/// presenting the newly generated list
	state->xdg_surface = xdg_wm_base_get_xdg_surface(state->xdg_wm_base, state->wl_surface_popup);
	state->xdg_toplevel = xdg_surface_get_toplevel(state->xdg_surface);
	wl_surface_attach(state->wl_surface_popup, NULL, 0, 0);
	wl_surface_commit(state->wl_surface_popup);
	xdg_surface_add_listener(state->xdg_surface, &xdg_surface_listener, state);
	xdg_toplevel_set_app_id(state->xdg_toplevel, "org.Diogenes.diowwindowlist");
	xdg_surface_set_window_geometry(state->xdg_surface, state->x_popup, 30,
																	state->width_popup,
																	state->height_popup);
	wl_surface_commit(state->wl_surface_popup);
}

static void close_popup(struct client_state *state) {
	wl_surface_attach(state->wl_surface_popup, NULL, 0, 0);
	xdg_toplevel_destroy(state->xdg_toplevel);
	xdg_surface_destroy(state->xdg_surface);
	xdg_wm_base_destroy(state->xdg_wm_base);
	wl_surface_commit(state->wl_surface_popup);
}

/* Regenerate the list of titles */
static void refresh_titles(struct client_state *state) {
	zwlr_foreign_toplevel_manager_v1_stop(state->zwlr_foreign_toplevel_manager_v1);
	wl_registry_destroy(state->wl_registry);
	state->wl_registry = wl_display_get_registry(state->wl_display);
	wl_registry_add_listener(state->wl_registry, &wl_registry_listener, state);
	wl_display_roundtrip(state->wl_display);
}

/* panel enter, popup open/leave, wheel sctoll and click actions */
static void wl_pointer_frame(void *data, struct wl_pointer *wl_pointer) {
	(void)wl_pointer;
	nrOfItems = 0;
	struct Node *headPtr = head;
	struct client_state *state = data;
	struct pointer_event *event = &state->pointer_event;
	uint32_t axis_events = POINTER_EVENT_AXIS_DISCRETE;
	
	/// get number of items
	while (headPtr->link != NULL) {
		nrOfItems = nrOfItems + 1;
		headPtr = headPtr->link;
	}

	/// browsing opened windows with mouse wheel
	if (event->event_mask & axis_events) {
		for (size_t i = 0; i < 2; ++i) {
			if (!event->axes[i].valid) {
				continue;
			}
			if (event->event_mask & POINTER_EVENT_AXIS_DISCRETE) {
				///fprintf(stderr, "discrete %d\n", event->axes[i].discrete);
				/// wheel down event
				if (event->axes[i].discrete > 0) {
					///fprintf(stderr, "mouse wheel down\n");
					headPtr = NULL;
					headPtr = head;
					/// prevent craches
					if ((wheelMoves < 0) || (wheelMoves > nrOfItems)) {
						wheelMoves = 0;
					}
					/// going to the next toplevel until the last one reached
					if (wheelMoves < nrOfItems) {
						wheelMoves = wheelMoves + 1;
					}
					/// if last node reached
					if (wheelMoves == nrOfItems) {
						wheelMoves = 0;
					}
					for (int i = 0; i < wheelMoves; i++) {
						headPtr = headPtr->link;
					}
					/// activating the window
					zwlr_foreign_toplevel_handle_v1_activate(headPtr->toplevel, state->wl_seat);
				}
				else {
					///fprintf(stderr, "mouse wheel up\n");
					headPtr = NULL;
					headPtr = head;
					/// if it's the first time, get the last node
					/// prevent craches
					if ((wheelMoves < 0) || (wheelMoves > nrOfItems)) {
						wheelMoves = 1;
					}
					if (wheelMoves == 0) {
						wheelMoves = nrOfItems - 1;
					}
					if (wheelMoves > 0 && wheelMoves < nrOfItems) {
						wheelMoves = wheelMoves - 1;
					}
					/// iterate ne number of times to get to the previous node
					for (int i = 0; i < wheelMoves; i++) {
						headPtr = headPtr->link;
					}
					/// activating the window
					zwlr_foreign_toplevel_handle_v1_activate(headPtr->toplevel, state->wl_seat);
				}
			}
		}
	}

	if (event->event_mask & POINTER_EVENT_ENTER) {
		///fprintf(stderr, "entered resizing\n");
		/// show panel
		zwlr_layer_surface_v1_set_size(state->layer_surface, 20, 20);
		wl_surface_commit(state->wl_surface);
	}

	if (!parentEntered && event->event_mask & POINTER_EVENT_LEAVE) {
		///fprintf(stderr, "left resizing\n");
		zwlr_layer_surface_v1_set_size(state->layer_surface, 1, 1);
		wl_surface_commit(state->wl_surface);
		parentEntered = FALSE;
	}

	/// clicking on the panel button
	if (event->event_mask & POINTER_EVENT_BUTTON) {
		char *stateev = event->state == WL_POINTER_BUTTON_STATE_RELEASED ? "released" : "pressed";
		///fprintf(stderr, "button %d %s\n", event->button, stateev);
		/// terminate the application
		if (event->button == 273) {
			state->closed = TRUE;
		}
		/// toggle open/close popup when clicking on the panel
		if (strcmp(stateev, "pressed") == 0 && panelClicked && !popupEntered) {
			/// check if no windows are currently opened
			if (preventInfitineLoopOnZeroApps >= 5) {
				appLock = FALSE;
				panelClicked = FALSE;
				fprintf(stderr, "No windows currently opened\n");
			}
			else {
				///printf("App lock off\n");
				appLock = FALSE;
				panelClicked = FALSE;
				close_popup(state);
				nrOfItems = 0;
			}
			///printf("----------------------- Popup Closed -----------------------\n");
		}
		else if (xdg_serial == 0 && strcmp(stateev, "pressed") == 0 && !panelClicked && \
																				!popupEntered) {
			///printf("----------------- First time panel clicked -----------------\n");
			firstTimeOpened = FALSE;
			panelClicked = TRUE;
			state->height_popup = 0;
			preventInfitineLoopOnZeroApps = 0;
			do {
				if (preventInfitineLoopOnZeroApps == 7) {
					break;
				}
				/// generate the new list
				free_linked_list(&head);
				///	///refresh_titles(state);
				refresh_titles(state);
				preventInfitineLoopOnZeroApps = preventInfitineLoopOnZeroApps + 1;
			} while (state->height_popup == 0);

			/// check if no windows are currently opened
			if (preventInfitineLoopOnZeroApps >= 5) {
				fprintf(stderr, "No windows currently opened\n");
			}
			else {
				/// opens popup the first time
				initial_popup_open(state);
			}
		}
		else if (xdg_serial != 0 && strcmp(stateev, "pressed") == 0 && !panelClicked && \
														!appLock && !popupEntered) {
			///printf("----------------- Second time panel clicked -----------------\n");
			panelClicked = TRUE;
			state->height_popup = 0;
			preventInfitineLoopOnZeroApps = 0;
			do {
				if (preventInfitineLoopOnZeroApps == 7) {
					break;
				}
				/// generate the new list
				free_linked_list(&head);
				///	///refresh_titles(state);
				refresh_titles(state);
				preventInfitineLoopOnZeroApps = preventInfitineLoopOnZeroApps + 1;
			} while (state->height_popup == 0);

			/// check if no windows are currently opened
			if (preventInfitineLoopOnZeroApps >= 5) {
				fprintf(stderr, "No windows currently opened\n");
			}
			else {
				/// position popup automatically
				if (state->height_popup >= 20 && state->height_popup < 160) {
					state->y_popup = get_int_value_from_conf(config, "posy");
					state->y_popup = ((state->y_popup - state->height_popup) - \
							(3 - ((state->height_popup + 40) / 3) + (state->height_popup * 1.1)));
				}
				else if (state->height_popup >= 160 && state->height_popup < 200) {
					state->y_popup = get_int_value_from_conf(config, "posy");
					state->y_popup = ((state->y_popup - state->height_popup) - \
						(10 - ((state->height_popup + 40) / 3) + (state->height_popup * 1.1)));
				}
				else if (state->height_popup >= 200 && state->height_popup < 400) {
					state->y_popup = get_int_value_from_conf(config, "posy");
					state->y_popup = ((state->y_popup - state->height_popup) - \
						(10 - ((state->height_popup + 40) / 3) + (state->height_popup * 1.1)));
				}
				else {
					state->y_popup = get_int_value_from_conf(config, "posy");
					state->y_popup = ((state->y_popup - state->height_popup) - \
						(-100 - ((state->height_popup + 40) / 3) + (state->height_popup * 1.2)));
				}
				/// open popup the second and all the next times
				show_popup(state);
			}
		}
		/// clicking inside popup
		if (strcmp(stateev, "pressed") == 0 && popupEntered) {
			///printf("-------------------- Cliked inside popup --------------------\n");
			/// don't go beyond the strict buffer height ignoring the padding
			if (state->y_motion < state->height_popup) {
				/* this calculates what item in the list of titles the mouse cursor is
				 * currently hovering over
				 */
				///int currentSelectedLine = state->y_motion / 40;
				///printf("current Selected Line: %d\n", currentSelectedLine);
				panelClicked = FALSE;
				/* this iterates the linked list and gets the title and the toplevel handle of
				 * the currently hovered item identified by its positional number
				 */
				struct Node *temp = head;
				for (int i = 0; i < (state->y_motion / 40); i++) {
					temp = temp->link;
				}
				///printf("selected title: %s\n", temp->title);
				/// this raises up (brings intp focus) the selected window
				zwlr_foreign_toplevel_handle_v1_activate(temp->toplevel, state->wl_seat);
				/// after the selected window is raised close the popup window
				close_popup(state);
				appLock = FALSE;
				temp = NULL;
			}
		}
	}

	if (event->event_mask & POINTER_EVENT_MOTION) {
		///state->x_motion = wl_fixed_to_int(event->surface_x);
		state->y_motion = wl_fixed_to_int(event->surface_y);
		///fprintf(stderr, "x=%d\n", state->x_motion);
	}

	///fprintf(stderr, "y\n");
	memset(event, 0, sizeof(*event));
}

static const struct wl_pointer_listener wl_pointer_listener = {
	.enter = wl_pointer_enter,
	.leave = wl_pointer_leave,
	.motion = wl_pointer_motion,
	.button = wl_pointer_button,
	.axis = noop,
	.frame = wl_pointer_frame,
	.axis_source = noop,
	.axis_stop = noop,
	.axis_discrete = wl_pointer_axis_discrete,
};

/*************************************************************************************************/
/************************************** DRAWING  POPUP *******************************************/
/*************************************************************************************************/
static void wl_buffer_release_popup(void *data, struct wl_buffer *wl_buffer) {
	/* Sent by the compositor when it's no longer using this buffer */
	(void)data;
	wl_buffer_destroy(wl_buffer);
}

static const struct wl_buffer_listener wl_buffer_listener_popup = {
	.release = wl_buffer_release_popup,
};
			
static struct wl_buffer *draw_frame_popup(struct client_state *state) {
	int32_t width = state->width_popup;
	int32_t height = (state->height_popup + 21);
	int32_t stride = width * 4;
	int32_t size = stride * height;
	int32_t fd_popup = fd_popup = allocate_shm_file(size);
	if (fd_popup == -1) {
		return NULL;
	}
	uint32_t *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_popup, 0);
	if (data == MAP_FAILED) {
		close(fd_popup);
		return NULL;
	}

	/// drawing titles
	struct wl_shm_pool *pool_popup = wl_shm_create_pool(state->wl_shm, fd_popup, size);
	struct wl_buffer *buffer_popup = wl_shm_pool_create_buffer(pool_popup, 0, width, height, stride,
																	WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool_popup);
	close(fd_popup);

	// cairo drawing
	cairo_surface_t *cairoSurface = cairo_image_surface_create_for_data((unsigned char *)data,
																		CAIRO_FORMAT_RGB24,
																		width,
																		height,
																		stride);
	cairo_t *cr = cairo_create(cairoSurface);
	cairo_paint(cr);

	///////////////////////////////// deaw to buffer /////////////////////////////////////////
	/// set cairo font size
	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	/// drawing frame aroung the windowint frameThickness = 10; // Thickness of the frame in pixels
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			if (x < frameThickness || x >= width - frameThickness || y < frameThickness || \
																y >= height - frameThickness) {
				// Inside the frame region
				data[y * width + x] = 0x0ad0f7; // Fill with the given color
			}
			else {
				/// filling up the whole buffer with given color
				data[y * width + x] = 0x3b3d40;
			}
		}
	}

	/// filling buffer with app_ids and titles
	struct Node *listIterFrame = head;
	while (listIterFrame != NULL) {
    	if (!popupEntered) {
    		cairo_set_font_size(cr, fontSize);
			cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
			cairo_move_to(cr, startPoint, endPoint);
			cairo_show_text(cr, listIterFrame->title);
		}
		if (popupEntered) {
			/// all spaces for items are of 40 pixels height
			/// space before selected item
			if (endPoint > 0 && endPoint <= ((state->y_motion / 40) * 40)) {
				cairo_set_font_size(cr, fontSize);
				cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
				cairo_move_to(cr, startPoint, endPoint);
				cairo_show_text(cr, listIterFrame->title);
			}
			/// currently selected item
			else if (endPoint >= ((state->y_motion / 40) * 40 - 40) && \
											endPoint <= ((state->y_motion / 40) * 40) + 40) {
				cairo_set_font_size(cr, 27);
				cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, 1.0);
				cairo_move_to(cr, startPoint, endPoint);
				cairo_show_text(cr, listIterFrame->title);

			}
			/// space after selected item
			else {
				cairo_set_font_size(cr, fontSize);
				cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
				cairo_move_to(cr, startPoint, endPoint);
				cairo_show_text(cr, listIterFrame->title);
			}
		}
		
		/// drawing icons
		if (height > 1) {
			const RsvgRectangle rectIcons = {
				.x = 15,
				.y = (endPoint - 28),
				.width = 40,
				.height = 40,
			};

			///cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
			RsvgHandle *svgIcons = rsvg_handle_new_from_file(listIterFrame->iconPath, NULL);
			rsvg_handle_render_document(svgIcons, cr, &rectIcons, NULL);
			/// destroy cairo
			g_object_unref(svgIcons);
		}

		/// this moves to the next string in the linked list
		listIterFrame = listIterFrame->link;
		/// this arranges the next string below the previous one
		endPoint = endPoint + 40;
		if (endPoint > state->height_popup) {
			endPoint = 40;
		}
	}

	// destroy cairo
	cairo_surface_destroy (cairoSurface);
	cairo_destroy (cr);
	munmap(data, size);
	listIterFrame = NULL;

	/// destroy the buffer
	wl_buffer_add_listener(buffer_popup, &wl_buffer_listener_popup, NULL);
	return buffer_popup;
}

static void wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time) {
	(void)time;
	struct client_state *state = data;
	/* Destroy this callback */
	wl_callback_destroy(cb);
	cb = wl_display_sync(state->wl_display);
	if (popupEntered) {
		/// unsleep(1000000) = 1 sexond, reduce framerate, throtlle CPU usage
		usleep(150000);
		wl_callback_add_listener(cb, &wl_surface_frame_listener, state);
	}
	else {
		wl_callback_destroy(cb);
	}
	/* Submit a frame for this event */
	struct wl_buffer *buffer = draw_frame_popup(state);
	if (panelClicked) {
		/// fix: surface must not have a buffer attached ...
		wl_surface_attach(state->wl_surface_popup, buffer, 0, 0);
		wl_surface_damage_buffer(state->wl_surface_popup, 0, 0, INT32_MAX, INT32_MAX);
		wl_surface_commit(state->wl_surface_popup);
	}
}

static const struct wl_callback_listener wl_surface_frame_listener = {
	.done = wl_surface_frame_done,
};

/*************************************************************************************************/
/************************************** DRAWING WIDGET *******************************************/
/*************************************************************************************************/
static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
	/// Sent by the compositor when it's no longer using this buffer
	(void)data;
	wl_buffer_destroy(wl_buffer);
}

static const struct wl_buffer_listener wl_buffer_listener = {
	.release = wl_buffer_release,
};

static struct wl_buffer *draw_frame(struct client_state *state) {
	/// this function runs in a loop
	int width = state->width;
	int height = state->height;
	int stride = width * 4;
	int size = stride * height;
	int fd = allocate_shm_file(size);

	uint32_t *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	struct wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, size);
	struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride,
																	WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);
	
	/// drawing frame aroung the window
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			if (x < 1 || x >= width - 1 || y < 1 || y >= height - 1) {
				// deawing only the 1 pixel wide frame around the buffer
				data[y * width + x] = 0x0ad0f7; // Fill with the given color
			}
			else {
				/// filling up the whole buffer with given color
				data[y * width + x] = 0x3b3d40;
			}
		}
	}

	munmap(data, size);
	/// very important to release the buffer to prevent memory leak
	wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
	return buffer;
}

static void wl_seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities) {
	(void)wl_seat;
	struct client_state *state = data;
	bool have_pointer = capabilities & WL_SEAT_CAPABILITY_POINTER;
	if (have_pointer && state->wl_pointer == NULL) {
		state->wl_pointer = wl_seat_get_pointer(state->wl_seat);
		wl_pointer_add_listener(state->wl_pointer, &wl_pointer_listener, state);
	}
	else if (!have_pointer && state->wl_pointer != NULL) {
		wl_pointer_release(state->wl_pointer);
		state->wl_pointer = NULL;
	}
}

static const struct wl_seat_listener wl_seat_listener = {
	.capabilities = wl_seat_capabilities,
	.name = noop,
};

// configuring popup, attaching buffer
static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
	xdg_serial = serial;
    struct client_state *state = data;
    xdg_surface_ack_configure(xdg_surface, serial);
    struct wl_buffer *buffer = draw_frame_popup(state);
    wl_surface_attach(state->wl_surface_popup, buffer, 0, 0);
    wl_surface_commit(state->wl_surface_popup);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
	(void)data;
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

/********************************* TOPLEVEL MANAGEMENT ****************************************/
// getting titles foreign manager listener
static void zwlr_foreign_toplevel_handle_v1_handle_title(void *data,
												struct zwlr_foreign_toplevel_handle_v1 *toplevel,
																				const char *title) {
	(void)toplevel;
	/// REMEMBER THAT NO MATTER WHAT CHANGES YOU DO HERE, THEY ALL GET RESET WHEN POPUP OPEN!!!!!!
	if (strcmp(title, "nil") != 0) {
		struct client_state *state = data;
		strncpy(state->title, title, 2048);
		/* string cut workaround */
		bool isMultibyte = FALSE;
		const int bytesToCut = 25; /// number of chars you want to cut the string at
		char *ifCutWorkaround = get_char_value_from_conf(config, "cut_string_workaround");
		/// check if the string is even long and needs to be cut and it is true in the config
		if (strcmp(ifCutWorkaround, "true") == 0 && \
										strlen(state->title) >= (long unsigned int)bytesToCut) {
			/// check if the string is not a multibyte
			for (size_t i = 0; i < strlen(state->title); i++) {
				if (state->title[i] < 0) {
					isMultibyte = TRUE;
					break;
				}
			}
			if (!isMultibyte) {
				/// the stirng is not multibyte
				/// cut the string
				state->title[bytesToCut - 3] = '.';
				state->title[bytesToCut - 2] = '.';
				state->title[bytesToCut - 1] = '.';
				state->title[bytesToCut] = '\0';
			}
			/// if the string is a multibyte string
			else {
				/// applying the workaround for the multibyte string			
				static int counter = 0;
				char newStr[bytesToCut * 4];
				char *ptr = newStr;
				/// populate the string from another multibyte string
				for (counter = 0; counter <= bytesToCut; counter++) {
					snprintf(newStr + counter, sizeof(newStr), "%.2s", state->title + counter);
				}
				/// null terminate any last invalid char
				for (int i = 0; i <= bytesToCut; i++) {
					///printf("ptr[%d]: %.2s\n", i, ptr + counter - i);
					if (ptr[counter - i] == -48) {
						ptr[counter - i] = '\0';
						break;
					}
				}

				/// adding 3 dots
				strcat(newStr, "...");
				snprintf(state->title, sizeof(newStr), "%s", newStr);
			}
		}
		free((void *)ifCutWorkaround);
		ifCutWorkaround = NULL;
	}
}

static void zwlr_foreign_toplevel_handle_v1_handle_app_id(void *data,
												struct zwlr_foreign_toplevel_handle_v1 *toplevel,
																			const char *app_id) {
	const char *iconPath = find_substring_in_file(cacheFilePath, app_id);
	struct client_state *state = data;
	if (strcmp(app_id, "org.Diogenes.diowwindowlist") == 0 && \
														strcmp(state->title, "nil") != 0) {
		///printf("App lock on\n");
		appLock = TRUE;
	}
	if (firstTimeOpened || (!appLock && strcmp(app_id, "org.Diogenes.diowwindowlist") != 0 && \
															strcmp(state->title, "nil") != 0)) {
		insert_node_at_beginning(&head, state->title, (char *)app_id, iconPath, toplevel);
		state->height_popup = state->height_popup + 1 * 20;
		free((void *)iconPath);
		iconPath = NULL;
	}
}

static void zwlr_foreign_toplevel_handle_v1_handle_closed(void *data,
											struct zwlr_foreign_toplevel_handle_v1 *toplevel) {
	(void)data;
	(void)toplevel;
	if (nrOfItems > 0) {
		nrOfItems = nrOfItems - 1;
	}
	appLock = FALSE;
}

static struct zwlr_foreign_toplevel_handle_v1_listener zwlr_foreign_toplevel_handle_v1_listener = {
	.title = zwlr_foreign_toplevel_handle_v1_handle_title,
	.app_id = zwlr_foreign_toplevel_handle_v1_handle_app_id,
	.output_enter = noop,
	.output_leave = noop,
	.state = noop,
	.done = noop,
	.closed = zwlr_foreign_toplevel_handle_v1_handle_closed,
	.parent = noop,
};

static void zwlr_foreign_toplevel_manager_v1_handle_toplevel(void *data,
			 struct zwlr_foreign_toplevel_manager_v1 *zwlr_foreign_toplevel_manager_v1,
			 struct zwlr_foreign_toplevel_handle_v1 *toplevel) {
	(void)zwlr_foreign_toplevel_manager_v1;
	struct client_state *state = data;
	zwlr_foreign_toplevel_handle_v1_add_listener(toplevel, &zwlr_foreign_toplevel_handle_v1_listener,
																					state);
}

static void zwlr_foreign_toplevel_manager_v1_handle_finished(void *data,
												struct zwlr_foreign_toplevel_manager_v1 *manager) {
	(void)data;
	(void)manager;
	///printf("Finishes toplevel\n");
	zwlr_foreign_toplevel_manager_v1_destroy(manager);
}

static const struct zwlr_foreign_toplevel_manager_v1_listener foreign_toplevel_listener = {
	.toplevel = zwlr_foreign_toplevel_manager_v1_handle_toplevel,
	.finished = zwlr_foreign_toplevel_manager_v1_handle_finished,
};

/// binding objects to globals
static void registry_global(void *data, struct wl_registry *wl_registry, uint32_t name,
													const char *interface, uint32_t version) {
	(void)version;
	struct client_state *state = data;
	if (strcmp(interface, wl_shm_interface.name) == 0) {
		state->wl_shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, 1);
	}
	else if (strcmp(interface, wl_compositor_interface.name) == 0) {
		state->wl_compositor = wl_registry_bind(wl_registry, name, &wl_compositor_interface, 4);
	}
	else if (strcmp(interface, wl_seat_interface.name) == 0) {
		state->wl_seat = wl_registry_bind(wl_registry, name, &wl_seat_interface, 7);
		wl_seat_add_listener(state->wl_seat, &wl_seat_listener, state);
	}
	else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		/// Bind Layer Shell interface
		state->layer_shell = wl_registry_bind(wl_registry, name, &zwlr_layer_shell_v1_interface, 1);
	}
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->xdg_wm_base = wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(state->xdg_wm_base, &xdg_wm_base_listener, state);
    }
    else if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
        state->zwlr_foreign_toplevel_manager_v1 = wl_registry_bind(wl_registry, name,
        											&zwlr_foreign_toplevel_manager_v1_interface, 3);
        zwlr_foreign_toplevel_manager_v1_add_listener(state->zwlr_foreign_toplevel_manager_v1,
																&foreign_toplevel_listener, state);
    }
}

static void registry_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name) {
	(void)data;
	(void)name;
	(void)wl_registry;
	/* This space deliberately left blank */
}

static const struct wl_registry_listener wl_registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

/* configure zwlr_layer_surface_v1 */
static void layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *surface,
											uint32_t serial, uint32_t width, uint32_t height) {
	(void)width;
	(void)height;
	struct client_state *state = data;
	zwlr_layer_surface_v1_ack_configure(surface, serial);
	struct wl_buffer *buffer = draw_frame(state);
	wl_surface_attach(state->wl_surface, buffer, 0, 0);
	wl_surface_commit(state->wl_surface);
}

static void zwlr_layer_surface_close(void *data, struct zwlr_layer_surface_v1 *surface) {
	(void)surface;
	struct client_state *state = data;
	state->closed = true;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = zwlr_layer_surface_close,
};

int main(void) {
	/// generate initial config
	create_configs();

	/// setting up the global config file path
	const char *HOME = getenv("HOME");
	const char *configPath = "/.config/diowwindowlist/diowwindowlist.conf";
	const char *iconCachePath = "/.config/diowwindowlist/icons.cache";
	config = malloc(sizeof(char) * strlen(HOME) + strlen(configPath) + 3);
	cacheFilePath = malloc(sizeof(char) * strlen(HOME) + strlen(iconCachePath) + 3);
	snprintf(config, strlen(HOME) + strlen(configPath) + 3, "%s%s", HOME, configPath);
	snprintf(cacheFilePath, strlen(HOME) + strlen(iconCachePath) + 3, "%s%s", HOME, iconCachePath);

	struct client_state state = { 0 };
	state.x_popup = get_int_value_from_conf(config, "posx");
	state.y_popup = get_int_value_from_conf(config, "posy");
	state.width = 20;
	state.height = 20;
	state.width_popup = 630;

	state.wl_display = wl_display_connect(NULL);
	state.wl_registry = wl_display_get_registry(state.wl_display);
	wl_registry_add_listener(state.wl_registry, &wl_registry_listener, &state);
	wl_display_roundtrip(state.wl_display);
	/// set cursor visible on surface
	state.wl_cursor_theme = wl_cursor_theme_load(NULL, 24, state.wl_shm);
	state.wl_cursor = wl_cursor_theme_get_cursor(state.wl_cursor_theme, "left_ptr");
	state.wl_cursor_image = state.wl_cursor->images[0];
	state.wl_cursor_buffer = wl_cursor_image_get_buffer(state.wl_cursor_image);
	state.wl_cursor_surface = wl_compositor_create_surface(state.wl_compositor);
	wl_surface_attach(state.wl_cursor_surface, state.wl_cursor_buffer, 0, 0);
	wl_surface_commit(state.wl_cursor_surface);
	/// end of cursor setup
	state.wl_surface = wl_compositor_create_surface(state.wl_compositor);
	state.wl_surface_popup = wl_compositor_create_surface(state.wl_compositor);
    state.xdg_surface = xdg_wm_base_get_xdg_surface(state.xdg_wm_base, state.wl_surface_popup);
    xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);
    state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
    xdg_toplevel_set_app_id(state.xdg_toplevel, "org.Diogenes.diowwindowlist");
	state.layer_surface = zwlr_layer_shell_v1_get_layer_surface(state.layer_shell,
																state.wl_surface,
																NULL,
																ZWLR_LAYER_SHELL_V1_LAYER_TOP,
																"panel");
	zwlr_layer_surface_v1_add_listener(state.layer_surface, &layer_surface_listener, &state);
	zwlr_layer_surface_v1_set_size(state.layer_surface, state.width, state.height);
	zwlr_layer_surface_v1_set_anchor(state.layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | \
																ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
	/// committing the surface showing the panel
	wl_surface_commit(state.wl_surface);
	///struct wl_callback *cb = wl_surface_frame(state.wl_surface_popup);
	///wl_callback_add_listener(cb, &wl_surface_frame_listener, &state);

	while (!state.closed && wl_display_dispatch(state.wl_display) != -1) {
		/* This space deliberately left blank */
	}

	/// free resources
	free(config);
	config = NULL;
    free_linked_list(&head);
	free((void *)cacheFilePath);
	cacheFilePath = NULL;
	zwlr_foreign_toplevel_manager_v1_stop(state.zwlr_foreign_toplevel_manager_v1);
	zwlr_foreign_toplevel_manager_v1_destroy(state.zwlr_foreign_toplevel_manager_v1);
	if (firstTimeOpened) {
		xdg_toplevel_destroy(state.xdg_toplevel);
		xdg_surface_destroy(state.xdg_surface);
	}
	wl_buffer_destroy(state.wl_cursor_buffer);
	xdg_wm_base_destroy(state.xdg_wm_base);
	wl_surface_destroy(state.wl_cursor_surface);
	zwlr_layer_surface_v1_destroy(state.layer_surface);
	zwlr_layer_shell_v1_destroy(state.layer_shell);
	wl_surface_destroy(state.wl_surface_popup);
	wl_surface_destroy(state.wl_surface);
	wl_registry_destroy(state.wl_registry);
	wl_display_disconnect(state.wl_display);

	printf("Wayland client terminated!\n");

    return 0;
}
