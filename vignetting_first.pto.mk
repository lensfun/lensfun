# makefile for automatic panorama generating, created by hugin using the new makefilelib

# Tool configuration
ICPFIND=icpfind
CELESTE=celeste_standalone
CHECKPTO=checkpto
CPCLEAN=cpclean
AUTOOPTIMISER=autooptimiser
PANO_MODIFY=pano_modify
LINEFIND=linefind
PROJECT={working_directory}/vignetting.pto
PROJECT_SHELL={working_directory}/vignetting.pto

all : $(PROJECT) 
	@echo 'Finding control points...'
	$(ICPFIND) -o $(PROJECT_SHELL) $(PROJECT_SHELL)
	@echo 'Statistical cleaning of control points...'
	$(CPCLEAN) -o $(PROJECT_SHELL) $(PROJECT_SHELL)
	@echo 'Searching for vertical lines...'
	$(LINEFIND) -o $(PROJECT_SHELL) $(PROJECT_SHELL)
	$(CHECKPTO) $(PROJECT_SHELL)
	ptovariable --positions --view --barrel $(PROJECT_SHELL)
	@echo 'Optimise project...'
	$(AUTOOPTIMISER) -n -m -l -s -o $(PROJECT_SHELL) $(PROJECT_SHELL)
	ptovariable --vignetting --response --exposure $(PROJECT_SHELL)
	@echo 'Optimise photometric parameters...'
	vig_optimize -p 10000 -o $(PROJECT_SHELL) $(PROJECT_SHELL)
	@echo 'Setting output options...'
	$(PANO_MODIFY) --canvas=70%% --crop=AUTO -o $(PROJECT_SHELL) $(PROJECT_SHELL)
