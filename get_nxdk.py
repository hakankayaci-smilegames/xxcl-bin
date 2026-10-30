import urllib.request
url = "https://raw.githubusercontent.com/XboxDev/nxdk/master/lib/hal/xbox.h"
content = urllib.request.urlopen(url).read().decode("utf-8")
for line in content.split("\n"):
    if "XC_" in line:
        print(line)
