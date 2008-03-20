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
    "start", "check_program", "add_config_h", "add_config_mak",
    "check_program", "check_compile", "check_compile_and_link",
    "check_cflags", "check_header",
    "check_library", "pkgconfig_check_library",
    "update_file", "write_to_file",
    "abort_configure"
]

# Default values for user-definable variables

# Project name
PROJ = "Unknown"
# Version number (major.minor.release)
VERSION = "0.0.0"

PREFIX = "/usr/local"
BINDIR = None
DATADIR = None
SYSCONFDIR = None
LIBDIR = None
LIBEXECDIR = None
DOCSDIR = None
INCLUDEDIR = None
VERBOSE = 0
MODE = "release"
SHAREDLIBS = True

COMPILER = os.getenv ("COMPILER", "gcc")
CFLAGS = os.getenv ("CFLAGS", "")
CXXFLAGS = os.getenv ("CXXFLAGS", "")
LDFLAGS = os.getenv ("LDFLAGS", "")
LIBS = os.getenv ("LIBS", "")

CONFIG_H   = ["/* This file has been automatically generated: do not modify",
              "   as it will be overwritten next time you run configure! */",
              ""]
CONFIG_MAK = ["# This file has been automatically generated: do not modify",
              "# as it will be overwritten next time you run configure!",
              ""]

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
    [ "v",  "verbose",    None,    "global VERBOSE; VERBOSE = 1",
      "Display verbosely the detection process" ],
    [ None, "mode",       "MODE",  "global MODE; MODE = optarg",
      "Use given compilation mode by default (debug|release)" ],
    [ None, "compiler",   "COMP",  "global COMPILER; COMPILER = optarg",
      "Use the respective compiler (supported: gcc|msvc)" ],
    [ None, "cflags",     "FLAGS", "global CFLAGS; CFLAGS = optarg",
      "Additional compiler flags for C" ],
    [ None, "cxxflags",   "FLAGS", "global CXXFLAGS; CXXFLAGS = optarg",
      "Additional compiler flags for C++" ],
    [ None, "ldflags",    "FLAGS", "global LDFLAGS; LDFLAGS = optarg",
      "Additional linker flags" ],
    [ None, "libs",       "LIBS", "global LIBS; LIBS = optarg",
      "Additional libraries to link with" ],
    [ None, "target",     "PLATFORM", "set_target (optarg)",
      "Target platform{.arch} (posix|windows|mac){.(x86|x86_64)}" ],
    [ None, "staticlibs", "YESNO", "global SHAREDLIBS; SHAREDLIBS = not parse_bool ('staticlibs', optarg)",
      "Build all libs as static, even if they are shared by default" ],
]

# Environment variables picked up: Name, Default value, Description
ENVARS = [
    [ "COMPILER", "gcc", "The compiler suite to use (default: @@@).\n"
                         "NOTE: a file named build/mak/$COMPILER.mak must exist!" ],
    [ "CFLAGS",   "",    "Additional compilation flags for C" ],
    [ "CXXFLAGS", "",    "Additional compilation flags for C++" ],
    [ "LDFLAGS",  "",    "Additional linker flags" ],
    [ "LIBS",     "",    "Additional libraries to link with" ],
    [ "CC",       None,  "Override the C compiler name/path" ],
    [ "CXX",      None,  "Override the C++ compiler name/path" ],
    [ "LD",       None,  "Override the linker name/path" ]
]

# -------------- # Abstract interface to different compilers # --------------

class compiler_gcc:
    def __init__ (self):
        # Find our compiler and linker
        self.CC = CC or "gcc"
        self.CXX = CXX or "g++"
        self.LD = LD or "g++"
        self.OBJ = ".o"

    def startup (self):
        global CXXFLAGS

        # Always use -fvisibility, if available, for shared libs
        # as suggested by gcc docs and mark exported symbols explicitly.
        if check_cflags ("-fvisibility=hidden", "CXXFLAGS", "-Werror") or \
           check_cflags ("-fvisibility-inlines-hidden", "CXXFLAGS", "-Werror"):
            add_config_h ("CONF_SYMBOL_VISIBILITY")

        check_cflags ("-Wno-non-virtual-dtor", "CXXFLAGS", "-Werror")
        add_config_mak ("GCC.CC", self.CC + " -c")
        add_config_mak ("GCC.CXX", self.CXX + " -c")
        add_config_mak ("GCC.LD", self.LD)

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
        self.LD = LD or  "link.exe"
        self.OBJ = ".obj"

    def startup (self):
        add_config_mak ("MSVC.CC", self.CC + " -c")
        add_config_mak ("MSVC.CXX", self.CXX + " -c")
        add_config_mak ("MSVC.LD", self.LD)

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
            tmp = "-libpath:" + path + " "
        return tmp + ".lib ".join (library.split ()) + ".lib"


# A list of supported compilers
COMPILERS = {
    "gcc":  lambda: compiler_gcc (),
    "msvc": lambda: compiler_msvc (),
}

#-----------------------------------------------------------------------------#

# Common initialization
def start ():
    global HOST, TARGET, DEVNULL, EXE, TOOLKIT
    global PREFIX, BINDIR, SYSCONFDIR, DATADIR, DOCSDIR, LIBDIR
    global INCLUDEDIR, LIBEXECDIR, SHAREDLIBS
    if os.name == "posix":
        HOST = ["posix"]
        TARGET = ["posix"]
        DEVNULL = "/dev/null"
    elif os.name == "nt":
        HOST = ["windows"]
        TARGET = ["windows"]
        DEVNULL = "nul"
    elif os.name == "mac":
        HOST = ["mac"]
        TARGET = ["mac"]
        DEVNULL = "/dev/null"
    else:
        print "Unknown operating system: " + os.name
        sys.exit (1)

    arch = platform.machine ()
    if arch [0] == "i" and arch [2:4] == "86" and arch [4:] == "":
        arch = "x86"

    HOST.append (arch)
    TARGET.append (arch)

    # Read environment variables first
    for e in ENVARS:
        command = "global " + e [0] + "; " + e [0] + " = os.getenv ('" + e [0] + "'"
        if len (e) > 1 and e [1] != None:
            command = command + ", '" + e [1] + "'"
        command = command + ")"
        exec command

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
            print "Unknown command-line option: '" + opt + "'"
            sys.exit (1)

        # Check if option needs argument
        if o [2] and opt_short:
            if optidx >= len (sys.argv):
                print "Option '" + opt + "' needs an argument"
                sys.exit (1)

            skip_opt = True
            optarg = sys.argv [optidx + 1]

        if not o [2] and optarg:
            print "Option '" + opt + "' does not accept an argument"
            sys.exit (1)

        exec o [3]

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

    add_config_mak ("HOST", HOST [0])
    add_config_mak ("TARGET", TARGET [0])
    add_config_mak ("ARCH", TARGET [1])

    if not BINDIR:
        BINDIR = PREFIX + "/bin"
    if not SYSCONFDIR:
        SYSCONFDIR = PREFIX + "/etc/" + PROJ
    if not DATADIR:
        DATADIR = PREFIX + "/share/" + PROJ
    if not DOCSDIR:
        DOCSDIR = PREFIX + "/share/doc/" + PROJ + "-" + VERSION
    if not LIBDIR:
        if arch [-2:] == "64":
            LIBDIR = PREFIX + "/lib64"
        else:
            LIBDIR = PREFIX + "/lib"
    if not INCLUDEDIR:
        INCLUDEDIR = PREFIX + "/include"
    if not LIBEXECDIR:
        LIBEXECDIR = PREFIX + "/libexec/" + PROJ

    add_config_h ("CONF_COMPILER_" + COMPILER)
    add_config_h ("CONF_PREFIX", '"' + PREFIX + '"')
    add_config_mak ("CONF_PREFIX", PREFIX + '/')
    add_config_h ("CONF_BINDIR", '"' + BINDIR + '"')
    add_config_mak ("CONF_BINDIR", BINDIR + '/')
    add_config_h ("CONF_SYSCONFDIR", '"' + SYSCONFDIR + '"')
    add_config_mak ("CONF_SYSCONFDIR", SYSCONFDIR + '/')
    add_config_h ("CONF_DATADIR", '"' + DATADIR + '"')
    add_config_mak ("CONF_DATADIR", DATADIR + '/')
    add_config_h ("CONF_LIBDIR", '"' + LIBDIR + '"')
    add_config_mak ("CONF_LIBDIR", LIBDIR + '/')
    add_config_h ("CONF_INCLUDEDIR", '"' + INCLUDEDIR + '"')
    add_config_mak ("CONF_INCLUDEDIR", INCLUDEDIR + '/')
    add_config_h ("CONF_DOCSDIR", '"' + DOCSDIR + '"')
    add_config_mak ("CONF_DOCSDIR", DOCSDIR + '/')
    add_config_h ("CONF_LIBEXECDIR", '"' + LIBEXECDIR + '"')
    add_config_mak ("CONF_LIBEXECDIR", LIBEXECDIR + '/')
    if SHAREDLIBS:
        add_config_h ("CONF_SHAREDLIBS", "1")
        add_config_mak ("SHAREDLIBS", "1")
    else:
        add_config_h ("CONF_SHAREDLIBS", "0")
        add_config_mak ("SHAREDLIBS", "")

    # Instantiate the compiler-dependent class
    TOOLKIT = COMPILERS.get (COMPILER);
    if not TOOLKIT:
        print "Unsupported compiler: " + COMPILER
        sys.exit (1)
    TOOLKIT = TOOLKIT ()
    TOOLKIT.startup ()


# Set the target platform.arch for compilation
def set_target (t):
    global TARGET
    t = t.split ('.')
    TARGET [0] = t [0]
    if len (t) > 1:
        TARGET [1] = t [1]


# Parse a boolean value
def parse_bool (o, t):
    global STATICLIBS;
    t = t.lower ();
    if t == "yes" or t == "on" or t == "enable" or t == "true" or t == "":
        return True;
    elif t == "no" or t == "off" or t == "disable" or t == "false":
        return False;
    print "Invalid argument to --%s given: `%s'" % (o, t);
    print "Must be one of (yes|on|enable|true) or (no|off|disable|false)";
    sys.exit (1)

# Build and print the help/usage text
def help ():
    print """
This script configures this project to build on your operating system.
The following switches are available:
"""

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
        print o [0].ljust (maxlen) + o [1].replace ("\n", "\n" + "".ljust (maxlen))

    print """
The following environment variables are automatically picked up:
"""
    for o in ENVARS:
        if len (o) > 2 and o [2]:
            left = ("    " + o [0]).ljust (maxlen)
            desc = o [2]
            if o [1]:
                desc = desc.replace ("@@@", o [1])
            print left + desc.replace ("\n", "\n" + "".ljust (maxlen))

    sys.exit (1)


def make_identifier (s):
    return s.replace ("+", "").replace (".", "").replace ("-", "_").upper ()


def remove (filename):
    try:
        os.remove (filename)
    except:
        pass


def add_config_h (macro, val = "1"):
    global CONFIG_H
    CONFIG_H.append ("#define %s %s" % (macro.strip (), val.strip ()))


def add_config_mak (macro, val = "1"):
    global CONFIG_MAK
    CONFIG_MAK.append ("%s=%s" % (macro.strip (), val.strip ()))


def check_started (text):
    print text.ljust (42), "...",


def check_finished (res):
    print res


def check_compile (srcf, outf, cflags = None):
    cflags = cflags or ""

    cmd = TOOLKIT.c_compile (srcf, outf, cflags)
    suffix = " >" + DEVNULL + " 2>&1"
    if VERBOSE > 0:
        suffix = ""
        print "\n#", cmd

    fd = os.popen (cmd + suffix)
    return fd.close () == None


def check_compile_and_link (srcf, outf, cflags = None, libs = None):
    cflags = cflags or ""
    libs = libs or ""

    cmd = TOOLKIT.comlink (srcf, outf, cflags, libs)
    suffix = " >" + DEVNULL + " 2>&1"
    if VERBOSE > 0:
        suffix = ""
        print "\n#", cmd

    fd = os.popen (cmd + suffix)
    return fd.close () == None


def write_to_file (filename, content):
    fd = open (filename, "w")
    fd.write (content)
    fd.close ()


def check_cflags (cflags, var, xcflags = ""):
    check_started ("Checking if compiler supports " + cflags)
    if var == "CFLAGS":
        srcf = "conftest.c"
    else:
        srcf = "conftest.cpp"

    write_to_file (srcf, """
int main () { }
""");

    if check_compile (srcf, "conftest" + TOOLKIT.OBJ, cflags + " " + xcflags):
        check_finished ("Yes")
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
    print "Cannot build the project because " + reqtext + "\n"
    sys.exit (1)


def compare_version (version, req_version):
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
    if not libs:
        libs = TOOLKIT.linklib (lib)

    check_started (check_lib_msg (lib, reqversion))

    if reqversion:
        if not compare_version (version, reqversion):
            check_finished ("FAILED")
            print "\nVersion %s of the library is required, but found version %s" % (reqversion, version)
            if reqtext:
                abort_configure (reqtext)
            return False

    rc = False

    write_to_file ("conftest.c", "int main () { return 0; }\n");

    if check_compile_and_link ("conftest.c", "conftest" + EXE, cflags, libs):
        if version:
            check_finished (version + ", OK")
        else:
            check_finished ("OK")
        libid = make_identifier (lib)
        add_config_h ("HAVE_LIB" + libid)
        if cflags and cflags != "":
            add_config_mak ("CFLAGS." + libid, cflags)
            add_config_mak ("CXXFLAGS." + libid, cflags)
        if libs and libs != "":
            add_config_mak ("LDLIBS." + libid, libs)
        rc = True
    else:
        check_finished ("NOT FOUND")

    remove ("conftest.c")
    remove ("conftest" + TOOLKIT.OBJ)
    remove ("conftest" + EXE)
    if not rc and reqtext:
        print "\nThis library is required to build %s," % PROJ
        abort_configure (reqtext)
    return rc


def pkgconfig_check_library (lib, reqversion, reqtext = None):
    global PROJ

    # First, check package for existence
    if not pkgconfig ("pkg-config", "exists", lib):
        # Print the negative result and return
        check_started (check_lib_msg (lib, reqversion))
        check_finished ("NOT FOUND")

        if reqtext:
            print "\nThis library is required to build %s," % PROJ
            abort_configure (reqtext)

        return False

    if reqversion:
        version = pkgconfig ("pkg-config", "modversion", lib)
    else:
        version = None

    cflags = pkgconfig ("pkg-config", "cflags", lib)
    libs = pkgconfig ("pkg-config", "libs", lib)

    return check_library (lib, reqversion, reqtext, version, cflags, libs)


def check_header (hdr, reqtext = None):
    rc = False
    check_started ("Checking for header file " + hdr)
    write_to_file ("conftest.c", """
#include <%s>
""" % hdr);

    if check_compile ("conftest.c", "conftest" + TOOLKIT.OBJ):
        check_finished ("OK")
        add_config_h ("HAVE_" + make_identifier (hdr))
        rc = True
    else:
        check_finished ("NOT FOUND")

    remove ("conftest.c")
    remove ("conftest" + TOOLKIT.OBJ)
    if not rc and reqtext:
        print "\nThis header file is required to build %s," % PROJ
        abort_configure (reqtext)
    return rc


def check_program (name, prog, ver_regex, req_version, failifnot = False):
    check_started ("Checking for " + name + " >= " + req_version)
    rc = False
    version = None
    try:
        fd = os.popen (prog + " 2>&1")
        line = fd.readline ().strip ()
        fd.close ()

        if VERBOSE > 0:
            print "\n# '" + prog + "' returned '" + line + "'"

        m = re.match (ver_regex, line)
        if not m:
            raise

        version = m.group (1)
        if not compare_version (version, req_version):
            raise

        check_finished (version + ", OK")
        rc = True
    except:
        if version:
            check_finished (version + " < " + req_version + ", FAILED")
        else:
            check_finished ("FAILED")

    if not rc and failifnot:
        print "\n" + name + " version " + req_version + " and above is required to build this project"
        sys.exit (1)

    return rc


def update_file (filename, content):
    try:
        fd = open (filename, "r")
        old_content = fd.read ()
        fd.close ()
    except:
        old_content = None

    if content != old_content:
        print ("Updating file: " + filename);
        write_to_file (filename, content)
    else:
        print ("Not changed: " + filename);
    pass


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
    if HOST != TARGET:
        abort_configure ("PKGCONFIG does not define the compiler flags" + "\n" +
                         "for " + tmp_lib + " and we cannot use pkg-config because we are cross-compiling.")

    cmd = prog;
    if lib:
        cmd = cmd + " " + lib
    suffix = " 2>" + DEVNULL
    cmd = cmd + " --" + args
    if VERBOSE > 0:
        suffix = ""
    fd = os.popen (cmd + suffix)
    line = " ".join (fd.readlines ()).strip ()
    if VERBOSE > 0:
        print "# '" + cmd + "' returned '" + line + "'"
    if fd.close () == None:
        if args == "exists":
            return True
        return line

    return ""
