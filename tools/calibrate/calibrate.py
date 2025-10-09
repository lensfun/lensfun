#!/usr/bin/env python3
#
# Copyright 2012-2016 Torsten Bronger <bronger@physik.rwth-aachen.de>

missing_packages = set()

import subprocess, os, os.path, sys, multiprocessing, math, re, contextlib, glob, codecs, struct, argparse
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
        subprocess.call([program], stdout=open(os.devnull, "w"), stderr=open(os.devnull, "w"))
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
    print("The following packages are missing (Ubuntu packages, names may differ on other systems):\n    {0}\nAbort.".
          format("  ".join(missing_packages)))
    sys.exit()
try:
    dcraw_version = float(subprocess.Popen(["dcraw_emu"], stdout=subprocess.PIPE).communicate()[0].splitlines()[1].
                          rpartition("v")[2])
except:
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


class Lens(object):

    def __init__(self, name, maker, mount, cropfactor, aspect_ratio, type_):
        self.name, self.maker, self.mount, self.cropfactor, self.aspect_ratio, self.type_ = \
                    name, maker, mount, cropfactor, aspect_ratio, type_
        self.calibration_lines = []
        self.minimal_focal_length = float("inf")

    def add_focal_length(self, focal_length):
        self.minimal_focal_length = min(self.minimal_focal_length, focal_length)

    def __lt__(self, other):
        return self.minimal_focal_length < other.minimal_focal_length

    def write(self, outfile):
        type_line = "        <type>{0}</type>\n".format(self.type_) if self.type_ else ""
        outfile.write("""
    <lens>
        <maker>{0}</maker>
        <model>{1}</model>
        <mount>{2}</mount>
        <cropfactor>{3}</cropfactor>
""".format(self.maker, self.name, self.mount, self.cropfactor))
        if self.type_:
            outfile.write("        <type>{0}</type>\n".format(self.type_))
        if self.aspect_ratio and self.aspect_ratio != "3:2":
            outfile.write("        <aspect-ratio>{0}</aspect-ratio>\n".format(self.aspect_ratio))
        if self.calibration_lines:
            outfile.write("        <calibration>\n")
            for line in self.calibration_lines:
                outfile.write("            {0}\n".format(line))
            outfile.write("        </calibration>\n")
        outfile.write("    </lens>\n")


def generate_raw_conversion_call(filename, dcraw_options):
    basename, extension = os.path.splitext(filename)
    extension = extension[1:]
    if extension.lower() in ["jpg", "jpeg", "tif"]:
        result = [convert_program, filename]
        if "-4" in dcraw_options:
            result.extend(["-colorspace", "RGB", "-depth", "16"])
        result.append("tiff:-" if "-" in dcraw_options else basename + ".tiff")
        return result
    else:
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
    exiv2_process = subprocess.Popen(
        ["exiv2", "-PEkt", "-g", "Exif.Photo.LensModel", "-g", "Exif.Photo.FocalLength", "-g", "Exif.Photo.FNumber"]
        + candidate_group, stdout=subprocess.PIPE)
    lines = exiv2_process.communicate()[0].decode("utf-8").splitlines()
    assert exiv2_process.returncode in [0, 253]
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
    output_filename = "{0}--{1}--{2}--{3}".format(*exif_data).replace(" ", "_").replace("/", "__").replace(":", "___"). \
                      replace("*", "++").replace("=", "##")
    gnuplot_filename = "{0}.gp".format(output_filename)
    try:
        gnuplot_line = codecs.open(gnuplot_filename, encoding="utf-8").readlines()[3]
        match = re.match(r'     [-e.0-9]+ \* \(1 \+ \((?P<k1>[-e.0-9]+)\) \* x\*\*2 \+ \((?P<k2>[-e.0-9]+)\) \* x\*\*4 \+ '
                         r'\((?P<k3>[-e.0-9]+)\) \* x\*\*6\) title "fit"', gnuplot_line)
        k1, k2, k3 = [float(k) for k in match.groups()]
    except IOError:
        radii, intensities = [], []
        for filepath in filepaths:
            maximal_radius = 1
            try:
                sidecar_file = open(os.path.splitext(filepath)[0] + ".txt")
            except FileNotFoundError:
                pass
            else:
                for line in sidecar_file:
                    if line.startswith("maximal_radius"):
                        maximal_radius = float(line.partition(":")[2])
            dcraw_process = subprocess.Popen(generate_raw_conversion_call(filepath, ["-4", "-M", "-o", "0", "-Z", "-"] + h_option),
                                             stdout=subprocess.PIPE)
            image_data = subprocess.check_output(
                [convert_program, "tiff:-", "-set", "colorspace", "RGB", "-resize", "250", "pgm:-"], stdin=dcraw_process.stdout,
                stderr=open(os.devnull, "w"))
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
                            assert line == b"65535", "Wrong grayscale depth: {} (must be 65535)".format(int(line))
                            break
            half_diagonal = math.hypot(width // 2, height // 2)
            image_data = struct.unpack("!{0}s{1}H".format(header_size, width * height), image_data)[1:]
            for i, intensity in enumerate(image_data):
                y, x = divmod(i, width)
                radius = math.hypot(x - width // 2, y - height // 2) / half_diagonal
                if radius <= maximal_radius:
                    radii.append(radius)
                    intensities.append(intensity)
        all_points_filename = "{0}-all_points.dat".format(output_filename)
        with open(all_points_filename, "w") as outfile:
            for radius, intensity in zip(radii, intensities):
                outfile.write("{0} {1}\n".format(radius, intensity))
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
        bins_filename = "{0}-bins.dat".format(output_filename)
        with open(bins_filename, "w") as outfile:
            for radius, intensity in zip(radii, intensities):
                outfile.write("{0} {1}\n".format(radius, intensity))
        radii, intensities = numpy.array(radii), numpy.array(intensities)

        def fit_function(radius, A, k1, k2, k3):
            return A * (1 + k1 * radius**2 + k2 * radius**4 + k3 * radius**6)

        A, k1, k2, k3 = leastsq(lambda p, x, y: y - fit_function(x, *p), [30000, -0.3, 0, 0], args=(radii, intensities))[0]

        lens_name, focal_length, aperture, distance = exif_data
        if distance == float("inf"):
            distance = "∞"
        codecs.open(gnuplot_filename, "w", encoding="utf-8").write("""set grid
set title "{6}, {7} mm, f/{8}, {9} m"
plot "{0}" with dots title "samples", "{1}" with linespoints lw 4 title "average", \\
     {2} * (1 + ({3}) * x**2 + ({4}) * x**4 + ({5}) * x**6) title "fit"
pause -1
""".format(all_points_filename, bins_filename, A, k1, k2, k3, lens_name, focal_length, aperture, distance))

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

    pool = multiprocessing.Pool()
    for group_exif_data in pool.map(call_exiv2, candidate_groups):
        file_exif_data.update(group_exif_data)
    pool.close()
    pool.join()
    for filename in list(file_exif_data):  # list() because I change the dict during iteration
        lens_model, focal_length, aperture = file_exif_data[filename]
        if not lens_model:
            lens_model = "Standard"
            if not missing_lens_model_warning_printed:
                print(filename + ":")
                print("""I couldn't detect the lens model name and assumed "Standard".
For cameras without interchangeable lenses, this may be correct.
However, this fails if there is data of different undetectable lenses.
A newer version of exiv2 (if available) may help.
(This message is printed only once.)\n""")
                missing_lens_model_warning_printed = True
            file_exif_data[filename] = (lens_model, focal_length, aperture)
        if numpy.isnan(focal_length) or numpy.isnan(aperture):
            print(filename + ":")
            print("""Aperture and/or focal length EXIF data is missing in this RAW file.
You have to rename them according to the scheme "Lens_name--16mm--1.4.RAW"
(Use your RAW file extension of course.)  Abort.""")
            sys.exit()


    #
    # Generation TIFFs from distortion RAWs
    #
    log("Generating TIFFs from distortion RAWs")

    if os.path.exists("distortion"):
        with chdir("distortion"):
            pool = multiprocessing.Pool()
            results = set()
            for filename in find_raw_files():
                if not os.path.exists(os.path.splitext(filename)[0] + ".tiff"):
                    results.add(pool.apply_async(subprocess.call, [generate_raw_conversion_call(filename, ["-w", "-Z", "tiff"])]))
            pool.close()
            pool.join()
            [result.get() for result in results]


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
        for linenumber, original_line in enumerate(open("lenses.txt")):
            linenumber += 1
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
                        print("Invalid line {0} in lenses.txt:\n{1}Abort.".format(linenumber, original_line))
                        sys.exit()
                    data = list(match.groups())
                    data[0] = float(data[0])
                    current_lens.add_focal_length(data[0])
                    if data[2] is None:
                        current_lens.calibration_lines.append(
                            """<distortion model="poly3" focal="{0:g}" k1="{1}"/>""".format(data[0], data[1]))
                    else:
                        current_lens.calibration_lines.append(
                            """<distortion model="ptlens" focal="{0:g}" a="{1}" b="{2}" c="{3}"/>""".format(*data))
    except IOError:
        focal_lengths = {}
        for exif_data in file_exif_data.values():
            focal_lengths.setdefault(exif_data[0], set()).add(exif_data[1])
        lens_names_by_focal_length = sorted((min(lengths), lens_name) for lens_name, lengths in focal_lengths.items())
        lens_names_by_focal_length = [item[1] for item in lens_names_by_focal_length]
        with open("lenses.txt", "w") as outfile:
            if focal_lengths:
                outfile.write("""# For suggestions for <maker> and <mount> see
# <https://github.com/lensfun/lensfun/tree/master/data/db>.
# <aspect-ratio> is optional and by default 3:2.
# Omit <type> for rectilinear lenses.
""")
                for lens_name in lens_names_by_focal_length:
                    outfile.write("\n{0}: <maker>, <mount>, <cropfactor>, <aspect-ratio>, <type>\n".format(lens_name))
                    # FixMe: Only generate focal lengths that are available for
                    # *distortion*.
                    for length in sorted(focal_lengths[lens_name]):
                        outfile.write("distortion({0}mm) = , , \n".format(length))
            else:
                outfile.write("""# No RAW images found (or no focal lengths in them).
# Please have a look at
# http://wilson.bronger.org/lens_calibration_tutorial/
""")
        print("I wrote a template lenses.txt.  Please fill this file with proper information.  Abort.")
        sys.exit()


    #
    # TCA correction
    #
    log("Running TCA correction")

    if os.path.exists("tca"):
        
        with chdir("tca"):
            pool = multiprocessing.Pool()
            results = set()
            raw_files = find_raw_files()
            for filename, tiff_filename, tca_filename in pool.map(generate_tca_tiffs, raw_files):
                if filename:
                    output = subprocess.check_output(["tca_correct", "-o", "bv" if args.complex_tca else "v", tiff_filename],
                                                     stderr=open(os.devnull, "w")).splitlines()[-1].strip()
                    exif_data = file_exif_data[os.path.join("tca", filename)]
                    with open(tca_filename, "w") as outfile:
                        outfile.write("{0}\n{1}\n{2}\n".format(exif_data[0], exif_data[1], output.decode("ascii")))
            pool.close()
            pool.join()
            calibration_lines = {}
            for filename in find_raw_files():
                lens_name, focal_length, tca_output = [line.rstrip("\n") for line in open(filename + ".tca").readlines()]
                focal_length = float(focal_length)
                data = re.match(
                    r"-r [.0]+:(?P<br>[-.0-9]+):[.0]+:(?P<vr>[-.0-9]+) -b [.0]+:(?P<bb>[-.0-9]+):[.0]+:(?P<vb>[-.0-9]+)",
                    tca_output).groupdict()
                open(filename + "_tca.gp", "w").write(
                    """set title "{}"
plot [0:1.8] {} * x**2 + {} title "red", {} * x**2 + {} title "blue"
pause -1""".format(filename, data["br"], data["vr"], data["bb"], data["vb"]))
                calibration_lines.setdefault(lens_name, []).append((focal_length,
                    """<tca model="poly3" focal="{0:g}" br="{1}" vr="{2}" bb="{3}" vb="{4}"/>""".format(
                        focal_length, data["br"], data["vr"], data["bb"], data["vb"]) if args.complex_tca else
                    """<tca model="poly3" focal="{0:g}" vr="{1}" vb="{2}"/>""".format(
                        focal_length, data["vr"], data["vb"])))
            for lens_name, lines in calibration_lines.items():
                lines.sort()
                lenses[lens_name].calibration_lines.extend(line[1] for line in lines)


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

    pool = multiprocessing.Pool()
    results = {}
    for exif_data, filepaths in images.items():
        results[exif_data] = pool.apply_async(evaluate_image_set, [exif_data, filepaths])
    for exif_data, result in results.items():
        vignetting_db_entries[exif_data] = result.get()
    pool.close()
    pool.join()

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
            lenses[lens].calibration_lines.append(
                """<vignetting model="pa" focal="{focal_length:g}" aperture="{aperture:g}" distance="{distance:g}" """
                """k1="{vignetting[0]:.4f}" k2="{vignetting[1]:.4f}" k3="{vignetting[2]:.4f}"/>""".format(
                    focal_length=focal_length, aperture=aperture, vignetting=vignetting, distance=distance))
        except KeyError:
            print("""Lens "{0}" not found in lenses.txt.  Abort.""".format(lens))
            sys.exit()

    log("Updating lensfun.xml")
    outfile = open("lensfun.xml", "w")
    outfile.write("<lensdatabase>\n")
    for lens in sorted(lenses.values()):
        lens.write(outfile)
    outfile.write("\n</lensdatabase>\n")
