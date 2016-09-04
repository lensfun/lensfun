#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""Interaction with the ownCloud synchronisation program ``owncloudcmd``.  It
works only on UNIX.
"""

import os, fcntl, subprocess, time, configparser


config = configparser.ConfigParser()
config.read(os.path.expanduser("~/calibration_webserver.ini"))


class OwncloudLock:
    """Lock on the ownCloud synchronisation program as a context manager.  It is
    adapted from very similar code from the JuliaBase project.  You can use
    this class like this::

        with OwncloudLock() as locked:
            if locked:
                do_work()
            else:
                print "I could not acquire the lock.  I just exit."

    The lock file is /tmp/owncloudcmd.pid.
    """

    def __init__(self):
        self.lockfile_path = os.path.join("/tmp/owncloudcmd.pid")
        self.locked = False

    def __enter__(self):
        try:
            self.lockfile = open(self.lockfile_path, "r+")
            fcntl.flock(self.lockfile.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            pid = int(self.lockfile.read().strip())
        except IOError as e:
            if e.strerror == "No such file or directory":
                self.lockfile = open(self.lockfile_path, "w")
                fcntl.flock(self.lockfile.fileno(), fcntl.LOCK_EX)
                already_running = False
            elif e.strerror == "Resource temporarily unavailable":
                already_running = True
            else:
                raise
        except ValueError:
            # Ignore invalid lock
            already_running = False
            self.lockfile.seek(0)
            self.lockfile.truncate()
        else:
            try:
                os.kill(pid, 0)
            except OSError as error:
                if error.strerror == "No such process":
                    # Ignore invalid lock
                    already_running = False
                    self.lockfile.seek(0)
                    self.lockfile.truncate()
                else:
                    raise
            else:
                # sister process is already active
                already_running = True
        if not already_running:
            self.lockfile.write(str(os.getpid()))
            self.lockfile.flush()
            self.locked = True
        return self.locked

    def __exit__(self, type_, value, tb):
        if self.locked:
            fcntl.flock(self.lockfile.fileno(), fcntl.LOCK_UN)
            self.lockfile.close()
            os.remove(self.lockfile_path)


class LockError(Exception):
    """Raised if the ownCloud lock could not be acquired.
    """
    pass


def sync():
    """Syncs the local ownCloud directory with the ownCloud server.  It prevents
    two synchronisations being performed at the same time.

    :raises LockError: if the lock could not be acquired even after retrying 60
      times within one hour.
    """
    cycles_left = 60
    while cycles_left:
        with OwncloudLock() as locked:
            if locked:
                subprocess.check_call(["owncloudcmd", "--silent", "--non-interactive", "--user", config["ownCloud"]["login"],
                                       "--password", config["ownCloud"]["password"], config["ownCloud"]["local_root"],
                                       config["ownCloud"]["server_url"]])
                return
        cycles_left -= 1
        time.sleep(60)
    raise LockError
