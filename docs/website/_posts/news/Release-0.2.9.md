---
published: false
title: Release 0.2.9
layout: default
---

Last update: Git commit 40d5bd8311bcdbd9367174f3f5c10ebe2ea19ff9

* Streamlined the names of Nikkor, Tamron, Tokina, Panasonic, Leica, Olympus,
  and Sigma lenses so that auto-detection works better
* Fixed names of Sony Coolpix cameras
* Comprehensive update of Lensfun's documentation
* Lens lists may now be sorted by focal length
* The <cropfactor> tag is now mandatory for <camera> and <lens> in the database files
* Increased accuracy because APS-C is not anymore assumed to have the crop
  factor 1.5 (non-Canon) or 1.6 (Canon), but the accurate crop factor of the
  respective camera
* Added command line tool "update-lensfun-data" for updating the database
* Added command line tool "lensfun-add-adapter" for managing mount compatibilities
* Removed compatibility of Four Third on Micro Four Third
* Removed compatibility of Sony Alpha on Sony E
* Many bugfixes, especially for the case if sensor sizes of calibration and
  image to-be-corrected are different
* MaxAperture is now the maximal f-stop number one can set on the given lens
* Removed non-working implementation of CCI
* Added new cameras

New/updated data for:

* Canon EF 24-70mm f/2.8L II USM
* Canon EF 24-105mm f/4L IS USM
* Canon EF 50mm f/1.8 MkII
* Canon EF 70-200mm f/2.8L IS II USM
* Canon EF 70-200mm f/4L IS USM
* Canon EF 100mm f/2.8L Macro IS USM
* Nikkor AF-S 10-24mm f/3.5-4.5G DX ED
* Nikkor AF-S 16-85mm f/3.5-5.6G DX ED VR
* Nikkor AF-S 18-55mm f/3.5-5.6G DX VR
* Nikkor AF-S 18-105mm f/3.5-5.6G DX ED VR
* Nikkor AF-S 35mm f/1.8G DX
* Sigma 15-30mm f/3.5-4.5 EX DG Aspherical
* Sigma 35mm f/1.4 DG HSM
* Sigma 50mm f/1.4 EX DG HSM
* Sigma 150mm f/2.8 EX DG APO HSM Macro
* Lumix G Vario 14-42 f/3.5-5.6 II
* Lumix G X Vario PZ 14-42 f/3.5-5.6

New interchangeable lenses:

* Canon EF-S 15-85mm f/3.5-5.6 IS USM
* Canon EF-M 18-55mm f/3.5-5.6 IS STM
* Canon EF-M 22mm f/2 STM
* Canon EF-S 18-55mm f/3.5-5.6 IS
* Canon EF-S 18-135mm f/3.5-5.6 IS STM
* Canon EF 24-70mm f/2.8L II USM
* Canon EF 50mm f/1.8 MkII
* Canon EF 50-200mm f/3.5-4.5L
* Canon EF-S 55-250mm f/4-5.6 IS
* 1 Nikkor AW 11-27.5mm f/3.5-5.6
* Nikkor AI-S 50mm f/1.2
* Nikkor AF-S 50mm f/1.8G
* Nikkor AF-S 55-300mm f/4.5-5.6G DX ED VR
* Nikkor AF-S 70-200mm f/4G VR IF-ED
* Nikkor AF-S 70-300mm f/4.5-5.6G VR IF-ED
* Fujifilm XF 18-55mm f/2.8-4 R LM OIS
* Olympus M.9-18mm f/4.0-5.6
* Olympus M.40-150mm f/4.0-5.6 R
* Lumix G Vario 12-35 f/2.8
* Lumix G Vario 14-42 f/3.5-5.6 II
* Lumix G 20 f/1.7 II
* Lumix G Vario 35-100 f/2.8
* smc Pentax-FA 28mm f/2.8 AL
* smc Pentax-DA L 55-300mm f/4-5.8 ED
* Samyang 8mm f/3.5 Fish-Eye CS
* Samsung Zoom 18-55mm f/3.5-5.6 OIS
* Samsung NX 20mm f/2.8 Pancake
* Samsung NX 45mm f/1.8 2D/3D
* Samyang 35mm f/1.4 AS UMC
* Sigma 10mm f/2.8 EX DC Fisheye HSM
* Sigma 17-70mm f/2.8-4 DC Macro OS HSM
* Sigma 19mm f/2.8 EX DN
* Sigma 30mm f/2.8 EX DN
* Sigma 50-150mm f/2.8 APO EX DC HSM II
* Sigma 60mm f/2.8 DN
* Sigma 70-200mm f/2.8 EX DG Macro HSM II
* Sigma 80-400mm f/4.5-5.6 EX DG OS
* Sigma 100-300mm f/4 APO EX DG HSM
* Sigma 105mm f/2.8 EX DG OS HSM Macro
* Tamron 18-200mm f/3.5-6.3 XR Di II LD
* Tokina AF 16-28mm f/2.8 AT-X Pro SD FX
* Tokina 500mm f/8 RMC Mirror Lens
* Carl Zeiss Distagon T* 2,8/21 ZE
* Carl Zeiss Distagon T* 2,8/21 ZF.2

New compact cameras:

* Canon PowerShot A720
* Canon PowerShot IXUS 70
* Canon PowerShot S90
* Canon PowerShot SX220 HS
* Canon PowerShot SX230 HS
* Fujifilm FinePix F770EXR
* Fujifilm FinePix HS20EXR
* Panasonic Lumix DMC-LX7
* Panasonic Lumix DMC-FZ200
* Sony DSC-HX300
* Sony RX100 II