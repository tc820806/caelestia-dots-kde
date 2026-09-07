#!/bin/bash
# The caelestia-cli binary isn't always at /usr/bin/caelestia (this KDE
# build installs it to ~/.local/bin), so resolve it instead of hardcoding
# a path that may not exist -- a failed relaunch here just kills the shell
# and never brings it back.
CAELESTIA_BIN="$HOME/.local/bin/caelestia"
if [[ ! -x "$CAELESTIA_BIN" ]]; then
    CAELESTIA_BIN="$(command -v caelestia 2>/dev/null || echo /usr/bin/caelestia)"
fi

"$CAELESTIA_BIN" shell -k 2>/dev/null
sleep 1.3

if pgrep -x quickshell > /dev/null; then
    killall -w quickshell 2>/dev/null
fi
if pgrep -x qs > /dev/null; then
    killall -w qs 2>/dev/null
fi

# Wipe the stale Quickshell socket locks
rm -rf "${XDG_RUNTIME_DIR:-/run/user/$UID}/quickshell/"*

source /etc/profile
[ -f ~/.profile ] && source ~/.profile
[ -f ~/.bashrc ] && source ~/.bashrc
export QML2_IMPORT_PATH="$HOME/.local/lib/qt6/qml"
export CAELESTIA_LIB_DIR="$HOME/.local/lib/caelestia"
export QS_NO_RELOAD_POPUP=1
export QS_DROP_EXPENSIVE_FONTS=1
export QS_DISABLE_CRASH_HANDLER=1
export QSG_RENDER_LOOP=threaded
export QT_QUICK_FLICKABLE_WHEEL_DECELERATION=10000
# Works around a Qt 6.8 QML-engine JIT bug (QQmlPropertyCache::createMetaObject /
# QQmlInterceptorMetaObject::toDynamicMetaObject SIGSEGV) that this build hits
# reliably when many property-interceptor-bound objects (Behavior/Animation)
# are created quickly, e.g. typing fast in the launcher's app search.
export QV4_FORCE_INTERPRETER=1

# Occasionally segfaults a few seconds into startup (unrelated native QML
# engine crash); retrying almost always succeeds, so retry a few times
# before giving up instead of leaving the shell dead.
MAX_ATTEMPTS=5
RETRY_DELAY=3
attempt=1
while (( attempt <= MAX_ATTEMPTS )); do
    "$CAELESTIA_BIN" shell -d
    sleep "$RETRY_DELAY"
    if pgrep -x quickshell > /dev/null || pgrep -x qs > /dev/null; then
        exit 0
    fi
    echo "[restart_shell] Caelestia shell did not stay up (attempt $attempt/$MAX_ATTEMPTS), retrying..." >&2
    rm -rf "${XDG_RUNTIME_DIR:-/run/user/$UID}/quickshell/"*
    attempt=$((attempt + 1))
done

echo "[restart_shell] Caelestia shell failed to start after $MAX_ATTEMPTS attempts." >&2
exit 1
