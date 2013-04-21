#
# A Python-based autoconf class
# Copyright (C) 2005-2007 Andrew Zabolotny
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import os
import sys
import platform
import re

# Public interface
__all__ = [
    "start", "add_config_h", "add_config_mak",
    "check_program", "check_compile", "check_compile_and_link",
    "check_cflags", "check_header",
    "check_library", "pkgconfig_check_library",
    "update_file", "read_file", "write_file",
    "abort_configure", "substmacros",
    "get_config_h", "get_config_mak"
]

# Default values for user-definable variables

# Project name
PROJ = "Unknown"
# Version number (major.minor.release.bugfix)
VERSION = "0.0.0.0"

PREFIX = "/usr/local"
BINDIR = None
DATADIR = None
SYSCONFDIR = None
LIBDIR = None
LIBEXECDIR = None
DOCDIR = None
INCLUDEDIR = None
VERBOSE = False
MODE = "release"
SHAREDLIBS = True

CONFIG_H = {}
_CONFIG_H = []
CONFIG_MAK = {}
_CONFIG_MAK = []

# pkg-config emulation for platforms without pkg-config
# NOTE: This has precedence over the pkg-config program!
PKGCONFIG = {}

# Default options: Short option, Long option, Metaname, Code, Description
OPTIONS = [
    [ "h",  "help",       None,    "help ()",
      "Displays this help text and exit" ],
    [ None, "prefix",     "DIR",   "global PREFIX; PREFIX = optarg",
      "Set the base prefix for installation" ],
    [ None, "bindir",     "DIR",   "global BINDIR; BINDIR = optarg",
      "Define the installation directory for executable files" ],
    [ None, "sysconfdir", "DIR",   "global SYSCONFDIR; SYSCONFDIR = optarg",
      "Define the installation directory for configuration files" ],
    [ None, "datadir",    "DIR",   "global DATADIR; DATADIR = optarg",
      "Define the installation directory for data files" ],
    [ None, "libdir",     "DIR",   "global LIBDIR; LIBDIR = optarg",
      "Define the installation directory for application libraries" ],
    [ None, "includedir", "DIR",   "global INCLUDEDIR; INCLUDEDIR = optarg",
      "Define the installation directory for include files" ],
    [ None, "libexecdir", "DIR",   "global LIBEXECDIR; LIBEXECDIR = optarg",
      "Define the installation for exectutable application components" ],
    [ None, "docdir",    "DIR",   "global DOCDIR; DOCDIR = optarg",
      "Define the installation directory for documentation files" ],
    [ "v",  "verbose",    None,    "global VERBOSE; VERBOSE = True",
      "Display verbosely the detection process" ],
    [ None, "mode",       "MODE",  "global MODE; MODE = optarg",
      "Use given compilation mode by default (debug|release)" ],
    [ None, "compiler",   "COMP",  "global COMPILER; COMPILER = optarg",
      "Use the respective compiler (supported: GCC|MSVC)" ],
    [ None, "cflags",     "FLAGS", "global CFLAGS; CFLAGS = optarg",
      "Additional compiler flags for C" ],
    [ None, "cxxflags",   "FLAGS", "global CXXFLAGS; CXXFLAGS = optarg",
      "Additional compiler flags for C++" ],
    [ None, "ldflags",    "FLAGS", "global LDFLAGS; LDFLAGS = optarg",
      "Additional linker flags" ],
    [ None, "libs",       "LIBS", "global LIBS; LIBS = optarg",
      "Additional libraries to link with" ],
    [ None, "target",     "PLATFORM", "set_target (optarg)",
      "Target platform{.arch}{.tune}, possible values:\n"\
      "(posix|windows|mac){.(x86|x86_64)}{.(generic|i686|athlon|...)}\n"\
      "If 'tune' is specified, code will be tuned for specific CPU" ],
    [ None, "staticlibs", "YESNO", "global SHAREDLIBS; SHAREDLIBS = not parse_bool ('staticlibs', optarg)",
      "Build all libs as static, even if they are shared by default" ],
]

# Environment variables picked up: Name, Default value, Description
ENVARS = [
    [ "COMPILER", "GCC", "The compiler suite to use (default: @@@).\n"
                         "NOTE: a file named build/mak/$COMPILER.mak must exist!" ],
    [ "CFLAGS",   "",    "Additional compilation flags for C" ],
    [ "CXXFLAGS", "",    "Additional compilation flags for C++" ],
    [ "LDFLAGS",  "",    "Additional linker flags" ],
    [ "LIBS",     "",    "Additional libraries to link with" ],
    [ "CC",       None,  "Override the C compiler name/path" ],
    [ "CXX",      None,  "Override the C++ compiler name/path" ],
    [ "LD",       None,  "Override the linker name/path" ],
    [ "AR",       None,  "Override the library manager name/path" ],
    [ "TKP",      "",    "Toolkit prefix (prepended to CC, CXX, LD, AR)" ]
]

# -------------- # Abstract interface to different compilers # --------------

class compiler_gcc:
    def __init__ (self):
        # Find our compiler and linker
        self.CC = CC or (TKP + "gcc")
        self.CXX = CXX or (TKP + "g++")
        self.LD = LD or (TKP + "g++")
        self.AR = AR or (TKP + "ar")
        self.OBJ = ".o"

    def startup (self):
        global CXXFLAGS, TARGET

        # Always use -fvisibility, if available, for shared libs
        # as suggested by gcc docs and mark exported symbols explicitly.
        if check_cflags ("-fvisibility=hidden", "CXXFLAGS", "-Werror") or \
           check_cflags ("-fvisibility-inlines-hidden", "CXXFLAGS", "-Werror"):
            add_config_h ("CONF_SYMBOL_VISIBILITY")

        check_cflags ("-Wno-non-virtual-dtor", "CXXFLAGS", "-Werror")
        check_cflags ("-mtune=" + TARGET [2], "CFLAGS")

        add_config_mak ("GCC.CC", self.CC + " -c")
        add_config_mak ("GCC.CXX", self.CXX + " -c")
        add_config_mak ("GCC.LD", self.LD)
        add_config_mak ("GCC.AR", self.AR)

    def c_compile (self, srcf, outf, cflags):
        # Return the command to compile a C file
        return self.CC + " -c " + srcf + " -o " + outf + " " + cflags;

    def cxx_compile (self, srcf, outf, cflags):
        # Return the command to compile a C++ file
        return self.CXX + " -c " + srcf + " -o " + outf + " " + cflags;

    def comlink (self, srcf, outf, cflags, libs):
        # Return the command to compile and link a C/C++ file
        return self.LD + " " + srcf + " -o " + outf + " " + cflags + " " + libs;

    def include (self, dirs):
        # The flags to add an include directory to compiler search path
        return "-I" + " -I".join (dirs.split ())

    def define (self, defs):
        # The flags to define a macro from compiler command line
        return "-D" + " -D".join (defs.split ())

    def linklib (self, library, path = None):
        # gcc: -L ../.. -lblah
        tmp = ""
        if path:
            tmp = "-L" + path + " "
        return tmp + "-l" + " -l".join (library.split ())


class compiler_msvc:
    def __init__ (self):
        # Find our compiler and linker
        self.CC = CC or "cl.exe"
        self.CXX = CXX or "cl.exe"
        self.LD = LD or "link.exe"
        self.OBJ = ".obj"

    def startup (self):
        add_config_mak ("MSVC.CC", self.CC + " -c")
        add_config_mak ("MSVC.CXX", self.CXX + " -c")
        add_config_mak ("MSVC.LD", self.LD)
        add_config_h ("CONF_SYMBOL_VISIBILITY")

    def c_compile (self, srcf, outf, cflags):
        return self.CC + " -c " + srcf + " -Fo" + outf + " " + cflags;

    def cxx_compile (self, srcf, outf, cflags):
        return self.CXX + " -c " + srcf + " -Fo" + outf + " " + cflags;

    def comlink (self, srcf, outf, cflags, libs):
        return self.CXX + " " + srcf + " -Fe" + outf + " " + cflags + " -link " + libs;

    def include (self, dirs):
        return "-I" + " -I".join (dirs.split ())

    def define (self, defs):
        return "-D" + " -D".join (defs.split ())

    def linklib (self, library, path = None):
        tmp = ""
        if path:
            tmp = "-libpath:" + path.replace ('/', '\\\\') + " "
        return tmp + ".lib ".join (library.split ()) + ".lib"


# A list of supported compilers
COMPILERS = {
    "GCC":  lambda: compiler_gcc (),
    "MSVC": lambda: compiler_msvc (),
}

#-----------------------------------------------------------------------------#

# Common initialization
def start ():
    """Start the autoconfiguring process. This includes searching the environment
    for variables, parsing the command line, detecting host/target and compiler
    and setting the base variables.
    """

    global HOST, TARGET, EXE, TOOLKIT, COMPILER
    global PREFIX, BINDIR, LIBDIR, SYSCONFDIR, DATADIR, DOCDIR
    global INCLUDEDIR, LIBEXECDIR, SHAREDLIBS
    global VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO, VERSION_BUGFIX

    detect_platform ()

    # Read environment variables first
    for e in ENVARS:
        globals () [e [0]] = os.getenv (e [0], e [1])

    # Parse command-line
    skip_opt = False
    for optidx in range (1, len (sys.argv)):
        if skip_opt:
            skip_opt = False
            continue

        opt = sys.argv [optidx]
        optarg = None
        opt_ok = False
        if opt [:2] == "--":
            opt = opt [2:]
            opt_short = False
            for o in OPTIONS:
                if o [1] and o [1] == opt [:len (o [1])]:
                    optarg = opt [len (o [1]):]
                    if optarg [:1] == '=' or len (opt) == len (o [1]):
                        opt_ok = True
                        optarg = optarg [1:]
                        break
        elif opt [:1] == "-":
            opt = opt [1:]
            opt_short = True
            for o in OPTIONS:
                if o [0] and o [0] == opt:
                    opt_ok = True
                    break

        if not opt_ok:
            print ("Unknown command-line option: '" + opt + "'")
            sys.exit (1)

        # Check if option needs argument
        if o [2] and opt_short:
            if optidx >= len (sys.argv):
                print ("Option '" + opt + "' needs an argument")
                sys.exit (1)

            skip_opt = True
            optarg = sys.argv [optidx + 1]

        if not o [2] and optarg:
            print ("Option '" + opt + "' does not accept an argument")
            sys.exit (1)

        exec (o [3])

    # Autodetect tune CPU, if required
    if TARGET [2] == "auto":
        cpu = platform.processor ()
        if (not cpu) or (len (cpu) == 0):
            cpu = platform.machine ()
        TARGET [2] = cpu.replace ("-", "_")

    # Print the host and target platforms
    print ("Compiling on host " + ".".join (HOST) + " for target " + \
          ".".join (TARGET [:2]) + " (tune for " + TARGET [2] + ")")

    # Now set target-specific defaults
    if TARGET [0] == "windows":
        EXE = ".exe"
    else:
        EXE = ""

    add_config_h ("CONF_PACKAGE", '"' + PROJ + '"')
    add_config_mak ("CONF_PACKAGE", PROJ)
    add_config_h ("CONF_VERSION", '"' + VERSION + '"')
    add_config_mak ("CONF_VERSION", VERSION)

    add_config_h ("PLATFORM_" + TARGET [0].upper ())
    add_config_h ("ARCH_" + TARGET [1].upper ())
    add_config_h ("TUNE_" + TARGET [2].upper ())

    add_config_mak ("HOST", HOST [0])
    add_config_mak ("TARGET", TARGET [0])
    add_config_mak ("ARCH", TARGET [1])
    add_config_mak ("TUNE", TARGET [2])

    if BINDIR is None:
        BINDIR = PREFIX + "/bin"
    if SYSCONFDIR is None:
        SYSCONFDIR = PREFIX + "/etc/" + PROJ
    if DATADIR is None:
        DATADIR = PREFIX + "/share/" + PROJ
    if DOCDIR is None:
        DOCDIR = PREFIX + "/share/doc/" + PROJ + "-" + VERSION
    # http://www.pathname.com/fhs/pub/fhs-2.3.html#LIB64
    if LIBDIR is None:
        if TARGET [1] [-2:] == "64":
            LIBDIR = PREFIX + "/lib64"
            # Debian doesn't follow LFS in this regard
            try:
                os.stat (LIBDIR)
            except:
                LIBDIR = PREFIX + "/lib"
        else:
            LIBDIR = PREFIX + "/lib"
    if INCLUDEDIR is None:
        INCLUDEDIR = PREFIX + "/include"
    if LIBEXECDIR is None:
        LIBEXECDIR = PREFIX + "/libexec/" + PROJ

    # Instantiate the compiler-dependent class
    COMPILER = COMPILER.upper ();
    TOOLKIT = COMPILERS.get (COMPILER);
    if not TOOLKIT:
        print ("Unsupported compiler: " + COMPILER)
        sys.exit (1)
    TOOLKIT = TOOLKIT ()
    TOOLKIT.startup ()

    add_config_h ("CONF_COMPILER_" + COMPILER)
    if PREFIX != "":
        add_config_h ("CONF_PREFIX", '"' + PREFIX + '"')
        add_config_mak ("CONF_PREFIX", PREFIX + '/')
    if BINDIR != "":
        add_config_h ("CONF_BINDIR", '"' + BINDIR + '"')
        add_config_mak ("CONF_BINDIR", BINDIR + '/')
    if SYSCONFDIR != "":
        add_config_h ("CONF_SYSCONFDIR", '"' + SYSCONFDIR + '"')
        add_config_mak ("CONF_SYSCONFDIR", SYSCONFDIR + '/')
    if DATADIR != "":
        add_config_h ("CONF_DATADIR", '"' + DATADIR + '"')
        add_config_mak ("CONF_DATADIR", DATADIR + '/')
    if LIBDIR != "":
        add_config_h ("CONF_LIBDIR", '"' + LIBDIR + '"')
        add_config_mak ("CONF_LIBDIR", LIBDIR + '/')
    if INCLUDEDIR != "":
        add_config_h ("CONF_INCLUDEDIR", '"' + INCLUDEDIR + '"')
        add_config_mak ("CONF_INCLUDEDIR", INCLUDEDIR + '/')
    if DOCDIR != "":
        add_config_h ("CONF_DOCDIR", '"' + DOCDIR + '"')
        add_config_mak ("CONF_DOCDIR", DOCDIR + '/')
    if LIBEXECDIR != "":
        add_config_h ("CONF_LIBEXECDIR", '"' + LIBEXECDIR + '"')
        add_config_mak ("CONF_LIBEXECDIR", LIBEXECDIR + '/')
    if SHAREDLIBS:
        add_config_h ("CONF_SHAREDLIBS", "1")
        add_config_mak ("SHAREDLIBS", "1")
    else:
        add_config_h ("CONF_SHAREDLIBS", "0")
        add_config_mak ("SHAREDLIBS", "")

    # Split version number into major/minor/micro/bugfix
    v = VERSION.split ('.')
    VERSION_MAJOR = '0'
    VERSION_MINOR = '0'
    VERSION_MICRO = '0'
    VERSION_BUGFIX = '0'
    if len (v) > 0:
        VERSION_MAJOR = v [0]
    if len (v) > 1:
        VERSION_MINOR = v [1]
    if len (v) > 2:
        VERSION_MICRO = v [2]
    if len (v) > 3:
        VERSION_BUGFIX = v [3]

    add_config_mak ("CONF_LIB_VERSION", VERSION_MAJOR + '.' + VERSION_MINOR + '.' + VERSION_MICRO)

# Autodetect platform and architecture
def detect_platform ():
    global HOST, TARGET, DEVNULL

    if sys.platform [:3] == "win":
        HOST = ["windows"]
        TARGET = ["windows"]
        DEVNULL = "nul"
    elif sys.platform [:6] == "darwin":
        HOST = ["mac"]
        TARGET = ["mac"]
        DEVNULL = "/dev/null"
    else:
        # Default to POSIX
        HOST = ["posix"]
        TARGET = ["posix"]
        DEVNULL = "/dev/null"

    arch = platform.machine ()
    # Python 2.5 on windows returns empty string here
    if (arch == "") or \
       (len (arch) >= 4 and arch [0] == "i" and arch [2:4] == "86" and arch [4:] == ""):
        arch = "x86"

    HOST.append (arch)
    TARGET.append (arch)

    # Earlier code here would automatically tune for host processor.
    # This is not good for package maintainers, since the machine on which
    # packages are built aren't related in any way to machines, on which
    # users will run the binary packages. So we'll stick to generic tuning
    # by default, and autodetect if user requires "auto" tuning.
    TARGET.append (platform.machine ())

    # HOST   contains ["platform", "arch"]
    # TARGET contains ["platform", "arch", "tune"]


# Set the target platform.arch for compilation
def set_target (t):
    global TARGET
    t = t.split ('.')
    if t [0] != "":
        TARGET [0] = t [0]
    if len (t) > 1 and t [1] != "":
        TARGET [1] = t [1]
    if len (t) > 2 and t [2] != "":
        TARGET [2] = t [2]


# Parse a boolean value
def parse_bool (o, t):
    global STATICLIBS;
    t = t.lower ();
    if t == "yes" or t == "on" or t == "enable" or t == "true" or t == "":
        return True;
    elif t == "no" or t == "off" or t == "disable" or t == "false":
        return False;
    print ("Invalid argument to --%s given: `%s'" % (o, t));
    print ("Must be one of (yes|on|enable|true) or (no|off|disable|false)");
    sys.exit (1)

# Build and print the help/usage text
def help ():
    print ("""
This script configures this project to build on your operating system.
The following switches are available:
""")

    lines = []
    maxlen = 0
    for o in OPTIONS:
        if len (o) < 5 or not o [4]:
            continue

        left = "    "
        if o [0]:
            left = left + "-" + o [0] + " "
        else:
            left = left + "   "

        if o [1]:
            left = left + "--" + o [1]
            if o [2]:
                left = left + "="
            else:
                left = left + " "

        if o [2]:
            left = left + o [2] + " "

        lines.append ([left, o [4]])
        if len (left) > maxlen:
            maxlen = len (left)

    for o in lines:
        print (o [0].ljust (maxlen) + o [1].replace ("\n", "\n" + "".ljust (maxlen)))

    print ("""
The following environment variables are automatically picked up:
""")
    for o in ENVARS:
        if len (o) > 2 and o [2]:
            left = ("    " + o [0]).ljust (maxlen)
            desc = o [2]
            if o [1]:
                desc = desc.replace ("@@@", o [1])
            print (left + desc.replace ("\n", "\n" + "".ljust (maxlen)))

    sys.exit (1)


def make_identifier (s):
    return s.replace ("+", "").replace (".", "").replace ("-", "_").upper ()


def remove (filename):
    try:
        os.remove (filename)
    except:
        pass


def add_config_h (macro, val = "1"):
    """Add a macro definition to config.h file. The contents of config.h is
    accumulated during configure run in the tibs.CONFIG_H variable and finally
    is written out with a tibs.update_file() call.

    :Parameters:
        `macro` : str
            The name of the macro to define. Usually upper-case.
        `val` : str
            The value of the macro. If not specified, defaults to "1".
    """
    global CONFIG_H, _CONFIG_H
    macro = macro.strip ()
    CONFIG_H [macro] = val.strip ()
    _CONFIG_H.append (macro)


def add_config_mak (macro, val = "1"):
    """Add a macro definition to config.mak file. The contents of config.mak is
    accumulated during configure run in the tibs.CONFIG_MAK variable and finally
    is written out with a tibs.update_file() call.

    :Parameters:
        `macro` : str
            The name of the macro to define. Usually upper-case.
        `val` : str
            The value of the macro. If not specified, defaults to "1".
    """
    global CONFIG_MAK, _CONFIG_MAK
    macro = macro.strip ()
    CONFIG_MAK [macro] = val.strip ()
    _CONFIG_MAK.append (macro)


def get_config_h (header = None):
    v = header or \
"""/* This file has been automatically generated: do not modify
   as it will be overwritten next time you run configure! */

"""
    global CONFIG_H, _CONFIG_H
    for x in _CONFIG_H:
        v = v + "#define %s %s\n" % (x, CONFIG_H [x])
    return v


def get_config_mak (header = None):
    v = header or \
"""# This file has been automatically generated: do not modify
# as it will be overwritten next time you run configure!

"""
    global CONFIG_MAK, _CONFIG_MAK
    for x in _CONFIG_MAK:
        v = v + "%s=%s\n" % (x, CONFIG_MAK [x])
    return v


def check_started (text):
    sys.stdout.write (text.ljust (64) + " ...")


def check_finished (res):
    print (res)


def check_compile (srcf, outf, cflags = None):
    """Check if some file will compile with a exit code of 0.

    :Parameters:
        `srcf` : str
            The source file name
        `outf` : str
            The output file name
        `cflags` : str
            Optional additional compiler flags
    """
    cflags = cflags or ""

    cmd = TOOLKIT.c_compile (srcf, outf, cflags)
    suffix = " >" + DEVNULL + " 2>&1"
    if VERBOSE:
        suffix = ""
        print ("\n#", cmd)

    fd = os.popen (cmd + suffix)
    return fd.close () == None


def check_compile_and_link (srcf, outf, cflags = None, libs = None):
    """Check if some file will compile and link with a exit code of 0.

    :Parameters:
        `srcf` : str
            The source file name
        `outf` : str
            The output file name
        `cflags` : str
            Optional additional compiler flags
        `libs` : str
            Optional additional libraries to link with
    """
    cflags = cflags or ""
    libs = libs or ""

    cmd = TOOLKIT.comlink (srcf, outf, cflags, libs)
    suffix = " >" + DEVNULL + " 2>&1"
    if VERBOSE:
        suffix = ""
        print ("\n#", cmd)

    fd = os.popen (cmd + suffix)
    return fd.close () == None


def read_file (filename):
    """Read a whole file into a variable.

    :Parameters:
        `filename` : str
            The name of the file to read from
    :Returns: the contents of the whole file in a string form.
    """
    try:
        fd = open (filename, "r")
        content = fd.read ()
        fd.close ()
        return content
    except IOError:
        return None


def write_file (filename, content):
    """Write a string variable into a file.

    :Parameters:
        `filename` : str
            The name of the file to write to
        `content` : str
            The content of the file
    """
    fd = open (filename, "w")
    fd.write (content)
    fd.close ()


def update_file (filename, content):
    """Compare the content of a file with a variable, and if they differ,
    write out the variable into the file. This can be used to update
    a existing file with new content without unneedingly changing
    timestamps if nothing changes.

    :Parameters:
        `filename` : str
            The name of the file to write to
        `content` : str
            The new content to write to file
    :Returns: True if content has changed, False if not
    """
    old_content = read_file (filename)

    if content != old_content:
        print ("Updating file: " + filename);
        write_file (filename, content)
        rc = True
    else:
        print ("Not changed: " + filename);
        rc = False
    return rc


def check_cflags (cflags, var, xcflags = ""):
    """Check if compiler supports certain flags. This is done by
    creating a dummy source file and trying to compile it using
    the requested flags.

    :Parameters:
        `cflags` : str
            The compiler flags to check for
        `var` : str
            The name of the variable to append the flags to in the case
            if the flags are supported (e.g. "CFLAGS", "CXXFLAGS" etc)
        `xcflags` : str
            Additional flags to use during test compilation. These won't
            be appended to var.
    :Returns: True if the flags are supported, False if not.
    """
    check_started ("Checking if compiler supports " + cflags)
    if var.endswith ("CFLAGS"):
        srcf = "conftest.c"
    else:
        srcf = "conftest.cpp"

    write_file (srcf, """
int main () { }
""");

    if check_compile (srcf, "conftest" + TOOLKIT.OBJ, cflags + " " + xcflags):
        check_finished ("Yes")
        if not var in globals ():
            globals () [var] = "";
        if globals () [var] != "":
            globals () [var] += " "
        globals () [var] += cflags
        rc = True
    else:
        check_finished ("No")
        rc = False
    remove (srcf)
    remove ("conftest" + TOOLKIT.OBJ)
    return rc


def abort_configure (reqtext):
    """Abort configuration process with a error message. The message
    will look like:

    Cannot build the project because ...

    with ... being replaced by a string specified by the user.

    :Parameters:
        `reqtext` : str
            The message explaining why the configure process is aborting.
    """
    print ("Cannot build the project because " + reqtext + "\n")
    sys.exit (1)


def compare_version (version, req_version):
    """Compare if a version number is greater or equal than another.

    :Parameters:
        `version` : str
            The version which is checked
        `req_version` : str
            The minimally required version
    :Returns: True if version fits the minimum requirements.
    """
    v1 = re.split ('[.-]', version)
    v2 = re.split ('[.-]', req_version)
    for i in range (min (len (v1), len (v2))):
        try:
            if long (v1 [i]) < long (v2 [i]):
                return False
        except:
            # Perhaps we have a version tail like .rc1 etc
            if v1 [i] < v2 [i]:
                return False
    return True


def check_lib_msg(lib, reqversion):
    s = "Checking for library " + lib
    if reqversion:
        s = s + " >= " + reqversion
    return s


def check_library (lib, reqversion = None, reqtext = None, version = None, cflags = None, libs = None):
    """Check if certain library is available to the compiler.

    :Parameters:
        `lib` : str
            The user-visible library name e.g. "Glib", "gnome libraries" and so on.
        `reqversion` : str
            If not None, the minimum required version of the library
        `reqtext` : str
            If this is not None in the event if library is not available a fatal error
            message is printed and process aborts.
        `version` : str
            The version of the installed library. Usually passed in when you check
            libraries with the pkgconfig_check_library method (the recommended way).
        `cflags` : str
            The cflags required to compiler a sample program using the library.
        `libs` : str
            The libraries required to link a sample program against the library.
    :Returns: True if library is ok, False if not
    """

    if not libs:
        libs = TOOLKIT.linklib (lib)

    check_started (check_lib_msg (lib, reqversion))

    if reqversion:
        if not compare_version (version, reqversion):
            check_finished ("FAILED")
            print ("\nVersion %s of the library is required, but found version %s" % (reqversion, version))
            if reqtext:
                abort_configure (reqtext)
            return False

    rc = False

    write_file ("conftest.c", "int main () { return 0; }\n");

    if check_compile_and_link ("conftest.c", "conftest" + EXE, cflags, libs):
        if version:
            check_finished (version + ", OK")
        else:
            check_finished ("OK")
        libid = make_identifier (lib)
        add_config_h ("HAVE_" + libid)
        add_config_mak ("HAVE_" + libid, "1")
        if cflags and cflags != "":
            add_config_mak ("CFLAGS." + libid, cflags)
        if libs and libs != "":
            add_config_mak ("LDLIBS." + libid, libs)
        rc = True
    else:
        check_finished ("NOT FOUND")

    remove ("conftest.c")
    remove ("conftest" + TOOLKIT.OBJ)
    remove ("conftest" + EXE)
    if not rc and reqtext:
        print ("\nThis library is required to build %s," % PROJ)
        abort_configure (reqtext)
    return rc


def pkgconfig_check_library (lib, reqversion, reqtext = None):
    """The easy way to check if a library is correctly installed and useable.

    :Parameters:
        `lib` : str
            The library name for pkgconfig
        `reqversion` : str
            Minimally required version of the library
        `reqtext` : str
            The error message to print when the library is not found.
    :Returns: True if library is ok, False if not
    """
    global PROJ

    # First, check package for existence
    if not pkgconfig ("pkg-config", "exists", lib):
        # Print the negative result and return
        check_started (check_lib_msg (lib, reqversion))
        check_finished ("NOT FOUND")

        if reqtext:
            print ("\nThis library is required to build %s," % PROJ)
            abort_configure (reqtext)

        return False

    if reqversion:
        version = pkgconfig ("pkg-config", "modversion", lib)
    else:
        version = None

    cflags = pkgconfig ("pkg-config", "cflags", lib)
    libs = pkgconfig ("pkg-config", "libs", lib)

    return check_library (lib, reqversion, reqtext, version, cflags, libs)


def check_header (hdr, cflags = None, reqtext = None):
    """Check if a header file is available to the compiler. This is done
    by creating a dummy source file and trying to compile it.

    :Parameters:
        `hdr` : str
            The name of the header file to check for
        `reqtext` : str
            The message to print in the fatal error message when the header
            file is not available. If it is None, the missing file is not
            considered a fatal error.
    :Returns: True if header file exists, False if not.
    """
    rc = False
    check_started ("Checking for header file " + hdr)
    write_file ("conftest.c", """
#include <%s>
""" % hdr);

    if check_compile ("conftest.c", "conftest" + TOOLKIT.OBJ, cflags):
        check_finished ("OK")
        add_config_h ("HAVE_" + make_identifier (hdr.replace (".", "_")))
        rc = True
    else:
        check_finished ("NOT FOUND")

    remove ("conftest.c")
    remove ("conftest" + TOOLKIT.OBJ)
    if not rc and reqtext:
        print ("\nThis header file is required to build %s," % PROJ)
        abort_configure (reqtext)
    return rc


def check_program (name, prog, ver_regex, req_version, failifnot = False):
    """Check if some program is present, and if its version number is
    sufficient.

    :Parameters:
        `name` : str
            End-user program name (e.g. "make", "doxygen" etc).
        `prog` : str
            The command-line to run when checking for the program.
            This includes any switches required to display version number
            e.g. "make --version" and so on.
        `var_regex` : str
            The regular expression to select program version from the captured
            output line. This could be something like ".*?([0-9\.]+).*".
            The regex must match the whole line.
        `req_version` : str
            The minimal required version to fit
        `failifnot` : bool
            If True, tibs will print a error message and stop the configuration
            process. Usually this means that the specified program is not optional
            and it is impossible to build the project without it.
    """

    check_started ("Checking for " + name + " >= " + req_version)
    rc = False
    version = None
    try:
        fd = os.popen (prog + " 2>&1")
        lines = fd.readlines ()
        fd.close ()

        for line in lines:
            line = line.strip ()

            if VERBOSE:
                print ("\n# '" + prog + "' returned '" + line + "'")

            m = re.match (ver_regex, line)
            if m:
                version = m.group (1)
                if not compare_version (version, req_version):
                    raise
                break

        if not m:
            raise

        check_finished (version + ", OK")
        rc = True
    except:
        if version:
            check_finished (version + " < " + req_version + ", FAILED")
        else:
            check_finished ("FAILED")

    if not rc and failifnot:
        print ("\n" + name + " version " + req_version + " and above is required to build this project")
        sys.exit (1)

    return rc


def pkgconfig (prog, args, lib = None):
    # Check if target overrides this library
    tmp_lib = lib
    if not tmp_lib:
        tmp_lib = prog.replace ("_config", "").replace ("-config", "")

    # Allow user to override any pkgconfig data with environment
    env_var = "PKGCONFIG_" + tmp_lib + "_" + args
    env_val = os.getenv (env_var, None)
    if env_val:
        if args == "exists":
            return True
        return env_val

    # Check if there's a static library configuration defined
    if tmp_lib in PKGCONFIG:
        if args == "exists":
            return True
        pkg = PKGCONFIG [tmp_lib]
        if args in pkg:
            return pkg [args]
        return ""

    # Do not use pkg-config if cross-compiling
    if HOST [:1] != TARGET [:1]:
        abort_configure (
"""
PKGCONFIG does not define the compiler flags
for %s and we cannot use pkg-config because we are cross-compiling.
""" % tmp_lib)

    cmd = prog;
    if lib:
        cmd = cmd + " " + lib
    suffix = " 2>" + DEVNULL
    cmd = cmd + " --" + args
    if VERBOSE:
        suffix = ""
    fd = os.popen (cmd + suffix)
    line = " ".join (fd.readlines ()).strip ()
    if VERBOSE:
        print ("# '" + cmd + "' returned '" + line + "'")
    if fd.close () == None:
        if args == "exists":
            return True
        return line

    return ""

def substmacros (infile, outfile = None, macros = None):
    """Substitute macros like @NAME@ in given file and write the result
    with substitutions to another file.

    The macros are either taken from globals(), or are passed as argument.

    :Parameters:
        `infile` : str
            The input file name
        `outfile` : str
            The output file name, if None tibs expects infile to end with
            '.in' in which case the suffix is removed and the result is
            used as outfile.
        `macros` : dict
            A dictionary of macros names to replace, if None the normal
            tibs variable names will be used (like CONF_PREFIX and so on).
    :Returns: True if file content has changed, False if not
    """
    if not outfile:
        if infile.endswith (".in"):
            outfile = infile [:-3]
        else:
            abort_configure (
"""substitution failed:
Cannot transform input file name `%s' into an ouput file name.
It either has to end in  `.in' or output file name must be given explicitly.
""" % (infile, infile))
    if not macros:
        macros = globals ()

    content = read_file (infile)
    if not content:
        abort_configure (
"""the file `%s' does not exist or is empty.
It is expected that it contains a template.
""" % infile)

    rx = re.compile ("@([a-zA-Z0-9_]*)@")
    sc = rx.split (content)
    for x in range (1, len (sc), 2):
        if sc [x] in macros:
            sc [x] = macros [sc [x]]
        else:
            abort_configure (
"""substitution failed:
Macro @%s@ is used in `%s' but its value is undefined.
""" % (sc [x], infile))

    content = "".join (sc)

    return update_file (outfile, content)
