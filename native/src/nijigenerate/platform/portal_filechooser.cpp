// Native transport for the independent BSD-2 dportals 0.1.0 FileChooser API.
// Its option schema and Promise response behavior originate with Luna (2022).
// Tick, dictionary and reference ordering follows ddbus 3.0.0-beta.2 (Tristan Hume, 2017, MIT).
// Attribution and package-declared licenses: native/port/licenses/portal-filechooser.txt.
#include "portal_filechooser.hpp"
#include <nijigenerate/compat/source_c_string.hpp>
#include <SDL3/SDL.h>
#include <dbus/dbus.h>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>

namespace nijigenerate::platform::portal {
namespace {
struct Error {
    DBusError value;
    Error() { dbus_error_init(&value); }
    ~Error() { dbus_error_free(&value); }
    void check() {
        if (dbus_error_is_set(&value)) throw std::runtime_error(value.message ? value.message : "D-Bus error");
    }
};
struct Connection {
    DBusConnection* value = nullptr;
    Connection() = default;
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    ~Connection() { if (value) dbus_connection_unref(value); }
};
thread_local Connection connection;
struct MessageDeleter { void operator()(DBusMessage* value) const { if (value) dbus_message_unref(value); } };
using Message = std::unique_ptr<DBusMessage, MessageDeleter>;
void append(DBusMessageIter& where, int type, const void* value) {
    if (!dbus_message_iter_append_basic(&where, type, value)) throw std::bad_alloc();
}
void string(DBusMessageIter& where, const char* value) { append(where, DBUS_TYPE_STRING, &value); }
void open(DBusMessageIter& parent, int type, const char* signature, DBusMessageIter& child) {
    if (!dbus_message_iter_open_container(&parent, type, signature, &child)) throw std::bad_alloc();
}
void close(DBusMessageIter& parent, DBusMessageIter& child) {
    if (!dbus_message_iter_close_container(&parent, &child)) throw std::bad_alloc();
}
void booleanOption(DBusMessageIter& options, const char* key, bool value) {
    DBusMessageIter entry, variant;
    open(options, DBUS_TYPE_DICT_ENTRY, nullptr, entry); string(entry, key);
    open(entry, DBUS_TYPE_VARIANT, "b", variant);
    const dbus_bool_t flag = value;
    append(variant, DBUS_TYPE_BOOLEAN, &flag);
    close(entry, variant); close(options, entry);
}
void filtersOption(DBusMessageIter& options, std::span<const tinyfiledialogs::TFD_Filter> filters) {
    if (filters.empty()) return;
    DBusMessageIter entry, variant, array;
    open(options, DBUS_TYPE_DICT_ENTRY, nullptr, entry); string(entry, "filters");
    open(entry, DBUS_TYPE_VARIANT, "a(sa(us))", variant);
    open(variant, DBUS_TYPE_ARRAY, "(sa(us))", array);
    for (const auto& filter : filters) {
        DBusMessageIter item, patterns;
        open(array, DBUS_TYPE_STRUCT, nullptr, item);
        string(item, filter.description ? filter.description : "");
        open(item, DBUS_TYPE_ARRAY, "(us)", patterns);
        for (std::size_t index = 0; index < filter.patterns.size(); ++index) {
            DBusMessageIter pattern;
            open(patterns, DBUS_TYPE_STRUCT, nullptr, pattern);
            // Preserve source tfdToFileFilter's actual per-pattern index tag.
            const dbus_uint32_t tag = static_cast<dbus_uint32_t>(index);
            append(pattern, DBUS_TYPE_UINT32, &tag);
            string(pattern, filter.patterns[index] ? filter.patterns[index] : "");
            close(patterns, pattern);
        }
        close(item, patterns); close(array, item);
    }
    close(variant, array); close(entry, variant); close(options, entry);
}
std::string parentWindow(SDL_Window* window) {
    const char* driver = SDL_GetCurrentVideoDriver();
    if (!window || !driver || std::strcmp(driver, "x11") != 0) return {};
    const auto handle = SDL_GetNumberProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    char buffer[32];
    const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), static_cast<std::uint64_t>(handle), 16);
    if (converted.ec != std::errc{}) throw std::runtime_error("Cannot format X11 portal window handle");
    // std.conv.to!string(value, 16) defaults to LetterCase.upper.
    for (char* digit = buffer; digit != converted.ptr; ++digit)
        if (*digit >= 'a' && *digit <= 'f') *digit = static_cast<char>(*digit - 'a' + 'A');
    return "x11:" + std::string(buffer, converted.ptr);
}
struct Pending {
    std::string path;
    bool done = false;
    std::optional<std::string> uri;
    std::exception_ptr failure;
};
DBusHandlerResult response(DBusConnection*, DBusMessage* message, void* data) noexcept {
    auto& pending = *static_cast<Pending*>(data);
    const char* path = dbus_message_get_path(message);
    if (pending.path.empty() || !path || pending.path != path ||
        !dbus_message_is_signal(message, "org.freedesktop.portal.Request", "Response"))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    try {
        const Message received(dbus_message_ref(message));
        DBusMessageIter args;
        if (!dbus_message_iter_init(message, &args) || dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_UINT32)
            throw std::runtime_error("Invalid portal response code");
        dbus_uint32_t code = 0; dbus_message_iter_get_basic(&args, &code);
        if (code == 0) {
            if (!dbus_message_iter_next(&args) || dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_ARRAY)
                throw std::runtime_error("Portal promise has no result dictionary");
            std::optional<DBusMessageIter> uriValue;
            DBusMessageIter entries; dbus_message_iter_recurse(&args, &entries);
            for (; dbus_message_iter_get_arg_type(&entries) != DBUS_TYPE_INVALID; dbus_message_iter_next(&entries)) {
                if (dbus_message_iter_get_arg_type(&entries) != DBUS_TYPE_DICT_ENTRY)
                    throw std::runtime_error("Invalid portal result entry");
                DBusMessageIter entry; dbus_message_iter_recurse(&entries, &entry);
                if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_STRING)
                    throw std::runtime_error("Invalid portal result key");
                const char* key = nullptr; dbus_message_iter_get_basic(&entry, &key);
                if (!dbus_message_iter_next(&entry) || dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_VARIANT)
                    throw std::runtime_error("Invalid portal result variant");
                if (!key || std::strcmp(key, "uris") != 0) continue;
                uriValue.emplace();
                dbus_message_iter_recurse(&entry, &*uriValue);
            }
            if (!uriValue) throw std::runtime_error("Portal promise has no URI result");
            if (dbus_message_iter_get_arg_type(&*uriValue) != DBUS_TYPE_ARRAY)
                throw std::runtime_error("Invalid portal URI array");
            DBusMessageIter uris; dbus_message_iter_recurse(&*uriValue, &uris);
            if (dbus_message_iter_get_arg_type(&uris) != DBUS_TYPE_STRING)
                throw std::runtime_error("Portal promise has no first URI");
            const char* uri = nullptr; dbus_message_iter_get_basic(&uris, &uri);
            pending.uri = uri ? uri : "";
        }
        // Cancellation and EndedOther complete successfully with no selected URI.
    } catch (...) { pending.failure = std::current_exception(); }
    pending.done = true;
    return DBUS_HANDLER_RESULT_HANDLED;
}
struct Subscription {
    DBusConnection* bus;
    Pending* pending;
    static constexpr const char* rule = "type='signal',interface='org.freedesktop.portal.Request',member='Response'";
    Subscription(DBusConnection* value, Pending& state) : bus(value), pending(&state) {
        if (!dbus_connection_add_filter(bus, &response, pending, nullptr)) throw std::bad_alloc();
        Error error;
        dbus_bus_add_match(bus, rule, &error.value);
        if (dbus_error_is_set(&error.value)) {
            dbus_connection_remove_filter(bus, &response, pending);
            error.check();
        }
    }
    ~Subscription() {
        dbus_connection_remove_filter(bus, &response, pending);
        Error error; dbus_bus_remove_match(bus, rule, &error.value);
    }
};
}
void ngInitializePortal() {
    Error error;
    Connection next;
    next.value = dbus_bus_get(DBUS_BUS_SESSION, &error.value);
    error.check();
    if (!next.value) throw std::runtime_error("Cannot connect to the session bus");
    if (connection.value) dbus_connection_unref(connection.value);
    connection.value = std::exchange(next.value, nullptr);
}
bool ngUpdatePortal() {
    // ddbus.tick takes Connection by value: retain the evaluated current
    // connection even when dispatch callbacks replace the TLS descriptor.
    Connection tickConnection;
    tickConnection.value = dbus_connection_ref(connection.value);
    return dbus_connection_read_write_dispatch(tickConnection.value, 0) != 0;
}
std::optional<std::string> ngChoosePortalFile(SDL_Window* window, std::string_view title,
        std::span<const tinyfiledialogs::TFD_Filter> filters, bool save, bool multiple, bool directory) {
    if (!connection.value) throw std::runtime_error("Portal connection is not initialized");
    Connection requestConnection;
    requestConnection.value = dbus_connection_ref(connection.value);
    auto* const bus = requestConnection.value;
    Pending pending;
    Subscription subscription(bus, pending);
    Message request(dbus_message_new_method_call("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.FileChooser", save ? "SaveFile" : "OpenFile"));
    if (!request) throw std::bad_alloc();
    DBusMessageIter args, options;
    dbus_message_iter_init_append(request.get(), &args);
    const auto parent = parentWindow(window);
    const compat::SourceCString parentString{compat::RetainedStringSlice(compat::RetainedString(parent))};
    string(args, parentString.data());
    const compat::SourceCString caption{compat::RetainedStringSlice(compat::RetainedString(title))};
    string(args, caption.data());
    open(args, DBUS_TYPE_ARRAY, "{sv}", options);
    booleanOption(options, "modal", true);
    if (!save) { booleanOption(options, "multiple", multiple); booleanOption(options, "directory", directory); }
    filtersOption(options, filters); close(args, options);
    Error error;
    Message reply(dbus_connection_send_with_reply_and_block(bus, request.get(), -1, &error.value));
    error.check();
    if (!reply) throw std::runtime_error("Portal returned no request handle");
    DBusMessageIter handle;
    if (!dbus_message_iter_init(reply.get(), &handle) || dbus_message_iter_get_arg_type(&handle) != DBUS_TYPE_OBJECT_PATH)
        throw std::runtime_error("Portal returned an invalid request handle");
    const char* path = nullptr; dbus_message_iter_get_basic(&handle, &path);
    if (!path) throw std::runtime_error("Portal returned a null request path");
    pending.path = path;
    // Promise.await calls dpUpdate on the current connection and ignores tick's result.
    while (!pending.done) {
        ngUpdatePortal();
    }
    if (pending.failure) std::rethrow_exception(pending.failure);
    return pending.uri;
}
}
