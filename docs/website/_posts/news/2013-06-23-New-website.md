---
title: New website
layout: default
category: news
---

Lensfun got a new website. Although the layout and design is quite similar to the old one, a lot of things changed under the hood. We moved from a dynamic blog and CMS engine to a static site generator called <a href="http://jekyllrb.com/">Jekyll</a>. 

This has several advantages:

* Much more easy to maintain. The whole website is now part of the SVN repository. Everybody can participate and edit.
* The website will run on any webserver. No more need for PHP, MySQL, ...
* Much faster!
* No more security trouble and time consuming updates of the CMS software.

To create the website with Jekyll it only needs two steps:

    cd lensfun/docs/website
    jekyll --server

This should start a webserver and you can view the website at http://localhost:4000/. To upload everything to a real webserver just go to the new subfolder <pre>_site</pre> and upload all the content.
