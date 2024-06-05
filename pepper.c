#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb.h>

void init_xcb(void);
int get_events(void);
void map_window(xcb_map_request_event_t *e);
void configure_request(xcb_configure_request_event_t *e);
void resize_window(xcb_window_t w, int x, int y);
void setfocus(xcb_window_t w);
void button_press(xcb_button_press_event_t *e);
void key_press(xcb_key_press_event_t *e);

xcb_connection_t *connection;
xcb_screen_t *screen;
xcb_window_t root_window;
xcb_drawable_t dw;

void init_xcb(void) {
	xcb_void_cookie_t cookie;
	int vals[] = {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT};

	connection = xcb_connect(NULL, NULL);
	if (xcb_connection_has_error(connection)) {
		fprintf(stderr, "Can't connect to X11, is it running?\n");
		exit(1);
	}
	screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
	root_window = screen->root;

	xcb_grab_server(connection);

	cookie = xcb_change_window_attributes_checked(connection, root_window, XCB_CW_EVENT_MASK, vals);

	if (xcb_request_check(connection, cookie)) {
		fprintf(stderr, "Another WM is already running\n");
		xcb_disconnect(connection);
		exit(1);
	}

	xcb_ungrab_server(connection);
	xcb_flush(connection);
}

int get_events(void) {
	int event_status = xcb_connection_has_error(connection);

	if (!event_status) {
		xcb_generic_event_t *event = xcb_wait_for_event(connection);

		switch (event->response_type & ~0x80) {
			case XCB_MAP_REQUEST:
				map_window((xcb_map_request_event_t *) event);
				break;
			case XCB_CONFIGURE_REQUEST:
				configure_request((xcb_configure_request_event_t *) event);
				break;
			case XCB_BUTTON_PRESS:
				button_press((xcb_button_press_event_t *) event);
				break;
			case XCB_BUTTON_RELEASE:
				xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
				break;
			case XCB_KEY_PRESS:
				key_press((xcb_key_press_event_t *) event);
				break;
			default:
				fprintf(stderr, "Unknown opcode: %d\n", event->response_type);
				break;
		}

		free(event);
	}

	xcb_flush(connection);

	return event_status;
}

void map_window(xcb_map_request_event_t *e) {
	xcb_window_t w = e->window;
	xcb_map_window(connection, w);
	setfocus(w);
	xcb_flush(connection);
}

void configure_request(xcb_configure_request_event_t *e) {
	xcb_window_t w = e->window;
	int vals[4] = {0};
	int mask = 0;
	int i = 0;

	if (e->x) {
		mask |= XCB_CONFIG_WINDOW_X;
		vals[i++] = e->x;
	}
	if (e->y) {
		mask |= XCB_CONFIG_WINDOW_Y;
		vals[i++] = e->y;
	}
	if (e->width > 1) { // Firefox: clicking "About Firefox" tries resizing to 1x1?
		mask |= XCB_CONFIG_WINDOW_WIDTH;
		vals[i++] = e->width;
	}
	if (e->height > 1) {
		mask |= XCB_CONFIG_WINDOW_HEIGHT;
		vals[i++] = e->height;
	}

	xcb_configure_window(connection, w, mask, vals);

	if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
		vals[0] = e->sibling;
		xcb_configure_window(connection, w, XCB_CONFIG_WINDOW_SIBLING, vals);
	}
	if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
		vals[0] = e->stack_mode;
		xcb_configure_window(connection, w, XCB_CONFIG_WINDOW_STACK_MODE, vals);
	}

	xcb_flush(connection);
}

void resize_window(xcb_window_t w, int x, int y) {
	int vals[] = {x, y};
	if (w && w != root_window) {
		xcb_configure_window(connection, w, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, vals);
	}
}

void setfocus(xcb_window_t w) {
	if (w && w != root_window) {
		xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, w, XCB_CURRENT_TIME);
	}
}

void button_press(xcb_button_press_event_t *e) {
	dw = e->child;
	int vals[] = {XCB_STACK_MODE_ABOVE};
	xcb_configure_window(connection, dw, XCB_CONFIG_WINDOW_STACK_MODE, vals);
	xcb_grab_pointer(connection, 0, root_window, XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_POINTER_MOTION_HINT, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, root_window, XCB_NONE, XCB_CURRENT_TIME);
}

void key_press(xcb_key_press_event_t *e) {
	;
}

int main(int argc, char **argv) {
	int event_status = 0;

	init_xcb();
	while (event_status == 0) {
		event_status = get_events();
	}
	xcb_disconnect(connection);

	return event_status;
}
