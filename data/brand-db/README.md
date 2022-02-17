
## Name and sorting rules

### File name rules and scheme
This section containes rules for the name scheme of xml database files.

#### File name rules
- All characters should be lower case (Example: `BrandName` -> `brandname`)
- All space characters should be replaced by an underscore (Example: `brand name` -> `brand_name`)
- All entries as singular (Example: `brand-cameras.xml` -> `brand-camera.xml`)

#### File name scheme
- Brand name first
- Hyphen after the brand name
- Object group second (see below for possible entries)
- File ending is .xml
- File is named after the brand, not the manufacturer

**Possible object group entries:**
- `action_camera` = For action cameras
- `camera` = For regular SLR (single-lens reflex) and MIL (mirrorless interchangeable-lens) cameras with interchangeable lens
- `compact_camera` = For point-and-shoot cameras with no interchangeable lens
- `drone` = For cameras fixed to drones
- `lens` = For lenses for cameras with interchangeable lens
- `portable_device` = For smartphones, tablets and similar other small devices

### Database entry rules and scheme
This section containes rules for the scheme within the xml database files.

- Entries sorted by alphabetical order of the `</model>` tag (the `<maker>` tag should be ignored)
- Lens entries with different lens mounts sorted in alphabetical order of their lens mount name
- Lens entries sorted ascending by their minumum focal length. For identical minumum focal, sort lens entries ascending by their maximum focal length. For identical maximum focal, sort lens entries ascending by their maximum aperture.
- Lens entries that only differ by their crop factor sorted ascending by their crop factor value.
- Lems emtries that include extenders sorted ascending by their multiplication value after the lens without the extender.
- Lens entries for action cameras, compact cameras, drones and portable devices sorted right after the camera
- Add `<!-- Device Name: NAME -->` before every entry with the full device name. Action cameras, compact cameras, drones and portable devices lenses only should only include the line once.
- Add `<!-- Wikidata Item: ITEM -->` for every entry with the device's Wikidata item (if existing)
- Add `<!-- Same as DEVICE (name in COUNTRY) -->` for every entry that also exists under a different name
- Add `<!-- Calibration images taken with CAMERA -->` for every entry with the camera name the lens has been calibrated with if possible

## Missing lenses
This section containes licences known to miss callibration data in the database

### Canon
- Canon EF 24–70mm f/2.8L II USM (Wikidata: Q1156947 (series))
- Canon EF 24–70mm f/4L IS USM (Wikidata: Q1156947 (series))
- Canon EF 24-105mm f/4L f/3.5-5.6 IS STM (Wikidata: Q1065981 (series))
- Canon EF 24-105mm f/4L f/4L IS II USM (Wikidata: Q1065981 (series))
- Canon EF-M 18-150mm f/3.5-6.3 IS STM (Wikidata: Q28035405)
- Canon EF-M 28mm f/3.5 Macro IS STM (Wikidata: Q24909141)
- Canon EF-M 32mm f/1.4 STM (Wikidata: Q100876300)
- Canon EF-S 18-55mm f/3.5-5.6 USM (Wikidata: Q1143683 (series))
- Canon EF-S 18-55mm f/3.5-5.6 II
- Canon EF-S 18-55mm f/3.5-5.6 II USM
- Canon EF-S 18-55mm f/3.5-5.6 IS II
- Canon EF-S 18-55mm f/4–5.6 IS STM
- Canon RF 5.2mm F2.8L DUAL FISHEYE
- Canon RF 16mm F2.8 STM
- Canon RF 35mm F1.8 MACRO IS STM (Wikidata: Q97154439)
- Canon RF 50mm F1.2L USM (Wikidata: Q101245617)