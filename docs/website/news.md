---
title: News
linktitle: News
layout: default
weight: 2
---

{% for post in site.posts limit: 40 %}
<div class="news">
<h2 class="news-title">{{ post.date | date: "%Y-%m-%d" }} {{ post.title }}</h2>
{{ post.content }}
</div>
{% endfor %}
