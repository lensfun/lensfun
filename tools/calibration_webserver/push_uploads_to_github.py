#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""This script is supposed to run as a cronjob, e.g. once per hour.
"""

import os, re, configparser
from github import Github


config = configparser.ConfigParser()
config.read(os.path.expanduser("~/calibration_webserver.ini"))

root = config["General"]["uploads_root"]
github = Github(config["GitHub"]["login"], config["GitHub"]["password"])
lensfun = github.get_organization("lensfun").get_repo("lensfun")
calibration_request_label = lensfun.get_label("calibration request")


title_regex = re.compile("Calibration upload ([0-9a-f]{6})$")
issue_ids = set()
for issue in lensfun.get_issues(state="", labels=[calibration_request_label]):
    match = title_regex.match(issue.title)
    if match:
        issue_ids.add(match.group(1))


id_regex = re.compile("([0-9a-f]{6})_.*$")
for directory in os.listdir(root):
    match = id_regex.match(directory)
    if match:
        id_ = match.group(1)
        if id_ not in issue_ids:
            body = "Calibration images were uploaded to the directory that starts with “`{0}_`”.\n\n" \
                   "Please read the [workflow description]" \
                   "(https://github.com/lensfun/lensfun/blob/master/tools/calibration-webserver/workflow.rst) for further " \
                   "instructions about the calibration.\n".format(id_)
            lensfun.create_issue("Calibration upload {}".format(id_), body=body, labels=[calibration_request_label])
