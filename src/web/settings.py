from os import getenv, getcwd

base_dir = getenv("BASE_DIR", "/home/pi/images/")
home_dir = getcwd()

DEFAULT_CONFIG = "default.json"
MAX_FILE_SIZE = getenv("MAX_FILE_SIZE", 1024 * 1024 * 1024 * 2)  # 2gb

HARDDRIVE_FILE_SUFFIX = ("hda", "hdn", "hdi", "nhd", "hdf", "hds", "dsk")
CDROM_FILE_SUFFIX = ("iso", "cdr", "toast", "img")
REMOVABLE_FILE_SUFFIX = ("hdr",)
ARCHIVE_FILE_SUFFIX = ("zip",)
VALID_FILE_SUFFIX = HARDDRIVE_FILE_SUFFIX + REMOVABLE_FILE_SUFFIX + \
        CDROM_FILE_SUFFIX + ARCHIVE_FILE_SUFFIX

# File containing canonical drive properties
DRIVE_PROPERTIES_FILE = home_dir + "/drive_properties.json"
# File ending used for drive properties files
PROPERTIES_SUFFIX = "properties"

REMOVABLE_DEVICE_TYPES = ("SCCD", "SCRM", "SCMO")
