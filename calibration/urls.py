from django.conf.urls import patterns, include, url
from django.views.generic import TemplateView

# Uncomment the next two lines to enable the admin:
# from django.contrib import admin
# admin.autodiscover()

urlpatterns = patterns("calibration.views",
                       (r"^$", "upload"),
                       (r"^results/(?P<id_>.+)", "show_issues"),
                       (r"^target_tips$", TemplateView.as_view(template_name="calibration/good_bad_ugly.html")),
                       (r"^thumbnails/(?P<id_>.+?)/(?P<hash_>[0-9a-f]+)", "thumbnail"),
)
