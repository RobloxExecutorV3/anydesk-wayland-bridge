#define _GNU_SOURCE
#include <dlfcn.h>
#include <gio/gio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void adw_log(const char *fmt, ...) {
    int fd = open("/tmp/anydesk-wayland-shim.log", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0666);
    if (fd < 0) return;
    dprintf(fd, "[pid=%d uid=%d] ", getpid(), getuid());
    va_list ap;
    va_start(ap, fmt);
    vdprintf(fd, fmt, ap);
    va_end(ap);
    dprintf(fd, "\n");
    close(fd);
}

static int is_login1(const char *s) {
    return s && strstr(s, "org.freedesktop.login1");
}

typedef GDBusProxy *(*proxy_new_bus_sync_fn)(GBusType, GDBusProxyFlags, GDBusInterfaceInfo *, const gchar *, const gchar *, const gchar *, GCancellable *, GError **);
typedef GVariant *(*cached_prop_fn)(GDBusProxy *, const gchar *);
typedef void (*conn_call_fn)(GDBusConnection *, const gchar *, const gchar *, const gchar *, const gchar *, GVariant *, const GVariantType *, GDBusCallFlags, gint, GCancellable *, GAsyncReadyCallback, gpointer);
typedef GVariant *(*conn_call_sync_fn)(GDBusConnection *, const gchar *, const gchar *, const gchar *, const gchar *, GVariant *, const GVariantType *, GDBusCallFlags, gint, GCancellable *, GError **);
typedef void (*proxy_call_fn)(GDBusProxy *, const gchar *, GVariant *, GDBusCallFlags, gint, GCancellable *, GAsyncReadyCallback, gpointer);
typedef GVariant *(*proxy_call_sync_fn)(GDBusProxy *, const gchar *, GVariant *, GDBusCallFlags, gint, GCancellable *, GError **);

GDBusProxy *g_dbus_proxy_new_for_bus_sync(GBusType bus_type, GDBusProxyFlags flags,
        GDBusInterfaceInfo *info, const gchar *name, const gchar *object_path,
        const gchar *interface_name, GCancellable *cancellable, GError **error) {
    static proxy_new_bus_sync_fn real_fn;
    if (!real_fn) real_fn = (proxy_new_bus_sync_fn)dlsym(RTLD_NEXT, "g_dbus_proxy_new_for_bus_sync");
    if (is_login1(name) || is_login1(interface_name))
        adw_log("proxy-new bus_type=%d name=%s path=%s iface=%s", bus_type,
                name ? name : "", object_path ? object_path : "", interface_name ? interface_name : "");
    return real_fn(bus_type, flags, info, name, object_path, interface_name, cancellable, error);
}

GVariant *g_dbus_proxy_get_cached_property(GDBusProxy *proxy, const gchar *property_name) {
    static cached_prop_fn real_fn;
    if (!real_fn) real_fn = (cached_prop_fn)dlsym(RTLD_NEXT, "g_dbus_proxy_get_cached_property");
    const gchar *name = g_dbus_proxy_get_name(proxy);
    const gchar *iface = g_dbus_proxy_get_interface_name(proxy);
    if (is_login1(name) || is_login1(iface))
        adw_log("cached-property name=%s iface=%s property=%s", name ? name : "", iface ? iface : "", property_name ? property_name : "");
    return real_fn(proxy, property_name);
}

void g_dbus_connection_call(GDBusConnection *connection, const gchar *bus_name,
        const gchar *object_path, const gchar *interface_name, const gchar *method_name,
        GVariant *parameters, const GVariantType *reply_type, GDBusCallFlags flags,
        gint timeout_msec, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
    static conn_call_fn real_fn;
    if (!real_fn) real_fn = (conn_call_fn)dlsym(RTLD_NEXT, "g_dbus_connection_call");
    if (is_login1(bus_name) || is_login1(interface_name) || (object_path && strstr(object_path, "/org/freedesktop/login1"))) {
        gchar *p = parameters ? g_variant_print(parameters, TRUE) : NULL;
        adw_log("conn-call bus=%s path=%s iface=%s method=%s params=%s", bus_name ? bus_name : "",
                object_path ? object_path : "", interface_name ? interface_name : "", method_name ? method_name : "", p ? p : "()");
        g_free(p);
    }
    real_fn(connection, bus_name, object_path, interface_name, method_name, parameters,
            reply_type, flags, timeout_msec, cancellable, callback, user_data);
}

GVariant *g_dbus_connection_call_sync(GDBusConnection *connection, const gchar *bus_name,
        const gchar *object_path, const gchar *interface_name, const gchar *method_name,
        GVariant *parameters, const GVariantType *reply_type, GDBusCallFlags flags,
        gint timeout_msec, GCancellable *cancellable, GError **error) {
    static conn_call_sync_fn real_fn;
    if (!real_fn) real_fn = (conn_call_sync_fn)dlsym(RTLD_NEXT, "g_dbus_connection_call_sync");
    if (is_login1(bus_name) || is_login1(interface_name) || (object_path && strstr(object_path, "/org/freedesktop/login1"))) {
        gchar *p = parameters ? g_variant_print(parameters, TRUE) : NULL;
        adw_log("conn-call-sync bus=%s path=%s iface=%s method=%s params=%s", bus_name ? bus_name : "",
                object_path ? object_path : "", interface_name ? interface_name : "", method_name ? method_name : "", p ? p : "()");
        g_free(p);
    }
    return real_fn(connection, bus_name, object_path, interface_name, method_name, parameters,
                   reply_type, flags, timeout_msec, cancellable, error);
}

void g_dbus_proxy_call(GDBusProxy *proxy, const gchar *method_name, GVariant *parameters,
        GDBusCallFlags flags, gint timeout_msec, GCancellable *cancellable,
        GAsyncReadyCallback callback, gpointer user_data) {
    static proxy_call_fn real_fn;
    if (!real_fn) real_fn = (proxy_call_fn)dlsym(RTLD_NEXT, "g_dbus_proxy_call");
    const gchar *name = g_dbus_proxy_get_name(proxy);
    const gchar *iface = g_dbus_proxy_get_interface_name(proxy);
    if (is_login1(name) || is_login1(iface)) {
        gchar *p = parameters ? g_variant_print(parameters, TRUE) : NULL;
        adw_log("proxy-call name=%s iface=%s method=%s params=%s", name ? name : "", iface ? iface : "", method_name ? method_name : "", p ? p : "()");
        g_free(p);
    }
    real_fn(proxy, method_name, parameters, flags, timeout_msec, cancellable, callback, user_data);
}

GVariant *g_dbus_proxy_call_sync(GDBusProxy *proxy, const gchar *method_name, GVariant *parameters,
        GDBusCallFlags flags, gint timeout_msec, GCancellable *cancellable, GError **error) {
    static proxy_call_sync_fn real_fn;
    if (!real_fn) real_fn = (proxy_call_sync_fn)dlsym(RTLD_NEXT, "g_dbus_proxy_call_sync");

    const gchar *name = g_dbus_proxy_get_name(proxy);
    const gchar *iface = g_dbus_proxy_get_interface_name(proxy);
    int patch_display = 0;
    int patch_type = 0;

    if (method_name && strcmp(method_name, "Get") == 0 && parameters &&
        iface && strcmp(iface, "org.freedesktop.DBus.Properties") == 0 &&
        g_variant_is_of_type(parameters, G_VARIANT_TYPE("(ss)"))) {
        const gchar *target_iface = NULL;
        const gchar *property = NULL;
        g_variant_get(parameters, "(&s&s)", &target_iface, &property);
        if (target_iface && property &&
            strcmp(target_iface, "org.freedesktop.login1.Session") == 0) {
            patch_display = strcmp(property, "Display") == 0;
            patch_type = strcmp(property, "Type") == 0;
        }
    }

    if (is_login1(name) || is_login1(iface)) {
        gchar *p = parameters ? g_variant_print(parameters, TRUE) : NULL;
        adw_log("proxy-call-sync name=%s iface=%s method=%s params=%s patch_display=%d patch_type=%d",
                name ? name : "", iface ? iface : "", method_name ? method_name : "",
                p ? p : "()", patch_display, patch_type);
        g_free(p);
    }

    GVariant *ret = real_fn(proxy, method_name, parameters, flags, timeout_msec, cancellable, error);

    if (patch_display && ret) {
        const char *target_display = getenv("ANYDESK_BRIDGE_DISPLAY");
        if (!target_display || !target_display[0]) target_display = ":99";
        gchar *old = g_variant_print(ret, TRUE);
        adw_log("PATCH Session.Display: %s -> %s", old ? old : "(null)", target_display);
        g_free(old);
        g_variant_unref(ret);
        return g_variant_ref_sink(g_variant_new("(v)", g_variant_new_string(target_display)));
    }

    if (patch_type && ret) {
        gchar *old = g_variant_print(ret, TRUE);
        adw_log("PATCH Session.Type: %s -> x11", old ? old : "(null)");
        g_free(old);
        g_variant_unref(ret);
        return g_variant_ref_sink(g_variant_new("(v)", g_variant_new_string("x11")));
    }

    if ((is_login1(name) || is_login1(iface)) && ret) {
        gchar *r = g_variant_print(ret, TRUE);
        adw_log("proxy-call-sync-reply name=%s iface=%s method=%s reply=%s",
                name ? name : "", iface ? iface : "", method_name ? method_name : "", r ? r : "");
        g_free(r);
    }
    return ret;
}
