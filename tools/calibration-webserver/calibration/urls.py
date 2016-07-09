from django.conf.urls import url
from django.views.generic import TemplateView
from calibration.views import upload, show_issues, thumbnail

# Uncomment the next two lines to enable the admin:
# from django.contrib import admin
# admin.autodiscover()

urlpatterns = [
    url(r"^$", upload),
    url(r"^results/(?P<id_>.+)", show_issues, name="show_issues"),
    url(r"^target_tips$", TemplateView.as_view(template_name="calibration/good_bad_ugly.html")),
    url(r"^thumbnails/(?P<id_>.+?)/(?P<hash_>[0-9a-f]+)", thumbnail),
]
