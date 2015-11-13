#!/bin/bash
set -e

export LC_ALL=en_US.UTF-8

if [ ! -f .vagrant/machines/default/virtualbox/id ]
then
   vagrant up
fi
vagrant ssh -c "cd lensfun ; ./buildpackages.sh"
