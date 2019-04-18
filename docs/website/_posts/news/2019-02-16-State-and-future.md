---
title: The past, the current state and the future
layout: default
category: news
---

The Lensfun project was started by Andrew Zabolotny in 2007 to lay the foundation of a free and open source database for the correction of photographic lens errors. After Lensfun did not receive any more fixes and database updates, I more or less took over the maintenance from Andrew in 2012. Afterwards, my main focus was to continuously integrate the submitted profiles and database changes and to regularly release new versions to make these changes available for the application users.

From 2012 till today a lot of changes and improvements came to the Lensfun project. Torsten Bronger established the [calibration webservice](https://wilson.bronger.org/calibration) and [GitHub repo](https://github.com/lensfun/lensfun) where today most of the calibration work is done by a couple of people. He also dived deep into the maths behind the corrections and fixed many errors and inaccuracies.
Many other people contributed fixes, lens profiles and tutorials with instructions and scripts to facilitate the calibration process.

If we look at the number of lens profiles for each release, we can see that there is an increasing growth and the Git master database now nearly counts 1000 lens profiles.

   Version |  #  
  ---------|-----
   0.2.6   | 350  
   0.2.7   | 374 
   0.2.8   | 441 
   0.3.0   | 580 
   0.3.1   | 667 
   0.3.2   | 714 
   Master  | 978 

More and more [image editors](/usage) and even some commercial applications today use Lensfun and its database for their processing. That's a great achievement and underlines the importance of the project.

In 2016 I started to refactor most of the library internals as it turned out that its structure cannot be extended, e.g. with the perspective correction now available in the alpha release, and was really difficult to maintain. A lot of code has been modernized and cleaned up to make Lensfun ready for the future. But unfortunately, the time I can spent in coding for the Lensfun project since then has decreased a lot and today the project is more or less stalled.

However, there is still a lot of work to be done. Just to name a few things that seem to be important to me:

1) Create a new stable release

2) Allow database lens entries that contain calibration data from various crop factors. Lensfun then just picks the closest one for each requested modification.

3) Move the complete project to GitHub and modernize the project infrastructure and build system. Use Travis CI and Appveyor for testing.

4) Develop a strategy how to decouple the database from the library source code to allow a faster release of new profiles to the user base. There is already the lensfun-updata-data script, but this only works reliably on Linux and not on OSX and Windows until now.

Currently, I do not have time to maintain the Lensfun project in way that is satisfactory for myself, for the community and for the users. This project really needs someone who takes a look at the bigger picture and has the vision and motivation to keep the project alive and in a good condition.

Therefore, I would like to ask the community to take over the Lensfun project management and maintenance in the next months. I will still be around and contribute from time to time. But to be realistic, none of the above listed changes will happen if no one else kicks in and does the job. Ideally, Lensfun is taken under the hood of a bigger project with experienced developers like e.g. Darktable or Rawtherapee that already makes use of Lensfun.

Let me know if you want to get involved!

