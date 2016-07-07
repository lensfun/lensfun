#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""This script is supposed to run as a cronjob, e.g. once per hour.
"""

import os, re
from github import Github


root = "/path/two/upload/directory"
github = Github("username", "password")
lensfun = github.get_organization("lensfun").get_repo("lensfun")
calibration_request_label = lensfun.get_label("calibration request")


title_regex = re.compile("Calibration upload ([0-9a-f]{7})$")
issue_ids = set()
for issue in lensfun.get_issues(state="", labels=[calibration_request_label]):
    match = title_regex.match(issue.title)
    if match:
        issue_ids.add(match.group(1))

id_regex = re.compile(".*_([0-9a-f]{7})$")
for directory in os.listdir(root):
    match = id_regex.match(directory)
    if match:
        id_ = match.group(1)
        if id_ not in issue_ids:
            body = "Calibration images were uploaded to the directory that starts with `{0}_`.\n\n" \
                   "If you want to work on it, please self-assign this issue first.  When you are finished, copy your " \
                   "final version of the images directory with a describing name into the top-level directory and remove " \
                   "the original directory `{0}_*`.  Then, close this issue.\n\n" \
                   "Thank you!\n".format(id_)
            lensfun.create_issue("Calibration upload {}".format(id_),body=body, labels=[calibration_request_label])
