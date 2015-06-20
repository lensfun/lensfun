---
title: FAQ - frequently asked questions
linktitle: FAQ
layout: default
weight: 6
---

* __Why is my lens or camera not automatically recognized in image editor XYZ?__

    There may be two reasons:
    1. The camera and lens are not in the Lensfun database which is available on your computer. 
       On Linux Lensfun is usually installed system wide by a package manager. On Windows and Mac
       Lensfun is usually shipped together with the image editing application. Please check for updates!

    2. The image editor was not able to properly read the EXIF data from the image. Most open 
       source programs today use the <a href="http://www.exiv2.org/">exiv2 library</a> to retrieve 
       the camera and lens information. You can use the command line tool `exiv2` to see if 
       the retrieved information is correct:
           $> exiv2 -pt 2938031.jpeg |grep -i 'lens\|model\|make'

* __How do I know if a certain lens and camera combination will be recognized by Lensfun?__

    Please check the <a href="/lenslist/">list of supported lenses</a> for the latest release. Before 
    you report a missing lens, camera, or even start calibration by your own, please also check the 
    list of lenses of the current development version at 
    <a href="http://wilson.bronger.org/lensfun_coverage.html">
    http://wilson.bronger.org/lensfun_coverage.html</a>

* __My camera and lens is in the list of supported lenses but my image editor does not recognize them?__

    Probably the Lensfun database on your computer is out of date. On Linux, Lensfun is usually 
    installed system wide by a package manager. On Windows and Mac, Lensfun is usually shipped 
    together with the image editing application. Please check for updates!

* __My lens should be supported but the camera is missing. What can I do?__

    The camera entry in the database only holds crop factor and lens mount. As a workaround you 
    can simply select any other camera with the same lens mount and sensor size or crop factor. 

* __The lens is wrongly identified by the image editor as a different lens than I have actually used. Why?__

    Most open source programs today use the <a href="http://www.exiv2.org/">exiv2 library</a> to 
    retrieve the camera and lens information. You can use the command line tool `exiv2` to see if 
    the retrieved information is correct:
           $> exiv2 -pt 2938031.jpeg |grep -i 'lens\|model\|make'
    If the lens name is not correct in the above output please report a bug at the 
    <a href="http://dev.exiv2.org/projects/exiv2/issues">exiv2 issue tracker</a>
    
