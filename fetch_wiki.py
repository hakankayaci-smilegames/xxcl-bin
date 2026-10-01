import urllib.request
import ssl

ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

url = "https://xboxdevwiki.net/Kernel"
try:
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, context=ctx) as response:
        content = response.read().decode('utf-8')
        with open("krnl_wiki.html", "w") as f:
            f.write(content)
        print("Downloaded Wiki!")
except Exception as e:
    print(f"Failed: {e}")
