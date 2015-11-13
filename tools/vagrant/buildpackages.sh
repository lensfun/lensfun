#!/bin/bash
set -e

if [ ! -f .vagrant/machines/default/virtualbox/id ]
then
   vagrant up
fi
vagrant ssh -c "cd lensfun ; ./buildpackages.sh"
