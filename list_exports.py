import urllib.request
import re

url = "https://raw.githubusercontent.com/XboxDev/xbox-linux/master/xbox/kernel/exports.txt"
try:
    req = urllib.request.urlopen(url)
    lines = req.read().decode("utf-8").split("\n")
    for line in lines:
        if "255" in line or "301" in line or "144" in line or "190" in line:
            print(line)
except Exception:
    pass
