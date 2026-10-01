import urllib.request
import ssl
import re

ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

url = "https://raw.githubusercontent.com/Cxbx-Reloaded/Cxbx-Reloaded/master/src/core/hle/kernel_exports.h"
# Let's try multiple common paths if the first fails
urls = [
    "https://raw.githubusercontent.com/Cxbx-Reloaded/Cxbx-Reloaded/master/src/core/hle/kernel_exports.h",
    "https://raw.githubusercontent.com/Cxbx-Reloaded/Cxbx-Reloaded/master/src/core/hle/krnl_export.h",
    "https://raw.githubusercontent.com/Cxbx-Reloaded/Cxbx-Reloaded/master/src/core/hle/kernel.h"
]

for u in urls:
    try:
        print(f"Trying {u}")
        req = urllib.request.Request(u, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, context=ctx) as response:
            content = response.read().decode('utf-8')
            with open("krnl.txt", "w") as f:
                f.write(content)
            print("Downloaded!")
            break
    except Exception as e:
        print(f"Failed: {e}")
