# Lensfun Calibration Tool

`calibrate.py` generates Lensfun XML calibration data (distortion, TCA, and
vignetting) from RAW image files. It reads EXIF data, processes images, fits
correction models, and outputs a `lensfun.xml` file ready for inclusion in the
Lensfun database.

For the full tutorial, see
[wilson.bronger.org/lens_calibration_tutorial/](https://wilson.bronger.org/lens_calibration_tutorial/).

## Requirements

- Python 3
- Python packages: `numpy`, `scipy`
- External tools: `dcraw_emu` (libraw), `convert` (ImageMagick),
  `tca_correct` (Hugin), `exiv2`

On Debian/Ubuntu:

```sh
apt install python3-numpy python3-scipy libraw-bin imagemagick hugin-tools exiv2
```

On Windows, set the `IM_CONVERT` environment variable to the full path of
ImageMagick's `convert.exe` to avoid conflict with the system `convert.exe`.

## Usage

```sh
python3 calibrate.py [--complex-tca] [--verbose]
```

| Option           | Description                                       |
| ---------------- | ------------------------------------------------- |
| `--complex-tca`  | Use non-linear polynomials for TCA (adds `br`/`bb` coefficients) |
| `--verbose`      | Print progress messages                           |

## Step-by-step workflow

**TL;DR**

- shoot images
- put them in directories
- run script
- edit `lenses.txt`
- run script again
- use `lensfun.xml`

### 1. Photograph calibration targets

Shoot RAW images for each calibration type you want to cover. You don't need
all three -- the tool processes whichever files are present.

**Distortion** -- photograph buildings or structures with prominent straight
lines, at least 8m away. Capture one image per focal length (for zooms, pick
five or more focal lengths spanning the zoom range).

**TCA (chromatic aberration)** -- photograph subjects with sharp black-on-white
edges that reach into the frame corners. Subjects should be 8+m away and the
image should be as sharp as possible. One image per focal length.

**Vignetting** -- have a diffuser (translucent white plastic, <=3mm thick)
flush against the front of the lens and photograph under even lighting. Use
aperture priority, lowest ISO, lock manual focus at infinity. For each focal
length, capture images at:

- Maximum aperture
- 2-3 intermediate stops (1-stop intervals)
- Minimum aperture

For a prime lens, this is ~5 images. For a zoom at 5 focal lengths, ~25.

### 2. Organize RAW files

Create a workspace directory and sort your RAW files into subdirectories:

```
./
    distortion/       your distortion RAW files
    tca/              your TCA RAW files
    vignetting/       your vignetting RAW files
```

Most common RAW formats are supported (CR2, CR3, NEF, ARW, DNG, RAF, ORF, PEF,
RW2, etc.).

If your images lack EXIF lens data (adapted lenses, manual lenses), you must
encode the metadata in the filename -- see
[EXIF data and filename patterns](#exif-data-and-filename-patterns) below.

### 3. First run -- generate lenses.txt

```sh
python3 calibrate.py
```

On the first run the tool:

- [Reads EXIF data (or filename patterns)](#exif-data-and-filename-patterns) from all RAW files
- Converts distortion RAW files to TIFF (for use in Hugin)
- Writes a template [`lenses.txt`](#lensestxt) and exits

The generated template lists every detected lens and focal length, but the
lens metadata fields and distortion coefficients are blank. The lens names in
the template are taken directly from EXIF (or from filename patterns) and must
stay as-is -- the tool matches RAW files to `lenses.txt` entries by exact lens
name.

### 4. Determine distortion coefficients with Hugin

Open the generated TIFF files from `distortion/` in Hugin's lens calibration
tool. For each focal length, Hugin will compute distortion coefficients
(a, b, c for the `ptlens` model, or a single k1 for the `poly3` model).

Use Hugin → Lens Calibration → Load images → Optimize → read the a/b/c values

### 5. Fill in lenses.txt

Edit `lenses.txt` and fill in the lens metadata (maker, mount, crop factor)
and the distortion coefficients obtained from Hugin. See
[lenses.txt format](#lensestxt) for the full syntax and an example.

### 6. Second run -- generate lensfun.xml

```sh
python3 calibrate.py [--complex-tca]
```

This time the tool reads the completed `lenses.txt`, processes TCA and
vignetting images, fits correction models, and writes `lensfun.xml`.

### 7. Use the result

The output `lensfun.xml` is a Lensfun database fragment. You can:

- Install it locally (see the Lensfun manual for database search paths)
- Submit it for inclusion in the
  [Lensfun database](https://github.com/lensfun/lensfun/tree/master/data/db)

## Directory structure

```
./
    distortion/          Distortion calibration RAW files
    tca/                 Transverse chromatic aberration RAW files
    vignetting/          Vignetting RAW files (infinity focus)
    vignetting_5/        Vignetting RAW files at 5m focus distance
    vignetting_0.5/      Vignetting RAW files at 0.5m focus distance
    lenses.txt           Lens metadata (generated on first run)
```

### Multiple focus distances

The bare `vignetting/` directory is treated as infinity focus. To measure at
specific focus distances, use directories named `vignetting_<meters>`:

| Directory          | Focus distance |
| ------------------ | -------------- |
| `vignetting/`      | infinity       |
| `vignetting_5`     | 5m             |
| `vignetting_0.5`   | 0.5m           |
| `vignetting_0.2`   | 0.2m           |

If only one distance is measured and it is greater than 10m, the tool
duplicates the result at both 10m and infinity to cover the full range.

### Maximal radius sidecar files

If a vignetting image doesn't cover the full image circle (e.g., due to a lens
hood), place a text sidecar file alongside the RAW file with the same base name
and a `.txt` extension:

```
vignetting/IMG_1234.CR2
vignetting/IMG_1234.txt
```

containing:

```
maximal_radius: 0.9
```

The value is a fraction of the image half-diagonal (`1.0` = full frame). Pixels
beyond this radius are excluded from the vignetting fit.

## lenses.txt

On the first run, if `lenses.txt` does not exist, the tool generates a template
from the detected [EXIF data or filename patterns](#exif-data-and-filename-patterns) and exits.
You must fill in the remaining lens metadata before running again.

The file should contain a separate block for each calibrated lens. Each lens
block starts with a header line:

```
<lens name>: <maker>, <mount>, <cropfactor>[, <aspect-ratio>][, <type>]
```

- **lens name** -- must match the EXIF `LensModel` string exactly
- **maker** -- lens manufacturer (see existing entries in `data/db/` for
  conventions)
- **mount** -- lens mount name (see `data/db/` for the list)
- **cropfactor** -- sensor crop factor (`1.0` for full frame, `1.5` for APS-C, etc.)
- **aspect-ratio** -- optional, defaults to `3:2`
- **type** -- optional, omit for rectilinear lenses; use `fisheye` for
  fisheye lenses

Followed by distortion lines (one per focal length):

```
distortion(<focal_length>mm) = <a>, <b>, <c>
```

A single value gives a `poly3` model (k1 only). Three values give a `ptlens`
model (a, b, c). These values are typically obtained from Hugin's lens
calibration.

### Example

The `lenses.txt` file specifying three different lenses:

```
# For suggestions for <maker> and <mount> see
# <https://github.com/lensfun/lensfun/tree/master/data/db>.
# <aspect-ratio> is optional and by default 3:2.
# Omit <type> for rectilinear lenses.

Canon EF 50mm f/1.4 USM: Canon, Canon EF, 1.0
distortion(50mm) = -0.00784

Sigma 18-35mm f/1.8 DC HSM | Art: Sigma, Canon EF, 1.5
distortion(18mm) = 0.00123, -0.04567, 0.03456
distortion(24mm) = 0.00012, -0.01234, 0.00567
distortion(35mm) = -0.00234, 0.00123, -0.00045

Samyang 8mm f/3.5 Fish-Eye CS II: Samyang, Canon EF, 1.5, 3:2, fisheye
distortion(8mm) = -0.02345, 0.01234, -0.00567
```

## EXIF data and filename patterns

The tool attempts to read lens model, focal length, and aperture from EXIF via `exiv2`.
If this data is missing (common with adapted lenses, lenses with no electronic
contacts, or older cameras), encode it in the filename instead:

```
<lens_name>--<focal_length>mm--<aperture>.<ext>
```

Examples:

```
Canon_EF_50mm_f__1.4_USM--50mm--1.4.CR2
Samyang_12mm_f__2.0--12mm--2.0.ARW
```

### Filename encoding

Since some characters are invalid in filenames, apply these substitutions in
the lens name:

| Character   | Replacement     |
| ----------- | --------------- |
| ` ` (space) | `_`             |
| `/`         | `__`            |
| `:`         | `___`           |
| `=`         | `##`            |
| `*`         | `++`            |

Arbitrary characters can also be encoded as `{<codepoint>}`, e.g., `{47}` for
`/`.
