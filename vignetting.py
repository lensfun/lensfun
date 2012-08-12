#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import unicode_literals, division, absolute_import

import math, multiprocessing
from PIL import Image, ImageFilter


images_manager = multiprocessing.Manager().dict()

def load_image(side, images_manager):
    image = Image.open("/tmp/vignetting/{0}.tiff".format(side)).convert("L")
    if side == "left":
        image = image.crop((image.size[0] // 2, 0, image.size[0], image.size[1]))
    else:
        image = image.crop((0, 0, image.size[0] // 2, image.size[1]))
    image = image.resize((image.size[0] // 4, image.size[1] // 4), Image.BICUBIC)
    image = image.filter(ImageFilter.MedianFilter(17))
    image = image.resize((image.size[0] // 4, image.size[1] // 4), Image.BICUBIC)
    image = image.resize((image.size[0] // 4, image.size[1] // 4), Image.BICUBIC)
    images_manager[side] = (image.size, image.tostring())

left_process = multiprocessing.Process(target=load_image, args=("left", images_manager))
left_process.start()
right_process = multiprocessing.Process(target=load_image, args=("right", images_manager))
right_process.start()
left_process.join()
right_process.join()

images = {}
for side, image in images_manager.items():
    images[side] = Image.fromstring("L", image[0], image[1])


dimensions = images["left"].size
assert images["right"].size == dimensions
half_width, half_height = dimensions[0] / 2 - 1, dimensions[1] / 2 - 1
half_diagonal = math.hypot(half_width + 1, half_height + 1)

def get_r(x, y):
    return math.hypot(x - half_width, y - half_height) / half_diagonal


print get_r(511, 383)

images["left"].save("/tmp/left.tiff")
