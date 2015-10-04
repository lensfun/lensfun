#!/bin/bash


git archive HEAD --prefix=lensfun-0.3.2/ -o lensfun_0.3.2.orig.tar.gz

tar xvzf lensfun_0.3.2.orig.tar.gz
cp -r debian/ lensfun-0.3.2/
cd lensfun-0.3.2
debuild -us -uc

cd ..
rm -rf lensfun-0.3.2
