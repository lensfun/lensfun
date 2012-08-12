
# makefile for panorama stitching, created by hugin using the new makefilelib

# Tool configuration
NONA=nona
PTSTITCHER=PTStitcher
PTMENDER=PTmender
PTBLENDER=PTblender
PTMASKER=PTmasker
PTROLLER=PTroller
ENBLEND=enblend
ENFUSE=enfuse
SMARTBLEND=smartblend.exe
HDRMERGE=hugin_hdrmerge
RM=rm
EXIFTOOL=exiftool

# Project parameters
HUGIN_PROJECTION=2
HUGIN_HFOV=360
HUGIN_WIDTH=3000
HUGIN_HEIGHT=1500

# options for the programs
NONA_LDR_REMAPPED_COMP=-z LZW
NONA_OPTS=
ENBLEND_OPTS= -w -f3000x1500
ENBLEND_LDR_COMP=--compression=LZW
ENBLEND_EXPOSURE_COMP=--compression=LZW
ENBLEND_HDR_COMP=
HDRMERGE_OPTS=-m avg -c
ENFUSE_OPTS= -w
EXIFTOOL_COPY_ARGS=-ImageDescription -Make -Model -Artist -WhitePoint -Copyright -GPS:all -DateTimeOriginal -CreateDate -UserComment -ColorSpace -OwnerName -SerialNumber
EXIFTOOL_INFO_ARGS='-Software=Hugin' '-UserComment<$${{UserComment}}&\#xa;Projection: Equirectangular (2)&\#xa;FOV: 360 x 180&\#xa;Ev: 10.29' -f

# the output panorama
LDR_REMAPPED_PREFIX={output_filename}
LDR_REMAPPED_PREFIX_SHELL={output_filename}
HDR_STACK_REMAPPED_PREFIX={output_filename}_hdr_
HDR_STACK_REMAPPED_PREFIX_SHELL={output_filename}_hdr_
LDR_EXPOSURE_REMAPPED_PREFIX={output_filename}_exposure_layers_
LDR_EXPOSURE_REMAPPED_PREFIX_SHELL={output_filename}_exposure_layers_
PROJECT_FILE={working_directory}{output_filename}.pto
PROJECT_FILE_SHELL={working_directory}{output_filename}.pto
LDR_BLENDED={output_filename}.tif
LDR_BLENDED_SHELL={output_filename}.tif
LDR_STACKED_BLENDED={output_filename}_fused.tif
LDR_STACKED_BLENDED_SHELL={output_filename}_fused.tif
LDR_EXPOSURE_LAYERS_FUSED={output_filename}_blended_fused.tif
LDR_EXPOSURE_LAYERS_FUSED_SHELL={output_filename}_blended_fused.tif
HDR_BLENDED={output_filename}_hdr.exr
HDR_BLENDED_SHELL={output_filename}_hdr.exr

# first input image
INPUT_IMAGE_1={working_directory}{input_filenames.0}.tiff
INPUT_IMAGE_1_SHELL={working_directory}{input_filenames.0}.tiff

# all input images
INPUT_IMAGES={working_directory}{input_filenames.0}.tiff\
{working_directory}{input_filenames.1}.tiff\
{working_directory}{input_filenames.2}.tiff
INPUT_IMAGES_SHELL={working_directory}{input_filenames.0}.tiff\
{working_directory}{input_filenames.1}.tiff\
{working_directory}{input_filenames.2}.tiff

# remapped images
LDR_LAYERS={output_filename}0000.tif\
{output_filename}0001.tif\
{output_filename}0002.tif
LDR_LAYERS_SHELL={output_filename}0000.tif\
{output_filename}0001.tif\
{output_filename}0002.tif

# remapped images (hdr)
HDR_LAYERS={output_filename}_hdr_0000.exr\
{output_filename}_hdr_0001.exr\
{output_filename}_hdr_0002.exr
HDR_LAYERS_SHELL={output_filename}_hdr_0000.exr\
{output_filename}_hdr_0001.exr\
{output_filename}_hdr_0002.exr

# remapped maxval images
HDR_LAYERS_WEIGHTS={output_filename}_hdr_0000_gray.pgm\
{output_filename}_hdr_0001_gray.pgm\
{output_filename}_hdr_0002_gray.pgm
HDR_LAYERS_WEIGHTS_SHELL={output_filename}_hdr_0000_gray.pgm\
{output_filename}_hdr_0001_gray.pgm\
{output_filename}_hdr_0002_gray.pgm

# stacked hdr images
HDR_STACK_0={output_filename}_stack_hdr_0000.exr
HDR_STACK_0_SHELL={output_filename}_stack_hdr_0000.exr
HDR_STACK_0_INPUT={output_filename}_hdr_0000.exr\
{output_filename}_hdr_0001.exr\
{output_filename}_hdr_0002.exr
HDR_STACK_0_INPUT_SHELL={output_filename}_hdr_0000.exr\
{output_filename}_hdr_0001.exr\
{output_filename}_hdr_0002.exr
HDR_STACKS_NUMBERS=0 
HDR_STACKS=$(HDR_STACK_0) 
HDR_STACKS_SHELL=$(HDR_STACK_0_SHELL) 

# number of image sets with similar exposure
LDR_EXPOSURE_LAYER_0={output_filename}_exposure_0000.tif
LDR_EXPOSURE_LAYER_0_SHELL={output_filename}_exposure_0000.tif
LDR_EXPOSURE_LAYER_0_INPUT={output_filename}_exposure_layers_0000.tif\
{output_filename}_exposure_layers_0001.tif\
{output_filename}_exposure_layers_0002.tif
LDR_EXPOSURE_LAYER_0_INPUT_SHELL={output_filename}_exposure_layers_0000.tif\
{output_filename}_exposure_layers_0001.tif\
{output_filename}_exposure_layers_0002.tif
LDR_EXPOSURE_LAYER_0_INPUT_PTMENDER={output_filename}0000.tif\
{output_filename}0001.tif\
{output_filename}0002.tif
LDR_EXPOSURE_LAYER_0_INPUT_PTMENDER_SHELL={output_filename}0000.tif\
{output_filename}0001.tif\
{output_filename}0002.tif
LDR_EXPOSURE_LAYER_0_EXPOSURE=10.2928
LDR_EXPOSURE_LAYERS_NUMBERS=0 
LDR_EXPOSURE_LAYERS=$(LDR_EXPOSURE_LAYER_0) 
LDR_EXPOSURE_LAYERS_SHELL=$(LDR_EXPOSURE_LAYER_0_SHELL) 
LDR_EXPOSURE_LAYERS_REMAPPED={output_filename}_exposure_layers_0000.tif\
{output_filename}_exposure_layers_0001.tif\
{output_filename}_exposure_layers_0002.tif
LDR_EXPOSURE_LAYERS_REMAPPED_SHELL={output_filename}_exposure_layers_0000.tif\
{output_filename}_exposure_layers_0001.tif\
{output_filename}_exposure_layers_0002.tif

# stacked ldr images
LDR_STACK_0={output_filename}_stack_ldr_0000.tif
LDR_STACK_0_SHELL={output_filename}_stack_ldr_0000.tif
LDR_STACK_0_INPUT={output_filename}_exposure_layers_0000.tif\
{output_filename}_exposure_layers_0001.tif\
{output_filename}_exposure_layers_0002.tif
LDR_STACK_0_INPUT_SHELL={output_filename}_exposure_layers_0000.tif\
{output_filename}_exposure_layers_0001.tif\
{output_filename}_exposure_layers_0002.tif
LDR_STACKS_NUMBERS=0 
LDR_STACKS=$(LDR_STACK_0) 
LDR_STACKS_SHELL=$(LDR_STACK_0_SHELL) 
DO_LDR_BLENDED=1

all : startStitching $(LDR_BLENDED) 

startStitching : 
	@echo '==========================================================================='
	@echo 'Stitching panorama'
	@echo '==========================================================================='

clean : 
	@echo '==========================================================================='
	@echo 'Remove temporary files'
	@echo '==========================================================================='
	-$(RM) $(LDR_LAYERS_SHELL) 

test : 
	@echo '==========================================================================='
	@echo 'Testing programs'
	@echo '==========================================================================='
	@echo -n 'Checking nona...'
	@-$(NONA) --help > /dev/null 2>&1 && echo '[OK]' || echo '[FAILED]'
	@echo -n 'Checking enblend...'
	@-$(ENBLEND) -h > /dev/null 2>&1 && echo '[OK]' || echo '[FAILED]'
	@echo -n 'Checking enfuse...'
	@-$(ENFUSE) -h > /dev/null 2>&1 && echo '[OK]' || echo '[FAILED]'
	@echo -n 'Checking hugin_hdrmerge...'
	@-$(HDRMERGE) -h > /dev/null 2>&1 && echo '[OK]' || echo '[FAILED]'
	@echo -n 'Checking exiftool...'
	@-$(EXIFTOOL) -ver > /dev/null 2>&1 && echo '[OK]' || echo '[FAILED]'

info : 
	@echo '==========================================================================='
	@echo '***************  Panorama makefile generated by Hugin       ***************'
	@echo '==========================================================================='
	@echo 'System information'
	@echo '==========================================================================='
	@echo -n 'Operating system: '
	@-uname -o
	@echo -n 'Release: '
	@-uname -r
	@echo -n 'Kernel version: '
	@-uname -v
	@echo -n 'Machine: '
	@-uname -m
	@echo 'Disc usage'
	@-df -h
	@echo 'Memory usage'
	@-free -m
	@echo '==========================================================================='
	@echo 'Output options'
	@echo '==========================================================================='
	@echo 'Hugin Version: 2011.4.0.cf9be9344356'
	@echo 'Project file: {working_directory}{output_filename}.pto'
	@echo 'Output prefix: {output_filename}'
	@echo 'Projection: Equirectangular (2)'
	@echo 'Field of view: 360 x 180'
	@echo 'Canvas dimensions: 3000 x 1500'
	@echo 'Crop area: (0,0) - (3000,1500)'
	@echo 'Output exposure value: 10.29'
	@echo 'Selected outputs'
	@echo 'Normal panorama'
	@echo '* Blended panorama'
	@echo '==========================================================================='
	@echo 'Input images'
	@echo '==========================================================================='
	@echo 'Number of images in project file: 3'
	@echo 'Number of active images: 3'
	@echo 'Image 0: {working_directory}{input_filenames.0}.tiff'
	@echo 'Image 0: Size 6024x4024, Exposure: 10.29'
	@echo 'Image 1: {working_directory}{input_filenames.1}.tiff'
	@echo 'Image 1: Size 6024x4024, Exposure: 10.29'
	@echo 'Image 2: {working_directory}{input_filenames.2}.tiff'
	@echo 'Image 2: Size 6024x4024, Exposure: 10.29'

# Rules for ordinary TIFF_m and hdr output

{output_filename}0000.tif : {working_directory}{input_filenames.0}.tiff $(PROJECT_FILE) 
	$(NONA) $(NONA_OPTS) $(NONA_LDR_REMAPPED_COMP) -r ldr -m TIFF_m -o $(LDR_REMAPPED_PREFIX_SHELL) -i 0 $(PROJECT_FILE_SHELL)

{output_filename}_hdr_0000.exr : {working_directory}{input_filenames.0}.tiff $(PROJECT_FILE) 
	$(NONA) $(NONA_OPTS) -r hdr -m EXR_m -o $(HDR_STACK_REMAPPED_PREFIX_SHELL) -i 0 $(PROJECT_FILE_SHELL)

{output_filename}0001.tif : {working_directory}{input_filenames.1}.tiff $(PROJECT_FILE) 
	$(NONA) $(NONA_OPTS) $(NONA_LDR_REMAPPED_COMP) -r ldr -m TIFF_m -o $(LDR_REMAPPED_PREFIX_SHELL) -i 1 $(PROJECT_FILE_SHELL)

{output_filename}_hdr_0001.exr : {working_directory}{input_filenames.1}.tiff $(PROJECT_FILE) 
	$(NONA) $(NONA_OPTS) -r hdr -m EXR_m -o $(HDR_STACK_REMAPPED_PREFIX_SHELL) -i 1 $(PROJECT_FILE_SHELL)

{output_filename}0002.tif : {working_directory}{input_filenames.2}.tiff $(PROJECT_FILE) 
	$(NONA) $(NONA_OPTS) $(NONA_LDR_REMAPPED_COMP) -r ldr -m TIFF_m -o $(LDR_REMAPPED_PREFIX_SHELL) -i 2 $(PROJECT_FILE_SHELL)

{output_filename}_hdr_0002.exr : {working_directory}{input_filenames.2}.tiff $(PROJECT_FILE) 
	$(NONA) $(NONA_OPTS) -r hdr -m EXR_m -o $(HDR_STACK_REMAPPED_PREFIX_SHELL) -i 2 $(PROJECT_FILE_SHELL)

# Rules for exposure layer output

{output_filename}_exposure_layers_0000.tif : {working_directory}{input_filenames.0}.tiff $(PROJECT_FILE) 
	$(NONA) $(NONA_OPTS) $(NONA_LDR_REMAPPED_COMP) -r ldr -e 10.2928 -m TIFF_m -o $(LDR_EXPOSURE_REMAPPED_PREFIX_SHELL) -i 0 $(PROJECT_FILE_SHELL)

{output_filename}_exposure_layers_0001.tif : {working_directory}{input_filenames.1}.tiff $(PROJECT_FILE) 
	$(NONA) $(NONA_OPTS) $(NONA_LDR_REMAPPED_COMP) -r ldr -e 10.2928 -m TIFF_m -o $(LDR_EXPOSURE_REMAPPED_PREFIX_SHELL) -i 1 $(PROJECT_FILE_SHELL)

{output_filename}_exposure_layers_0002.tif : {working_directory}{input_filenames.2}.tiff $(PROJECT_FILE) 
	$(NONA) $(NONA_OPTS) $(NONA_LDR_REMAPPED_COMP) -r ldr -e 10.2928 -m TIFF_m -o $(LDR_EXPOSURE_REMAPPED_PREFIX_SHELL) -i 2 $(PROJECT_FILE_SHELL)

# Rules for LDR and HDR stack merging, a rule for each stack

$(LDR_STACK_0) : $(LDR_STACK_0_INPUT) 
	$(ENFUSE) $(ENFUSE_OPTS) -o $(LDR_STACK_0_SHELL) -- $(LDR_STACK_0_INPUT_SHELL)
	-$(EXIFTOOL) -overwrite_original_in_place -TagsFromFile $(INPUT_IMAGE_1_SHELL) $(EXIFTOOL_COPY_ARGS) $(LDR_STACK_0_SHELL)

$(HDR_STACK_0) : $(HDR_STACK_0_INPUT) 
	$(HDRMERGE) $(HDRMERGE_OPTS) -o $(HDR_STACK_0_SHELL) -- $(HDR_STACK_0_INPUT_SHELL)

$(LDR_BLENDED) : $(LDR_LAYERS) 
	$(ENBLEND) $(ENBLEND_LDR_COMP) $(ENBLEND_OPTS) -o $(LDR_BLENDED_SHELL) -- $(LDR_LAYERS_SHELL)
	-$(EXIFTOOL) -E -overwrite_original_in_place -TagsFromFile $(INPUT_IMAGE_1_SHELL) $(EXIFTOOL_COPY_ARGS) $(EXIFTOOL_INFO_ARGS) $(LDR_BLENDED_SHELL)

$(LDR_EXPOSURE_LAYER_0) : $(LDR_EXPOSURE_LAYER_0_INPUT) 
	$(ENBLEND) $(ENBLEND_EXPOSURE_COMP) $(ENBLEND_OPTS) -o $(LDR_EXPOSURE_LAYER_0_SHELL) -- $(LDR_EXPOSURE_LAYER_0_INPUT_SHELL)
	-$(EXIFTOOL) -overwrite_original_in_place -TagsFromFile $(INPUT_IMAGE_1_SHELL) $(EXIFTOOL_COPY_ARGS) $(LDR_EXPOSURE_LAYER_0_SHELL)

$(LDR_STACKED_BLENDED) : $(LDR_STACKS) 
	$(ENBLEND) $(ENBLEND_LDR_COMP) $(ENBLEND_OPTS) -o $(LDR_STACKED_BLENDED_SHELL) -- $(LDR_STACKS_SHELL)
	-$(EXIFTOOL) -E -overwrite_original_in_place -TagsFromFile $(INPUT_IMAGE_1_SHELL) $(EXIFTOOL_COPY_ARGS) $(EXIFTOOL_INFO_ARGS) $(LDR_STACKED_BLENDED_SHELL)

$(LDR_EXPOSURE_LAYERS_FUSED) : $(LDR_EXPOSURE_LAYERS) 
	$(ENFUSE) $(ENBLEND_LDR_COMP) $(ENFUSE_OPTS) -o $(LDR_EXPOSURE_LAYERS_FUSED_SHELL) -- $(LDR_EXPOSURE_LAYERS_SHELL)
	-$(EXIFTOOL) -E -overwrite_original_in_place -TagsFromFile $(INPUT_IMAGE_1_SHELL) $(EXIFTOOL_COPY_ARGS) $(EXIFTOOL_INFO_ARGS) $(LDR_EXPOSURE_LAYERS_FUSED_SHELL)

$(HDR_BLENDED) : $(HDR_STACKS) 
	$(ENBLEND) $(ENBLEND_HDR_COMP) $(ENBLEND_OPTS) -o $(HDR_BLENDED_SHELL) -- $(HDR_STACKS_SHELL)

$(LDR_REMAPPED_PREFIX)_multilayer.tif : $(LDR_LAYERS) 
	tiffcp $(LDR_LAYERS_SHELL) $(LDR_REMAPPED_PREFIX_SHELL)_multilayer.tif

$(LDR_REMAPPED_PREFIX)_fused_multilayer.tif : $(LDR_STACKS) $(LDR_EXPOSURE_LAYERS) 
	tiffcp $(LDR_STACKS_SHELL) $(LDR_EXPOSURE_LAYERS_SHELL) $(LDR_REMAPPED_PREFIX_SHELL)_fused_multilayer.tif

$(LDR_REMAPPED_PREFIX)_multilayer.psd : $(LDR_LAYERS) 
	PTtiff2psd -o $(LDR_REMAPPED_PREFIX_SHELL)_multilayer.psd $(LDR_LAYERS_SHELL)

$(LDR_REMAPPED_PREFIX)_fused_multilayer.psd : $(LDR_STACKS) $(LDR_EXPOSURE_LAYERS) 
	PTtiff2psd -o $(LDR_REMAPPED_PREFIX_SHELL)_fused_multilayer.psd $(LDR_STACKS_SHELL)$(LDR_EXPOSURE_LAYERS_SHELL)
