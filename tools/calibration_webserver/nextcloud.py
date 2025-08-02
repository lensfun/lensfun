"""Interaction with the Nextcloud synchronisation program ``nextcloudcmd``.  It
works only on UNIX.
"""

import os, fcntl, subprocess, time, configparser


config = configparser.ConfigParser()
config.read(os.path.expanduser("~/calibration_webserver.ini"))


class NextcloudLock:
    """Lock on the Nextcloud synchronisation program as a context manager.  It
    is adapted from very similar code from the JuliaBase project.  You can use
    this class like this::

        with NextcloudLock() as locked:
            if locked:
                do_work()
            else:
                print "I could not acquire the lock.  I just exit."

    The lock file is /tmp/nextcloudcmd.pid.
    """

    def __init__(self):
        self.lockfile_path = os.path.join("/tmp/nextcloudcmd.pid")
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
    """Raised if the Nextcloud lock could not be acquired.
    """
    pass


def sync():
    """Syncs the local Nextcloud directory with the Nextcloud server.  It
    prevents two synchronisations being performed at the same time.

    :raises LockError: if the lock could not be acquired even after retrying
      every minute for 6 hours.
    """
    cycles_left = 60 * 6
    while cycles_left:
        with NextcloudLock() as locked:
            if locked:
                subprocess.run(["nextcloudcmd", "--non-interactive", "-h", "--user", config["Nextcloud"]["login"],
                                "--password", config["Nextcloud"]["password"], config["Nextcloud"]["local_root"],
                                config["Nextcloud"]["server_url"]], check=True)
                return
        cycles_left -= 1
        time.sleep(60)
    raise LockError
