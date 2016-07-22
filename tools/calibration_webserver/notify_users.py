#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""One-off script to notify uploaders of currently open calibration issues.
Such uploaders haven't yet received any email, they are waiting, and possibly
angry."""


import json, smtplib, configparser, os
from email.mime.text import MIMEText
from github import Github


config = configparser.ConfigParser()
config.read(os.path.expanduser("~/calibration_webserver.ini"))

admin = "{} <{}>".format(config["General"]["admin_name"], config["General"]["admin_email"])
github = Github(config["GitHub"]["login"], config["GitHub"]["password"])
lensfun = github.get_organization("lensfun").get_repo("lensfun")
calibration_request_label = lensfun.get_label("calibration request")


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
    message = MIMEText(body, _charset = "utf-8")
    message["Subject"] = subject
    message["From"] = admin
    message["To"] = to
    smtp_connection = smtplib.SMTP(config["SMTP"]["machine"], config["SMTP"]["port"])
    smtp_connection.starttls()
    smtp_connection.login(config["SMTP"]["login"], config["SMTP"]["password"])
    smtp_connection.sendmail(admin, [to, config["General"]["admin_email"]], message.as_string())


class UploadDirectoryNotFound(Exception):
    def __init__(self):
        super().__init__("The upload directory (with the uploader's email address) could not be found.")


def get_upload_data(upload_hash):
    uploads_root = config["General"]["uploads_root"]
    for directory in os.listdir(uploads_root):
        if directory.partition("_")[0] == upload_hash:
            path = os.path.join(uploads_root, directory)
            return path, json.load(open(os.path.join(path, "originator.json")))
    else:
        raise UploadDirectoryNotFound


recipients = {}
for issue in lensfun.get_issues(state="open", labels=[calibration_request_label]):
    upload_hash = issue.title.split()[-1]
    upload_path, uploader_email = get_upload_data(upload_hash)
    recipients.setdefault(uploader_email, set()).add(issue.html_url)


body_template = """Dear uploaders,

unfortunately, I've been unable to tender Lensfun during the last 6
months.  Now, it is picking up speed again, but I am unable to
process all your wonderful uploads.  Thus, I spent the last two
weeks to open the calibration business to everyone.  It is now
GitHub- and ownCloud-based.  Uploading works the same way as before,
but processing can be done by anyone willing to help, see
<https://github.com/lensfun/lensfun/blob/master/tools/calibration_webserver/workflow.rst>.

I apologise for not having been active and responsive over the last
months.  I will continue to make calibrations myself, but I hope to
do this not alone.  A couple of people are already on board.
Besides, a single point-of-failure was a bad idea anyway.  Thus,
there is a chance that it'll work faster and more reliably in the
future.

Your uploads (it may be only one) are tracked here:

{}

If you have a GitHub account, you may add a comment to those issues
saying “I am the uploader”.  Then, any questions will be discussed
on GitHub.  Else, we contact you by email.  In any case, an email is
sent to you once the processing is done.

Thank you again for your contribution!

(This is an automatically generated message.)
"""

for email, urls in recipients.items():
    body = body_template.format("\n".join(urls))
#    send_email(email, "Your calibration upload", body)
    print("Sent mail to {}.".format(email))
