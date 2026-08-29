#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/extensions/record.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/uinput.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int ufd = -1;
static volatile sig_atomic_t running = 1;
static bool have_pos = false;
static int last_x = 0, last_y = 0;
static uint64_t last_motion_ms = 0;
static bool needs_edge_calibration = true;

static void on_signal(int sig) { (void)sig; running = 0; }

static int emit_event(uint16_t type, uint16_t code, int32_t value) {
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.code = code;
    ev.value = value;
    return write(ufd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev) ? 0 : -1;
}

static void sync_events(void) { emit_event(EV_SYN, SYN_REPORT, 0); }

static int setup_uinput(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) { perror("open /dev/uinput"); return -1; }

    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0 ||
        ioctl(fd, UI_SET_EVBIT, EV_REL) < 0) {
        perror("UI_SET_EVBIT"); close(fd); return -1;
    }

    for (int key = 0; key <= KEY_MAX; ++key)
        ioctl(fd, UI_SET_KEYBIT, key);

    ioctl(fd, UI_SET_RELBIT, REL_X);
    ioctl(fd, UI_SET_RELBIT, REL_Y);
    ioctl(fd, UI_SET_RELBIT, REL_WHEEL);
    ioctl(fd, UI_SET_RELBIT, REL_HWHEEL);

    struct uinput_setup usetup;
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_VIRTUAL;
    usetup.id.vendor = 0x1d6b;
    usetup.id.product = 0xad99;
    usetup.id.version = 1;
    snprintf(usetup.name, UINPUT_MAX_NAME_SIZE, "AnyDesk Wayland Bridge");

    if (ioctl(fd, UI_DEV_SETUP, &usetup) < 0) {
        perror("UI_DEV_SETUP"); close(fd); return -1;
    }
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        perror("UI_DEV_CREATE"); close(fd); return -1;
    }
    usleep(150000);
    return fd;
}

static void absolute_sync_once(int x, int y) {
    pid_t pid = fork();
    if (pid == 0) {
        char expr[160];
        snprintf(expr, sizeof(expr), "hl.dsp.cursor.move({ x = %d, y = %d })", x, y);
        execl("/usr/bin/hyprctl", "hyprctl", "dispatch", expr, (char *)NULL);
        _exit(127);
    }
    if (pid > 0) waitpid(pid, NULL, 0);
}

static uint64_t monotonic_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static void warp_x_pointer(Display *dpy, int x, int y) {
    if (!dpy) return;
    XWarpPointer(dpy, None, DefaultRootWindow(dpy), 0, 0, 0, 0, x, y);
    XFlush(dpy);
}

static void edge_calibrate(Display *dpy, int target_x, int target_y) {
    const int screen = DefaultScreen(dpy);
    const int edge_x = DisplayWidth(dpy, screen) - 1;
    const int edge_y = DisplayHeight(dpy, screen) - 1;

    /* AnyDesk's remote pointer may start with a stale internal offset.
       Saturating both pointer spaces at the same screen edge removes that offset. */
    absolute_sync_once(edge_x, edge_y);
    warp_x_pointer(dpy, edge_x, edge_y);
    usleep(20000);

    /* Return immediately to the position of the first real remote event. */
    warp_x_pointer(dpy, target_x, target_y);
    absolute_sync_once(target_x, target_y);

    last_x = target_x;
    last_y = target_y;
    have_pos = true;
    needs_edge_calibration = false;
    fprintf(stderr, "pointer edge-calibrated via %d,%d -> %d,%d\n",
            edge_x, edge_y, target_x, target_y);
    fflush(stderr);
}

static void handle_motion(Display *dpy, int x, int y) {
    uint64_t now = monotonic_ms();

    /* A long idle usually means a new AnyDesk connection/session. Recalibrate
       automatically on the first motion so the remote cursor never starts offset. */
    if (last_motion_ms != 0 && now - last_motion_ms > 3000) {
        needs_edge_calibration = true;
        have_pos = false;
    }
    last_motion_ms = now;

    if (needs_edge_calibration || !have_pos) {
        edge_calibrate(dpy, x, y);
        return;
    }

    int dx = x - last_x;
    int dy = y - last_y;
    last_x = x;
    last_y = y;
    if (dx || dy) {
        emit_event(EV_REL, REL_X, dx);
        emit_event(EV_REL, REL_Y, dy);
        sync_events();
    }
}

static int mouse_button_code(unsigned int b) {
    switch (b) {
        case 1: return BTN_LEFT;
        case 2: return BTN_MIDDLE;
        case 3: return BTN_RIGHT;
        case 8: return BTN_SIDE;
        case 9: return BTN_EXTRA;
        default: return -1;
    }
}

static void record_cb(XPointer closure, XRecordInterceptData *data) {
    Display *ctrl_dpy = (Display *)closure;
    if (!data) return;
    if (!running) { XRecordFreeData(data); return; }
    if (data->category != XRecordFromServer || data->data_len < 8) {
        XRecordFreeData(data); return;
    }

    const unsigned char *p = data->data;
    unsigned long bytes = data->data_len * 4UL;
    for (unsigned long off = 0; off + 32 <= bytes; off += 32) {
        const xEvent *e = (const xEvent *)(p + off);
        int type = e->u.u.type & 0x7f;
        int x = e->u.keyButtonPointer.rootX;
        int y = e->u.keyButtonPointer.rootY;

        switch (type) {
            case MotionNotify:
                handle_motion(ctrl_dpy, x, y);
                break;
            case KeyPress:
            case KeyRelease: {
                int xcode = e->u.u.detail;
                int evdev = xcode - 8;
                if (evdev >= 0 && evdev <= KEY_MAX) {
                    emit_event(EV_KEY, (uint16_t)evdev, type == KeyPress ? 1 : 0);
                    sync_events();
                }
                break;
            }
            case ButtonPress:
            case ButtonRelease: {
                unsigned int b = e->u.u.detail;
                if (b >= 4 && b <= 7) {
                    if (type == ButtonPress) {
                        if (b == 4) emit_event(EV_REL, REL_WHEEL, 1);
                        if (b == 5) emit_event(EV_REL, REL_WHEEL, -1);
                        if (b == 6) emit_event(EV_REL, REL_HWHEEL, -1);
                        if (b == 7) emit_event(EV_REL, REL_HWHEEL, 1);
                        sync_events();
                    }
                } else {
                    int code = mouse_button_code(b);
                    if (code >= 0) {
                        if (type == ButtonPress) {
                            absolute_sync_once(x, y);
                            last_x = x;
                            last_y = y;
                            have_pos = true;
                        }
                        emit_event(EV_KEY, (uint16_t)code, type == ButtonPress ? 1 : 0);
                        sync_events();
                    }
                }
                break;
            }
            default:
                break;
        }
    }
    XRecordFreeData(data);
}

int main(void) {
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    ufd = setup_uinput();
    if (ufd < 0) return 2;

    Display *ctrl = XOpenDisplay(NULL);
    Display *data = XOpenDisplay(NULL);
    if (!ctrl || !data) {
        fprintf(stderr, "cannot connect to DISPLAY=%s\n", getenv("DISPLAY") ? getenv("DISPLAY") : "(unset)");
        return 3;
    }

    int major = 0, minor = 0;
    if (!XRecordQueryVersion(ctrl, &major, &minor)) {
        fprintf(stderr, "XRecord unavailable\n"); return 4;
    }

    XRecordRange *range = XRecordAllocRange();
    if (!range) return 5;
    range->device_events.first = KeyPress;
    range->device_events.last = MotionNotify;
    XRecordClientSpec clients = XRecordAllClients;
    XRecordContext ctx = XRecordCreateContext(ctrl, 0, &clients, 1, &range, 1);
    XFree(range);
    if (!ctx) return 6;

    fprintf(stderr, "AnyDesk input relay ready: DISPLAY=%s XRecord=%d.%d\n",
            getenv("DISPLAY") ? getenv("DISPLAY") : "", major, minor);
    fflush(stderr);

    XSync(ctrl, False);
    XRecordEnableContext(data, ctx, record_cb, (XPointer)ctrl);

    XRecordDisableContext(ctrl, ctx);
    XRecordFreeContext(ctrl, ctx);
    XCloseDisplay(data);
    XCloseDisplay(ctrl);
    ioctl(ufd, UI_DEV_DESTROY);
    close(ufd);
    return 0;
}
