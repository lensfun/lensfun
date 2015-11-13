#!/bin/bash
set -e

vagrant ssh -c "cd lensfun ; ./buildpackages.sh"
