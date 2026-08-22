#!/usr/bin/env python3
"""Build a compact PocketBook podcast catalogue from public RSS feeds."""
import hashlib, html, io, re, unicodedata, urllib.request
import xml.etree.ElementTree as ET
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from PIL import Image

FEEDS = [
    ("karlik", "Karlikovy minecrafticke pohadky", "https://anchor.fm/s/10958483c/podcast/rss", True),
    ("minecraft", "Minecraft pribehy na dobrou noc", "https://anchor.fm/s/101a549a0/podcast/rss", False),
    ("otazky", "Same otazky", "https://anchor.fm/s/ea8661c8/podcast/rss", False),
]
OUT = Path(__file__).parent / "public"

def clean(value, limit):
    value = re.sub(r"<[^>]+>", " ", html.unescape(value or ""))
    value = unicodedata.normalize("NFKD", value).encode("ascii", "ignore").decode()
    return re.sub(r"[|\r\n]+|\s+", " ", value).strip()[:limit]

def child_text(item, suffix):
    for node in item:
        if node.tag.split("}")[-1] == suffix and node.text: return node.text
    return ""

def image_url(item, fallback):
    for node in item.iter():
        tag = node.tag.split("}")[-1]
        if tag in ("image", "thumbnail") and (node.get("href") or node.get("url")):
            return node.get("href") or node.get("url")
    return fallback

def download_image(args):
    url, key = args
    if not url: return ""
    target = OUT / "images" / f"{key}.jpg"
    try:
        if target.exists() and target.stat().st_size < 120_000: return f"images/{key}.jpg"
        if target.exists(): raw=target.read_bytes()
        else:
            req=urllib.request.Request(url,headers={"User-Agent":"MeleysPocketBook/1.0"})
            raw=urllib.request.urlopen(req,timeout=25).read(8_000_001)
        if len(raw)>8_000_000: raise ValueError("image too large")
        with Image.open(io.BytesIO(raw)) as image:
            image.thumbnail((220,220)); image.convert("L").save(target,"JPEG",quality=72,optimize=True)
        return f"images/{key}.jpg"
    except Exception as exc:
        print(f"image {url}: {exc}"); target.unlink(missing_ok=True); return ""

def main():
    (OUT / "images").mkdir(parents=True,exist_ok=True); lines=[]
    for feed_id,name,url,hide_bonus in FEEDS:
        req=urllib.request.Request(url,headers={"User-Agent":"MeleysPocketBook/1.0"})
        root=ET.fromstring(urllib.request.urlopen(req,timeout=45).read())
        channel=root.find("channel"); fallback=""
        for node in channel:
            if node.tag.split("}")[-1]=="image": fallback=node.get("href") or (node.findtext("url") or fallback)
        items=[]
        for item in channel.findall("item"):
            title=clean(item.findtext("title"),150)
            if hide_bonus and "BONUS" in title.upper(): continue
            enclosure=item.find("enclosure"); audio=enclosure.get("url","") if enclosure is not None else ""
            if not audio.startswith(("http://","https://")): continue
            guid=clean(item.findtext("guid") or audio,200)
            key=hashlib.sha1(guid.encode()).hexdigest()[:16]
            date=clean(item.findtext("pubDate"),35)
            duration=clean(child_text(item,"duration"),12)
            items.append((key,title,date,duration,image_url(item,fallback),audio))
            if len(items)>=30: break
        with ThreadPoolExecutor(max_workers=10) as pool:
            pictures=list(pool.map(download_image,((row[4],row[0]) for row in items)))
        items=[(row[0],row[1],row[2],row[3],pictures[i],row[5]) for i,row in enumerate(items)]
        lines.append(f"P|{feed_id}|{name}|{len(items)}")
        lines.extend("E|"+feed_id+"|"+"|".join(row) for row in items)
        print(feed_id,len(items))
    used={line.split("|")[2]+".jpg" for line in lines if line.startswith("E|")}
    for old in (OUT/"images").glob("*.jpg"):
        if old.name not in used: old.unlink()
    temp=OUT/"catalog.tmp"; temp.write_text("\n".join(lines)+"\n",encoding="utf-8"); temp.replace(OUT/"catalog.dat")
if __name__=="__main__": main()
