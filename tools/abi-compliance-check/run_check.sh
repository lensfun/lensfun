#!/bin/bash

# set commit, branch or tag to compare
LF_COMMIT_NEW=release_0.3.x
LF_COMMIT_OLD=v0.3.2

ACC_VERSION=1.99.21

CURRENT_BRANCH=`git rev-parse --abbrev-ref HEAD`

# download and unzip the abi-compliance-checker tool
wget https://github.com/lvc/abi-compliance-checker/archive/$ACC_VERSION.zip
unzip $ACC_VERSION.zip
rm $ACC_VERSION.zip

# prepare build directory
mkdir build
cd build

# checkout and build new version
git checkout $LF_COMMIT_NEW
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../LF_new/ ../../../
make && make install

# checkout and build old version
git checkout $LF_COMMIT_OLD
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../LF_old/ ../../../
make && make install

cd ../

# prepare config
sed "s|<version>.*</version>|<version>${LF_COMMIT_OLD}</version>|" LF_tmp_old.xml > LF_old.xml
sed "s|<version>.*</version>|<version>${LF_COMMIT_NEW}</version>|" LF_tmp_new.xml > LF_new.xml

# run check and show results in firefox
perl abi-compliance-checker-$ACC_VERSION/abi-compliance-checker.pl -lib lensfun -new LF_new.xml  -old LF_old.xml
firefox compat_reports/lensfun/${LF_COMMIT_OLD}_to_${LF_COMMIT_NEW}/compat_report.html &

# cleanup
echo "Cleanup..."
rm -rf build
rm -rf LF_new
rm -rf LF_old
rm -rf logs
rm -rf abi-compliance-checker-*
rm LF_new.xml
rm LF_old.xml

echo "Going back to branch <$CURRENT_BRANCH>"
git checkout $CURRENT_BRANCH
