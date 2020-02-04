import urllib.request
import zipfile
import sys
import os


with open("deps.lock") as f:
    tag = f.read().strip()


def get_platform():
    if sys.platform.startswith("linux"):
        return "Linux"
    elif sys.platform == "win32":
        return "Windows"
    elif sys.platform == "darwin":
        return "macOS"
    raise Exception("Invalid platform: {}".format(sys.platform))


platform = get_platform()
zip_path = "gbemu-build-{}.zip".format(platform)
url_template = "https://github.com/jack-karamanian/gbemu-build/releases/download/{}/{}"
url = url_template.format(tag, zip_path)

print(url)

with urllib.request.urlopen(url) as res:
    with open(zip_path, "wb") as zip_file:
        zip_file.write(res.read())

    with open(zip_path, "rb") as zip_file, zipfile.ZipFile(zip_file) as zip:
        zip.extractall("./")
os.remove(zip_path)
