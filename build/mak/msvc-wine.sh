#!/bin/sh

# This script sets up a build environment for the command-line Visual C compiler
# (like the one from the free Visual Studio Express version) for usage under
# Linux with WINE. You must have set up a proper WINE environment including
# ability to run EXE files directly.

# You can specify the path to MSVC compiler in two ways:
# a) Via environment, e.g. "export MSVC_PATH=/usr/somewhere/msvc"
# b) Via command line, e.g. "./msvc.sh /usr/somewhere/msvc"

if [ -n "$1" ]; then
        MSVC_PATH="$1"
elif [ -z "$MSVC_PATH" ]; then
        MSVC_PATH="/usr/local/msvc"
fi

export PATH=$MSVC_PATH/bin:$PATH
export INCLUDE=$MSVC_PATH/include
export LIB="`winepath -w $MSVC_PATH/lib`;`winepath -w $MSVC_PATH/gtk/bin`"
export SDKDIR=$MSVC_PATH/gtk

# The path where additional DLLs are
DLLPATH=`winepath -w $MSVC_PATH/gtk/bin`

# The "//\\/\\\\" may be a bit scary, but it actually replaces every
# single backslash with a double backslash to workaround shell quoting
if ! wine cmd.exe /c echo "%PATH%" | grep -q "${DLLPATH//\\/\\\\}"; then
        echo "Please add Path=$DLLPATH"
        echo "to the HKEY_CURRENT_USER\Environment registry key, otherwise"
        echo "you won't be able to run compiled executables."
        echo ""
fi

if [ -z "$2" ]; then
        echo ""
        echo "The environment is set up for using the Microsoft Visual C compiler."
        echo "Now you can execute e.g. configure or make, or alternatively you may"
        echo "type 'exit' to end this session."
        echo ""

        exec ${SHELL:-/bin/sh}
fi
