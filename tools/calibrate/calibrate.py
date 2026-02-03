#!/usr/bin/env python3
#
# Copyright 2012-2016 Torsten Bronger <bronger@physik.rwth-aachen.de>

import subprocess, os, os.path, sys, multiprocessing, math, re, contextlib, glob, struct, argparse
import xml.etree.ElementTree as ET

missing_packages = set()
try:
    import numpy
except ImportError:
    missing_packages.add("python3-numpy")
try:
    from scipy.optimize import leastsq
except ImportError:
    missing_packages.add("python3-scipy")

def test_program(program, package_name):
    try:
        subprocess.call([program], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except OSError:
        missing_packages.add(package_name)

convert_program = "convert"
if os.name == 'nt':
    # on Windows, there is a system binary called convert.exe to convert FAT partitions to NTFS, so we need to call
    # the ImageMagick's convert.exe by full path; this path can be set in the environment variable IM_CONVERT,
    # or the default installation path is used
    convert_program = os.environ.get("IM_CONVERT", "C:\\Program Files\\ImageMagick\\convert.exe")
test_program("dcraw_emu", "libraw-bin")
test_program(convert_program, "imagemagick")
test_program("tca_correct", "hugin-tools")
test_program("exiv2", "exiv2")
if missing_packages:
    print(f"The following packages are missing (Ubuntu packages, names may differ on other systems):\n    {'  '.join(missing_packages)}\nAbort.")
    sys.exit()
try:
    dcraw_version = float(subprocess.run(["dcraw_emu"], stdout=subprocess.PIPE, check=False).stdout.splitlines()[1].
                          rpartition(b"v")[2])
except (OSError, IndexError, ValueError):
    dcraw_version = 0
h_option = [] if 8.99 < dcraw_version < 9.18 else ["-h"]


parser = argparse.ArgumentParser(description="Calibration generator for Lensfun.")
parser.add_argument("--complex-tca", action="store_true", help="switches on non-linear polynomials for TCA")
parser.add_argument("--verbose", action="store_true", help="enable verbose logging")
args = parser.parse_args()

def log(msg):
    if args.verbose:
        print(f"[INFO] {msg}")


def unquote_filename_component(name):
    """Unescapes `name` so that special escaping is replaced by the actual
    characters.  This undoes the work of
    `tools.calibration_webserver.process_upload.quote_directory` (or more
    accurately, the ``quote_filename_component`` function therein).

    :param str name: the name to be unescaped

    :returns:
      the unescaped name

    :rtype: str
    """
    assert "/" not in name
    name = re.sub(r"\{(\d+)\}", lambda match: chr(int(match.group(1))), name)
    name = name.replace("##", "=").replace("++", "*").replace("___", ":").replace("__", "/").replace("_", " ")
    return name


@contextlib.contextmanager
def chdir(dirname=None):
    curdir = os.getcwd()
    try:
        if dirname is not None:
            os.chdir(dirname)
        yield
    finally:
        os.chdir(curdir)


class Lens:

    def __init__(self, name, maker, mount, cropfactor, aspect_ratio, type_):
        self.name, self.maker, self.mount, self.cropfactor, self.aspect_ratio, self.type_ = \
                    name, maker, mount, cropfactor, aspect_ratio, type_
        self.calibration_elements = []
        self.minimal_focal_length = float("inf")

    def add_focal_length(self, focal_length):
        self.minimal_focal_length = min(self.minimal_focal_length, focal_length)

    def __lt__(self, other):
        return self.minimal_focal_length < other.minimal_focal_length

    def to_element(self):
        lens_elem = ET.Element("lens")
        ET.SubElement(lens_elem, "maker").text = self.maker
        ET.SubElement(lens_elem, "model").text = self.name
        ET.SubElement(lens_elem, "mount").text = self.mount
        ET.SubElement(lens_elem, "cropfactor").text = self.cropfactor
        if self.type_:
            ET.SubElement(lens_elem, "type").text = self.type_
        if self.aspect_ratio and self.aspect_ratio != "3:2":
            ET.SubElement(lens_elem, "aspect-ratio").text = self.aspect_ratio
        if self.calibration_elements:
            calibration_elem = ET.SubElement(lens_elem, "calibration")
            for elem in self.calibration_elements:
                calibration_elem.append(elem)
        return lens_elem


def generate_raw_conversion_call(filename, dcraw_options):
    basename, extension = os.path.splitext(filename)
    extension = extension[1:]
    if extension.lower() in ["jpg", "jpeg", "tif"]:
        result = [convert_program, filename]
        if "-4" in dcraw_options:
            result.extend(["-colorspace", "RGB", "-depth", "16"])
        result.append("tiff:-" if "-" in dcraw_options else basename + ".tiff")
        return result

    return ["dcraw_emu", "-T", "-t", "0"] + dcraw_options + [filename]


raw_file_extensions = ["3fr", "ari", "arw", "bay", "crw", "cr2", "cr3", "cap", "dcs", "dcr", "dng", "drf", "eip", "erf", "fff",
                       "iiq", "k25", "kdc", "mef", "mos", "mrw", "nef", "nrw", "obm", "orf", "pef", "ptx", "pxn", "r3d",
                       "raf", "raw", "rwl", "rw2", "rwz", "sr2", "srf", "srw", "x3f", "jpg", "jpeg", "tif"]
def find_raw_files():
    result = []
    for file_extension in raw_file_extensions:
        result.extend(glob.glob("*." + file_extension))
        result.extend(glob.glob("*." + file_extension.upper()))
    return result

def browse_directory(directory):
    if os.path.exists(directory):
        with chdir(directory):
            for filename in find_raw_files():
                full_filename = os.path.join(directory, filename)
                match = filepath_pattern.match(os.path.splitext(filename)[0])
                if match:
                    file_exif_data[full_filename] = \
                        (unquote_filename_component(match.group("lens_model")),
                         float(match.group("focal_length")), float(match.group("aperture")))
                else:
                    exiv2_candidates.append(full_filename)

def call_exiv2(candidate_group):
    result_proc = subprocess.run(
        ["exiv2", "-PEkt", "-g", "Exif.Photo.LensModel", "-g", "Exif.Photo.FocalLength", "-g", "Exif.Photo.FNumber"]
        + candidate_group, stdout=subprocess.PIPE, check=False)
    lines = result_proc.stdout.decode("utf-8").splitlines()
    assert result_proc.returncode in [0, 253]
    result = {}
    for line in lines:
        filename, data = line.split("Exif.Photo.")
        filename = filename.rstrip()
        if not filename:
            assert len(candidate_group) == 1
            filename = candidate_group[0]
        fieldname, field_value = data.split(None, 1)
        exif_data = result.setdefault(filename, [None, float("nan"), float("nan")])
        if fieldname == "LensModel":
            exif_data[0] = field_value
        elif fieldname == "FocalLength":
            exif_data[1] = float(field_value.partition("mm")[0])
        elif fieldname == "FNumber":
            exif_data[2] = float(field_value.partition("F")[2])
    for filename, exif_data in result.copy().items():
        result[filename] = tuple(exif_data)
    return result

def generate_tca_tiffs(filename):
    tca_filename = filename + ".tca"
    if not os.path.exists(tca_filename):
        tiff_filename = os.path.splitext(filename)[0] + ".tiff"
        if not os.path.exists(tiff_filename):
            subprocess.check_call(generate_raw_conversion_call(filename, ["-4", "-o", "0", "-M", "-Z", "tiff"]))
        return filename, tiff_filename, tca_filename
    return None, None, None

def evaluate_image_set(exif_data, filepaths):
    output_filename = f"{exif_data[0]}--{exif_data[1]}--{exif_data[2]}--{exif_data[3]}".replace(" ", "_").replace("/", "__").replace(":", "___"). \
                      replace("*", "++").replace("=", "##")
    gnuplot_filename = f"{output_filename}.gp"
    try:
        with open(gnuplot_filename, encoding="utf-8") as gnuplot_file:
            gnuplot_line = gnuplot_file.readlines()[3]
        match = re.match(r'     [-e.0-9]+ \* \(1 \+ \((?P<k1>[-e.0-9]+)\) \* x\*\*2 \+ \((?P<k2>[-e.0-9]+)\) \* x\*\*4 \+ '
                         r'\((?P<k3>[-e.0-9]+)\) \* x\*\*6\) title "fit"', gnuplot_line)
        k1, k2, k3 = [float(k) for k in match.groups()]
    except OSError:
        radii, intensities = [], []
        for filepath in filepaths:
            maximal_radius = 1
            try:
                with open(os.path.splitext(filepath)[0] + ".txt", encoding="utf-8") as sidecar_file:
                    for line in sidecar_file:
                        if line.startswith("maximal_radius"):
                            maximal_radius = float(line.partition(":")[2])
            except FileNotFoundError:
                pass
            with subprocess.Popen(generate_raw_conversion_call(filepath, ["-4", "-M", "-o", "0", "-Z", "-"] + h_option),
                                  stdout=subprocess.PIPE) as dcraw_process:
                image_data = subprocess.check_output(
                    [convert_program, "tiff:-", "-set", "colorspace", "RGB", "-resize", "250", "pgm:-"], stdin=dcraw_process.stdout,
                    stderr=subprocess.DEVNULL)
                dcraw_process.stdout.close()
            width, height = None, None
            header_size = 0
            for i, line in enumerate(image_data.splitlines(True)):
                header_size += len(line)
                if i == 0:
                    assert line == b"P5\n", "Wrong image format (must be NetPGM binary)"
                else:
                    line = line.partition(b"#")[0].strip()
                    if line:
                        if not width:
                            width, height = line.split()
                            width, height = int(width), int(height)
                        else:
                            assert line == b"65535", f"Wrong grayscale depth: {int(line)} (must be 65535)"
                            break
            half_diagonal = math.hypot(width // 2, height // 2)
            image_data = struct.unpack(f"!{header_size}s{width * height}H", image_data)[1:]
            for i, intensity in enumerate(image_data):
                y, x = divmod(i, width)
                radius = math.hypot(x - width // 2, y - height // 2) / half_diagonal
                if radius <= maximal_radius:
                    radii.append(radius)
                    intensities.append(intensity)
        all_points_filename = f"{output_filename}-all_points.dat"
        with open(all_points_filename, "w", encoding="utf-8") as outfile:
            for radius, intensity in zip(radii, intensities):
                outfile.write(f"{radius} {intensity}\n")
        number_of_bins = 16
        bins = [[] for i in range(number_of_bins)]
        for radius, intensity in zip(radii, intensities):
            # The zeroth and the last bin are only half bins which means that their
            # means are skewed.  But this is okay: For the zeroth, the curve is
            # supposed to be horizontal anyway, and for the last, it underestimates
            # the vignetting at the rim which is a good thing (too much of
            # correction is bad).
            bin_index = int(round(radius / maximal_radius * (number_of_bins - 1)))
            bins[bin_index].append(intensity)
        radii = [i / (number_of_bins - 1) * maximal_radius for i in range(number_of_bins)]
        intensities = [numpy.median(bin) for bin in bins]
        bins_filename = f"{output_filename}-bins.dat"
        with open(bins_filename, "w", encoding="utf-8") as outfile:
            for radius, intensity in zip(radii, intensities):
                outfile.write(f"{radius} {intensity}\n")
        radii, intensities = numpy.array(radii), numpy.array(intensities)

        def fit_function(radius, A, k1, k2, k3):
            return A * (1 + k1 * radius**2 + k2 * radius**4 + k3 * radius**6)

        A, k1, k2, k3 = leastsq(lambda p, x, y: y - fit_function(x, *p), [30000, -0.3, 0, 0], args=(radii, intensities))[0]

        lens_name, focal_length, aperture, distance = exif_data
        if distance == float("inf"):
            distance = "\u221e"
        with open(gnuplot_filename, "w", encoding="utf-8") as gnuplot_file:
            gnuplot_file.write(f"""set grid
set title "{lens_name}, {focal_length} mm, f/{aperture}, {distance} m"
plot "{all_points_filename}" with dots title "samples", "{bins_filename}" with linespoints lw 4 title "average", \\
     {A} * (1 + ({k1}) * x**2 + ({k2}) * x**4 + ({k3}) * x**6) title "fit"
pause -1
""")

    return (k1, k2, k3)


if __name__ == '__main__':

    #
    # Collect EXIF data
    #
    log("Collecting EXIF data")

    file_exif_data = {}
    filepath_pattern = re.compile(r"(?P<lens_model>.+)--(?P<focal_length>[0-9.]+)mm--(?P<aperture>[0-9.]+)")
    missing_lens_model_warning_printed = False
    exiv2_candidates = []


    browse_directory("distortion")
    browse_directory("tca")
    for directory in glob.glob("vignetting*"):
        browse_directory(directory)
    candidates_per_group = len(exiv2_candidates) // multiprocessing.cpu_count() + 1
    candidate_groups = []
    while exiv2_candidates:
        candidate_group = exiv2_candidates[:candidates_per_group]
        if candidate_group:
            candidate_groups.append(candidate_group)
        del exiv2_candidates[:candidates_per_group]

    with multiprocessing.Pool() as pool:
        for group_exif_data in pool.map(call_exiv2, candidate_groups):
            file_exif_data.update(group_exif_data)
    for filename in list(file_exif_data):  # list() because I change the dict during iteration
        lens_model, focal_length, aperture = file_exif_data[filename]
        if not lens_model:
            lens_model = "Standard"
            if not missing_lens_model_warning_printed:
                print(f"{filename}:")
                print("""I couldn't detect the lens model name and assumed "Standard".
For cameras without interchangeable lenses, this may be correct.
However, this fails if there is data of different undetectable lenses.
A newer version of exiv2 (if available) may help.
(This message is printed only once.)\n""")
                missing_lens_model_warning_printed = True
            file_exif_data[filename] = (lens_model, focal_length, aperture)
        if numpy.isnan(focal_length) or numpy.isnan(aperture):
            print(f"{filename}:")
            print("""Aperture and/or focal length EXIF data is missing in this RAW file.
You have to rename them according to the scheme "Lens_name--16mm--1.4.RAW"
(Use your RAW file extension of course.)  Abort.""")
            sys.exit(1)


    #
    # Generation TIFFs from distortion RAWs
    #
    log("Generating TIFFs from distortion RAWs")

    if os.path.exists("distortion"):
        with chdir("distortion"):
            with multiprocessing.Pool() as pool:
                results = set()
                for filename in find_raw_files():
                    if not os.path.exists(os.path.splitext(filename)[0] + ".tiff"):
                        results.add(pool.apply_async(subprocess.call, [generate_raw_conversion_call(filename, ["-w", "-Z", "tiff"])]))
                pool.close()
                pool.join()
                for result in results:
                    result.get()


    #
    # Parse/generate lenses.txt
    #

    log("Parsing/generating lenses.txt")

    lens_line_pattern = re.compile(
        r"(?P<name>.+):\s*(?P<maker>[^,]+)\s*,\s*(?P<mount>[^,]+)\s*,\s*(?P<cropfactor>[^,]+)"
        r"(\s*,\s*(?P<aspect_ratio>\d+:\d+|[0-9.]+))?(\s*,\s*(?P<type>[^,]+))?")
    distortion_line_pattern = re.compile(r"\s*distortion\((?P<focal_length>[.0-9]+)mm\)\s*=\s*"
                                         r"(?P<a>[-.0-9e]+)(?:\s*,\s*(?P<b>[-.0-9e]+)\s*,\s*(?P<c>[-.0-9e]+))?")
    lenses = {}
    try:
        with open("lenses.txt", encoding="utf-8") as lenses_file:
            for linenumber, original_line in enumerate(lenses_file, start=1):
                line = original_line.strip()
                if line and not line.startswith("#"):
                    match = lens_line_pattern.match(line)
                    if match:
                        data = match.groupdict()
                        current_lens = Lens(data["name"], data["maker"], data["mount"], data["cropfactor"],
                                            data["aspect_ratio"], data["type"])
                        lenses[data["name"]] = current_lens
                    else:
                        match = distortion_line_pattern.match(line)
                        if not match:
                            print(f"Invalid line {linenumber} in lenses.txt:\n{original_line}Abort.")
                            sys.exit(1)
                        data = list(match.groups())
                        focal_length = float(data[0])
                        current_lens.add_focal_length(focal_length)
                        if data[2] is None:
                            elem = ET.Element("distortion")
                            elem.set("model", "poly3")
                            elem.set("focal", f"{focal_length:g}")
                            elem.set("k1", data[1])
                        else:
                            elem = ET.Element("distortion")
                            elem.set("model", "ptlens")
                            elem.set("focal", f"{focal_length:g}")
                            elem.set("a", data[1])
                            elem.set("b", data[2])
                            elem.set("c", data[3])
                        current_lens.calibration_elements.append(elem)
    except OSError:
        focal_lengths = {}
        for exif_data in file_exif_data.values():
            focal_lengths.setdefault(exif_data[0], set()).add(exif_data[1])
        lens_names_by_focal_length = sorted((min(lengths), lens_name) for lens_name, lengths in focal_lengths.items())
        lens_names_by_focal_length = [item[1] for item in lens_names_by_focal_length]
        with open("lenses.txt", "w", encoding="utf-8") as outfile:
            if focal_lengths:
                outfile.write("""# For suggestions for <maker> and <mount> see
# <https://github.com/lensfun/lensfun/tree/master/data/db>.
# <aspect-ratio> is optional and by default 3:2.
# Omit <type> for rectilinear lenses.
""")
                for lens_name in lens_names_by_focal_length:
                    outfile.write(f"\n{lens_name}: <maker>, <mount>, <cropfactor>, <aspect-ratio>, <type>\n")
                    # FixMe: Only generate focal lengths that are available for
                    # *distortion*.
                    for length in sorted(focal_lengths[lens_name]):
                        outfile.write(f"distortion({length}mm) = , , \n")
            else:
                outfile.write("""# No RAW images found (or no focal lengths in them).
# Please have a look at
# http://wilson.bronger.org/lens_calibration_tutorial/
""")
        print("I wrote a template lenses.txt.  Please fill this file with proper information.  Abort.")
        sys.exit(1)


    #
    # TCA correction
    #
    log("Running TCA correction")

    if os.path.exists("tca"):

        with chdir("tca"):
            with multiprocessing.Pool() as pool:
                raw_files = find_raw_files()
                for filename, tiff_filename, tca_filename in pool.map(generate_tca_tiffs, raw_files):
                    if filename:
                        output = subprocess.check_output(["tca_correct", "-o", "bv" if args.complex_tca else "v", tiff_filename],
                                                         stderr=subprocess.DEVNULL).splitlines()[-1].strip()
                        exif_data = file_exif_data[os.path.join("tca", filename)]
                        with open(tca_filename, "w", encoding="utf-8") as outfile:
                            outfile.write(f"{exif_data[0]}\n{exif_data[1]}\n{output.decode('ascii')}\n")
            calibration_elements = {}
            for filename in find_raw_files():
                with open(filename + ".tca", encoding="utf-8") as tca_file:
                    lens_name, focal_length, tca_output = [line.rstrip("\n") for line in tca_file.readlines()]
                focal_length = float(focal_length)
                data = re.match(
                    r"-r [.0]+:(?P<br>[-.0-9]+):[.0]+:(?P<vr>[-.0-9]+) -b [.0]+:(?P<bb>[-.0-9]+):[.0]+:(?P<vb>[-.0-9]+)",
                    tca_output).groupdict()
                with open(filename + "_tca.gp", "w", encoding="utf-8") as gp_file:
                    gp_file.write(
                        f'set title "{filename}"\n'
                        f'plot [0:1.8] {data["br"]} * x**2 + {data["vr"]} title "red", '
                        f'{data["bb"]} * x**2 + {data["vb"]} title "blue"\n'
                        f'pause -1')
                elem = ET.Element("tca")
                elem.set("model", "poly3")
                elem.set("focal", f"{focal_length:g}")
                if args.complex_tca:
                    elem.set("br", data["br"])
                    elem.set("vr", data["vr"])
                    elem.set("bb", data["bb"])
                    elem.set("vb", data["vb"])
                else:
                    elem.set("vr", data["vr"])
                    elem.set("vb", data["vb"])
                calibration_elements.setdefault(lens_name, []).append((focal_length, elem))
            for lens_name, elements in calibration_elements.items():
                elements.sort(key=lambda x: x[0])
                lenses[lens_name].calibration_elements.extend(elem for _, elem in elements)


    #
    # Vignetting
    #
    log("Running vignetting correction")

    images = {}
    distances_per_triplett = {}

    for vignetting_directory in glob.glob("vignetting*"):
        distance = float(vignetting_directory.partition("_")[2] or "inf")
        assert distance == float("inf") or distance < 1000
        with chdir(vignetting_directory):
            for filename in find_raw_files():
                exif_data = file_exif_data[os.path.join(vignetting_directory, filename)] + (distance,)
                distances_per_triplett.setdefault(exif_data[:3], set()).add(distance)
                images.setdefault(exif_data, []).append(os.path.join(vignetting_directory, filename))

    vignetting_db_entries = {}

    with multiprocessing.Pool() as pool:
        results = {}
        for exif_data, filepaths in images.items():
            results[exif_data] = pool.apply_async(evaluate_image_set, [exif_data, filepaths])
        for exif_data, result in results.items():
            vignetting_db_entries[exif_data] = result.get()

    new_vignetting_db_entries = {}
    for configuration, vignetting in vignetting_db_entries.items():
        triplett = configuration[:3]
        distances = distances_per_triplett[triplett]
        if len(distances) == 1 and list(distances)[0] > 10:
            # If only one distance was measured and at more than 10m, insert it twice
            # at 10m and ∞, so that the whole range is covered.
            new_vignetting_db_entries[tuple(configuration[:3] + (10,))] = \
                new_vignetting_db_entries[tuple(configuration[:3] + (float("inf"),))] = vignetting
        else:
            new_vignetting_db_entries[configuration] = vignetting
    vignetting_db_entries = new_vignetting_db_entries


    for configuration in sorted(vignetting_db_entries):
        lens, focal_length, aperture, distance = configuration
        vignetting = vignetting_db_entries[configuration]
        if distance == float("inf"):
            distance = 1000
        try:
            elem = ET.Element("vignetting")
            elem.set("model", "pa")
            elem.set("focal", f"{focal_length:g}")
            elem.set("aperture", f"{aperture:g}")
            elem.set("distance", f"{distance:g}")
            elem.set("k1", f"{vignetting[0]:.4f}")
            elem.set("k2", f"{vignetting[1]:.4f}")
            elem.set("k3", f"{vignetting[2]:.4f}")
            lenses[lens].calibration_elements.append(elem)
        except KeyError:
            print(f'Lens "{lens}" not found in lenses.txt.  Abort.')
            sys.exit(1)

    log("Updating lensfun.xml")
    root = ET.Element("lensdatabase")
    for lens in sorted(lenses.values()):
        root.append(lens.to_element())
    ET.indent(root, space="    ")
    tree = ET.ElementTree(root)
    with open("lensfun.xml", "w", encoding="utf-8") as outfile:
        tree.write(outfile, encoding="unicode", xml_declaration=False)
        outfile.write("\n")
