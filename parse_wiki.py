from bs4 import BeautifulSoup
import re

with open("krnl_wiki.html", "r") as f:
    html = f.read()

soup = BeautifulSoup(html, "html.parser")
tables = soup.find_all("table")

for table in tables:
    rows = table.find_all("tr")
    for row in rows:
        cols = row.find_all(["td", "th"])
        if len(cols) >= 2:
            ordinal = cols[0].text.strip()
            name = cols[1].text.strip()
            if ordinal.isdigit():
                print(f"{ordinal}: {name}")
