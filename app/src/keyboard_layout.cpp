// SPDX-License-Identifier: GPL-3.0-or-later
#include "keyboard_layout.h"
#include "scancodes.h"

#include <cstring>
#include <memory>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

namespace {

using Context = std::unique_ptr<xkb_context, decltype(&xkb_context_unref)>;

Context newContext()
{
    // The environment's XKB_DEFAULT_* only count in loadSystem(), never behind a test's back.
    return {xkb_context_new(XKB_CONTEXT_NO_ENVIRONMENT_NAMES), &xkb_context_unref};
}

// Keys that type a character: letters, digits, punctuation and the ISO 102nd key. The rest
// (Enter, F1, arrows…) keep their names.
bool typingKey(int usage)
{
    return (usage >= 0x04 && usage <= 0x27) || (usage >= 0x2d && usage <= 0x38) || usage == 0x64;
}

// Dead keys type their accent on the next key; the cap shows the accent alone.
uint32_t deadKeyChar(xkb_keysym_t sym)
{
    switch (sym) {
    case XKB_KEY_dead_grave: return U'`';
    case XKB_KEY_dead_acute: return U'´';
    case XKB_KEY_dead_circumflex: return U'^';
    case XKB_KEY_dead_tilde: return U'~';
    case XKB_KEY_dead_diaeresis: return U'¨';
    case XKB_KEY_dead_cedilla: return U'¸';
    case XKB_KEY_dead_abovering: return U'°';
    case XKB_KEY_dead_caron: return U'ˇ';
    case XKB_KEY_dead_macron: return U'¯';
    }
    return 0;
}

xkb_keysym_t symAt(xkb_keymap *keymap, xkb_keycode_t key, xkb_level_index_t level)
{
    const xkb_keysym_t *syms = nullptr;
    return xkb_keymap_key_get_syms_by_level(keymap, key, 0, level, &syms) == 1 ? syms[0] : XKB_KEY_NoSymbol;
}

// The keymap the compositor sends with wl_keyboard: what this seat types right now.
QByteArray waylandKeymap()
{
    wl_display *display = wl_display_connect(nullptr);
    if (!display)
        return {};
    struct State {
        wl_seat *seat = nullptr;
        wl_keyboard *keyboard = nullptr;
        QByteArray keymap;
    } state;

    static const wl_keyboard_listener keyboardListener = {
        [](void *data, wl_keyboard *, uint32_t format, int32_t fd, uint32_t size) {
            if (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 && size > 0) {
                void *map = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
                if (map != MAP_FAILED) {
                    // The text ends with a NUL that isn't part of the keymap.
                    static_cast<State *>(data)->keymap = QByteArray(static_cast<const char *>(map), int(strnlen(static_cast<const char *>(map), size)));
                    munmap(map, size);
                }
            }
            close(fd);
        },
        [](void *, wl_keyboard *, uint32_t, wl_surface *, wl_array *) {},
        [](void *, wl_keyboard *, uint32_t, wl_surface *) {},
        [](void *, wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t) {},
        [](void *, wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {},
        nullptr,  // repeat_info: version 4, not bound
    };
    static const wl_seat_listener seatListener = {
        [](void *data, wl_seat *seat, uint32_t caps) {
            auto *s = static_cast<State *>(data);
            if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !s->keyboard) {
                s->keyboard = wl_seat_get_keyboard(seat);
                wl_keyboard_add_listener(s->keyboard, &keyboardListener, s);
            }
        },
        nullptr,  // name: version 2, not bound
    };
    static const wl_registry_listener registryListener = {
        [](void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t) {
            auto *s = static_cast<State *>(data);
            if (!s->seat && std::strcmp(interface, wl_seat_interface.name) == 0) {
                s->seat = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
                wl_seat_add_listener(s->seat, &seatListener, s);
            }
        },
        [](void *, wl_registry *, uint32_t) {},
    };

    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registryListener, &state);
    // Globals, then the seat's capabilities, then the keyboard's keymap.
    for (int i = 0; i < 3 && state.keymap.isEmpty(); ++i)
        if (wl_display_roundtrip(display) < 0)
            break;
    if (state.keyboard)
        wl_keyboard_destroy(state.keyboard);
    if (state.seat)
        wl_seat_destroy(state.seat);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    return state.keymap;
}

}  // namespace

KeyboardLayout::KeyboardLayout(QObject *parent) : QObject(parent) {}

bool KeyboardLayout::loadSystem()
{
    if (loadKeymap(waylandKeymap()))
        return true;
    const QString layout = qEnvironmentVariable("XKB_DEFAULT_LAYOUT");
    return !layout.isEmpty() && loadNames(layout.section(',', 0, 0), qEnvironmentVariable("XKB_DEFAULT_VARIANT").section(',', 0, 0));
}

bool KeyboardLayout::loadKeymap(const QByteArray &text)
{
    if (text.isEmpty())
        return false;
    Context context = newContext();
    if (!context)
        return false;
    return load(xkb_keymap_new_from_buffer(context.get(), text.constData(), size_t(text.size()),
                                           XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS));
}

bool KeyboardLayout::loadNames(const QString &layout, const QString &variant)
{
    Context context = newContext();
    if (!context)
        return false;
    const QByteArray l = layout.toUtf8(), v = variant.toUtf8();
    xkb_rule_names names{};
    names.layout = l.constData();
    names.variant = v.constData();
    return load(xkb_keymap_new_from_names(context.get(), &names, XKB_KEYMAP_COMPILE_NO_FLAGS));
}

bool KeyboardLayout::load(xkb_keymap *keymap)
{
    if (!keymap)
        return false;
    QStringList labels(256);
    for (int code = 0; code < 256; ++code) {
        const int usage = usageForEvdev(code);
        if (!typingKey(usage))
            continue;
        const xkb_keycode_t key = code + kXkbKeycodeOffset;
        const xkb_keysym_t sym = symAt(keymap, key, 0);
        uint32_t c = xkb_keysym_to_utf32(sym);
        if (c == 0)
            c = deadKeyChar(sym);
        if (c < 0x20 || c == 0x7f)
            continue;  // nothing printable: the US name stays
        // Keycaps print letters as capitals: Shift gives the capital of what the key types.
        if (xkb_keysym_to_upper(sym) != sym && symAt(keymap, key, 1) == xkb_keysym_to_upper(sym))
            c = xkb_keysym_to_utf32(xkb_keysym_to_upper(sym));
        labels[usage] = QString::fromUcs4(reinterpret_cast<const char32_t *>(&c), 1);
    }
    const char *name = xkb_keymap_layout_get_name(keymap, 0);
    m_name = name ? QString::fromUtf8(name) : QString();
    m_labels = labels;
    xkb_keymap_unref(keymap);
    emit changed();
    return true;
}
