from django.conf.urls import patterns, include, url

# Uncomment the next two lines to enable the admin:
# from django.contrib import admin
# admin.autodiscover()

urlpatterns = patterns("calibration.views",
                       (r"^$", "upload"),
                       (r"^results/(?P<id_>.+)", "show_issues"),
                       (r"^thumbnails/(?P<id_>[^/]+)/(?P<hash_>.+)", "thumbnail"),
)
