#!/usr/bin/env python3

"""Consistency checker of calibration requests on GitHub.

Ideally, there is a 1:1 relationship between open calibration request issues on
GitHub and directories in the calibration directory of Lensfun's ownCloud
service.  This script checks whether this is the case, and sends an error mail
to the Lensfun admin if it is not.

It is recommended to run this script once per day.

Note that this script does not synchronise the ownCloud directory.  This is to
avoid a race with follow_db_changes.py, which does this.  It should not be a
big problem, though, because follow_db_changes.py runs frequently enough to
keep the directory up to date.
"""

import re, configparser, os, json, smtplib, datetime
from pathlib import Path
from email.mime.text import MIMEText
import yaml
from github import Github


config = configparser.ConfigParser()
assert config.read(os.path.expanduser("~/calibration_webserver.ini")), os.path.expanduser("~/calibration_webserver.ini")

github = Github(config["GitHub"]["token"])
lensfun = github.get_organization("lensfun").get_repo("lensfun")
admin = "{} <{}>".format(config["General"]["admin_name"], config["General"]["admin_email"])
owncloud_root = Path(config["ownCloud"]["local_root"])/"calibration"


def send_email(to, subject, body):
    """Sends an email using the SMTP configuration given in the INI file.  The
    sender is always the administrator, also as given in the INI file.

    :param to: recipient of the email
    :param subject: subject of the email
    :param body: body text of the email

    :type to: str
    :type subject: str
    :type body: str
    """
    message = MIMEText(body)
    message["Subject"] = subject
    message["From"] = admin
    message["To"] = to
    smtp_connection = smtplib.SMTP(config["SMTP"]["machine"], config["SMTP"]["port"])
    if config["SMTP"].get("TLS", "off").lower() in {"on", "true", "yes"}:
        smtp_connection.starttls()
    if "login" in config["SMTP"]:
        smtp_connection.login(config["SMTP"]["login"], config["SMTP"]["password"])
    smtp_connection.sendmail(admin, [to, config["General"]["admin_email"]], message.as_string())


def collect_hashes(lensfun, owncloud_root):
    calibration_request_label = lensfun.get_label("calibration request")
    title_regex = re.compile(r"Calibration upload ([a-f0-9]{6})$")
    open_issues = {title_regex.match(issue.title).group(1): issue.html_url
                   for issue in lensfun.get_issues(state="open", labels=[calibration_request_label])}
    open_hashes = set(open_issues)
    closed_issues = {title_regex.match(issue.title).group(1): issue.html_url
                     for issue in lensfun.get_issues(state="closed", labels=[calibration_request_label])}
    closed_hashes = set(closed_issues)
    owncloud_directories = {path.name.partition("_")[0]: Path(path.path)
                            for path in os.scandir(owncloud_root) if path.is_dir()}
    owncloud_hashes = set(owncloud_directories)
    return open_issues, open_hashes, closed_issues, closed_hashes, owncloud_directories, owncloud_hashes

def analyse_owncloud(dangling_directories, closed_hashes, owncloud_directories):
    error_hashes = set()
    problem_hashes = set()
    existing_but_closed_hashes = dangling_directories & closed_hashes
    for hash_ in dangling_directories - existing_but_closed_hashes:
        path = owncloud_directories[hash_]
        try:
            if json.load(open(path/"result.json")) != [None, []]:
                problem_hashes.add(hash_)
        except FileNotFoundError:
            error_hashes.add(hash_)
    return error_hashes, problem_hashes, existing_but_closed_hashes

def filter_old_problem_hashes(problem_hashes, owncloud_directories):
    result = set()
    four_weeks = datetime.timedelta(28)
    for hash_ in problem_hashes:
        timestamp_path = owncloud_directories[hash_]/"consistency_check_timestamp.yaml"
        now = datetime.datetime.now()
        try:
            timestamp = yaml.safe_load(open(timestamp_path))
        except FileNotFoundError:
            timestamp = now
            yaml.dump(timestamp, open(timestamp_path, "w"))
        if now - timestamp > four_weeks:
            result.add(hash_)
    return result

open_issues, open_hashes, closed_issues, closed_hashes, owncloud_directories, owncloud_hashes = \
    collect_hashes(lensfun, owncloud_root)

dangling_issues = open_hashes - owncloud_hashes
dangling_directories = owncloud_hashes - open_hashes

error_hashes, problem_hashes, existing_but_closed_hashes = \
    analyse_owncloud(dangling_directories, closed_hashes, owncloud_directories)

unexplicably_dangling_hashes = dangling_directories - error_hashes - problem_hashes - existing_but_closed_hashes

problem_hashes = filter_old_problem_hashes(problem_hashes, owncloud_directories)

if dangling_issues or existing_but_closed_hashes or error_hashes or problem_hashes or unexplicably_dangling_hashes:
    error_message = "There are inconsistencies between ownCloud directories and GitHub issues.\n"
    if dangling_issues:
        error_message += "\nOpen GitHub issues with no directory in ownCloud:\n\n"
        for hash_ in dangling_issues:
            error_message += open_issues[hash_] + "\n"
    if existing_but_closed_hashes:
        error_message += "\nClosed GitHub issues with ownCloud directories:\n\n"
        for hash_ in existing_but_closed_hashes:
            error_message += closed_issues[hash_] + "\n"
    if error_hashes:
        error_message += "\nownCloud directories with internal errors:\n\n"
        for hash_ in error_hashes:
            error_message += str(owncloud_directories[hash_].relative_to(owncloud_root)) + "\n"
    if problem_hashes:
        error_message += "\nownCloud directories with problems that need to be resolved by uploader\n" \
            "for longer than four weeks:\n\n"
        for hash_ in problem_hashes:
            error_message += str(owncloud_directories[hash_].relative_to(owncloud_root)) + "\n"
    if unexplicably_dangling_hashes:
        error_message += "\nownCloud directories without GitHub issue for unknown reason:\n\n"
        for hash_ in unexplicably_dangling_hashes:
            error_message += str(owncloud_directories[hash_].relative_to(owncloud_root)) + "\n"
    send_email(admin, "Lensfun: Inconsistencies between ownCloud and GitHub", error_message)
