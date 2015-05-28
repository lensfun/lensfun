The Lensfun website is based on __Jekyll__ which is a static website generator.
See http://jekyllrb.com/ for more information.

Basic instructions:

* Install Ruby and its development packages
      
      sudo apt-get install ruby ruby-dev nodejs

* Install Jekyll

      sudo gem install jekyll rdiscount

* Run Jekyll to build the html files and serve the whole web site 
  with a local web server at http://localhost:4000/

      jekyll serve

* All web content can be found in the `_site/` subfolder and may be 
  packaged into the documentation or uploaded to any web server.
  Use `jekyll build` to only build without starting a local web server.

