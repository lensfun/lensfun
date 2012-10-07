# Copyright 2010-2012 Sebastian Kraft
#
# DESCRIPTION:
# This is a python script to create a html formatted list of all lenses and 
# cameras in the lensfun database. If necessary adjust "XMLDir" variable 
# below to point to your lensfun database.
#
 
XMLDir = "../../data/db/"

###################################################################################

import os
from xml.dom.minidom import parse
from collections import defaultdict
import re

SLRDict = defaultdict(list)
CompactCamDict = defaultdict(list)
MirrorlessDict = defaultdict(list)

#MakerDict = {
#'canon': 'Canon',
#'casio':'Casio',
#'contax':'Contax',
#'fujifilm':'Fujifilm',
#'hasselblad':'Hasselblad',
#'kodak':'Kodak',
#'konica-minolta':'Konica Minolta',
#'leica':'Leica',
#'nikon':'Nikon',
#'olympus':'Olympus',
#'panasonic':'Panasonic',
#'pentax':'Pentax',
#'ricoh':'Ricoh',
#'samsung':'Samsung',
#'sigma':'Sigma',
#'sony':'Sony'
#}

# get files in database directory
FileList = os.listdir(XMLDir)
XMLFileList = [f for f in FileList if (os.path.splitext(f)[1]).lower()=='.xml']

# find all xml files in database directory
for xmlfile in XMLFileList:
    dom = parse (XMLDir + os.sep + xmlfile)

    camtype_search = re.search('(.*)-(.*).xml', xmlfile, re.IGNORECASE)
   
        
    if camtype_search:
        camtype = camtype_search.group(1)
        for node in dom.getElementsByTagName('lens'):  # visit every node <bar />

            # look for calibration data
            calibnode = node.getElementsByTagName('calibration')
            if calibnode :                       
                mount = node.getElementsByTagName('mount')[0].firstChild.data.strip()
                
                # put olympus to compact cameras as it is a fixed lens SLR
                if mount=='olympusE10':
                    camtype='compact'           
                
                # branch to different cam types
                if camtype == 'slr':
                    model = node.getElementsByTagName('model')[0].firstChild.data.strip()
                    maker = node.getElementsByTagName('maker')[0].firstChild.data.strip()    
                    SLRDict[maker].append(model)

                if camtype == 'mil':
                    model = node.getElementsByTagName('model')[0].firstChild.data.strip()
                    maker = node.getElementsByTagName('maker')[0].firstChild.data.strip()
                    MirrorlessDict[maker].append(model)
                    
                                
                if camtype == 'compact':
                    lensmodel = node.getElementsByTagName('model')[0].firstChild.data.strip()                                                     
                    for camnode in dom.getElementsByTagName('camera'):  
                            if (camnode.getElementsByTagName('mount')[0].firstChild.data.strip() == mount):
                                    maker = camnode.getElementsByTagName('maker')[0].firstChild.data.strip()
                                    model = camnode.getElementsByTagName('model')[0].firstChild.data.strip()
                                    
                                    variantnode = camnode.getElementsByTagName('variant')                                    
                                    if variantnode:                                   
                                        model = model + ' ' + variantnode[0].firstChild.data.strip()   
                                            
                                    if lensmodel=='Standard':
                                        CompactCamDict[maker].append(model)
                                    else:
                                        CompactCamDict[maker].append(model + ', ' + lensmodel)                                        
                

                                    

# print html
print ("<p><b>SLR Lenses</b></p>")
for maker in sorted(SLRDict.keys()):
                print ("<br/><p><b>"+maker+"</b></p>")
                for  model in sorted(SLRDict[maker]):
                        print(model + "<br />")
print ("<br /><br />\n\n")
print ("<p><b>Mirrorless</b></p>")
for maker in sorted(MirrorlessDict.keys()):
                print ("<br/><p><b>"+maker+"</b></p>")
                for  model in sorted(MirrorlessDict[maker]):
                        print(model + "<br />")

print ("<br /><br />\n\n")
print ("<p><b>Compact Cameras</b></p>")
for maker in sorted(CompactCamDict.keys()):
                print ("<br/><p><b>"+maker+"</b></p>")
                for  model in sorted(CompactCamDict[maker]):
                        print(model + "<br />")
