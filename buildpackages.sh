#!/bin/bash

# remove previous build folder
rm -rf lensfun-0.3.2

# 
git archive HEAD --prefix=lensfun-0.3.2/ -o lensfun_0.3.2.orig.tar.gz
tar xvzf lensfun_0.3.2.orig.tar.gz

# remove debian folder from HEAD and replace with working copy 
rm -rf lensfun-0.3.2/debian
cp -r debian/ lensfun-0.3.2/

# build packages
cd lensfun-0.3.2
debuild -us -uc

cd ..
