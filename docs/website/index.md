---
title: Welcome!
linktitle: Home
layout: default
weight: 1
---

This site is all about the Lensfun library. What is it, you may ask?

Digital photographs are not ideal. Of course, the better is your camera, the better the results will be, but in any case if you look carefully at shots taken even by the most expensive cameras equipped with the most expensive lenses you will see various artifacts. It is very hard to make ideal cameras, because there are a lot of factors that affect the final image quality, and at some point camera and lens designers have to trade one factor for another to achieve the optimal image quality, within the given design restrictions and budget.

But we all want ideal shots, don't we? :) So that's what's Lensfun is all about – rectifying the defects introduced by your photographic equipment. 

---

## Latest news ##

{% for post in site.categories.news limit: 2 %}
<div class="news">
<h2 class="news-title-frontpage">{{ post.date | date: "%Y-%m-%d" }} {{ post.title }}</h2>
{{ post.content }}
</div>
{% endfor %}
