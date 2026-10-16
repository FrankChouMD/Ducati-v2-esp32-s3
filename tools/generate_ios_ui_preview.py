"""Rebuild the offline HTML design preview from the native UI's theme and fonts.

This is a review artifact, not an iOS renderer. Map geometry comes from the
imported OSM demo fixture; the native app continues to use Apple MapKit.
"""
from pathlib import Path
from html import escape
import base64
import json
import math
import re

ROOT = Path(__file__).resolve().parents[1]
IOS = ROOT / "third_party/moto-gps-waveshare/platforms/ios"
OUT = ROOT / "docs/previews/ducati-ios-ui.html"


def data_font(name):
    data = base64.b64encode((IOS / "App/Fonts" / name).read_bytes()).decode()
    return f"url(data:font/ttf;base64,{data}) format('truetype')"


def map_svg():
    fixture = json.loads((ROOT / "third_party/moto-gps-waveshare/shared/demo_fixture/jinan_big_data_to_inspur.json").read_text(encoding="utf-8"))
    route = fixture["route_wgs84"]
    # Local equirectangular projection, adequate for this ~1.5 km fixture.
    cos_lat = math.cos(math.radians(route[0][0]))
    points = [(lon * cos_lat, -lat) for lat, lon in route]
    x0, x1 = min(x for x, y in points), max(x for x, y in points)
    y0, y1 = min(y for x, y in points), max(y for x, y in points)
    scale = min(290 / (x1 - x0), 188 / (y1 - y0))
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2

    def project(p):
        lat, lon = p
        return ((lon * cos_lat - cx) * scale + 195, (-lat - cy) * scale + 120)

    def path(coords):
        return "M" + " L".join(f"{x:.2f},{y:.2f}" for x, y in map(project, coords))

    roads = "".join(f'<path d="{path(road["points_wgs84"])}" class="road"/>' for road in fixture["road_context"])
    start, end = project(route[0]), project(route[-1])
    return f'''<svg viewBox="0 0 390 250" class="route-map" role="img" aria-label="济南演示路线，真实 OSM 道路几何">
      <defs><pattern id="map-grid" width="24" height="24" patternUnits="userSpaceOnUse"><path d="M24 0H0V24" fill="none" stroke="#202124" stroke-width=".45"/></pattern></defs>
      <rect width="390" height="250" fill="#101114"/><rect width="390" height="250" fill="url(#map-grid)"/>
      <g>{roads}</g><path d="{path(route)}" fill="none" stroke="#000" stroke-width="11" stroke-linejoin="round"/>
      <path d="{path(route)}" class="selected-route"/>
      <circle cx="{start[0]:.1f}" cy="{start[1]:.1f}" r="7" fill="white" stroke="#101114" stroke-width="3"/>
      <circle cx="{end[0]:.1f}" cy="{end[1]:.1f}" r="9" fill="var(--accent)" stroke="#101114" stroke-width="3"/>
      <text x="14" y="232" fill="#a3a3aa" font-size="9">预览底图 · © OpenStreetMap contributors</text>
      <text x="14" y="22" fill="#a3a3aa" font-size="10">济南 · 高新区</text></svg>'''


def main():
    theme = (IOS / "App/DucatiTheme.swift").read_text(encoding="utf-8")
    colors = re.findall(r"static let (\w+) = Color\(hex: 0x([A-Fa-f0-9]{6})\)", theme)
    tokens = ";".join(f"--{key}:#{value}" for key, value in colors)
    html = TEMPLATE.replace("__TOKENS__", tokens).replace("__FONT_LABEL__", data_font("BarlowCondensed-Medium.ttf"))
    html = html.replace("__FONT_NUMBER__", data_font("BarlowCondensed-SemiBoldItalic.ttf"))
    html = html.replace("__MAP__", map_svg())
    html = html.replace("__FONT_LICENSE__", escape((IOS / "App/Fonts/OFL.txt").read_text(encoding="utf-8")))
    OUT.write_text(html, encoding="utf-8")
    print(f"Generated {OUT.relative_to(ROOT)} ({OUT.stat().st_size} bytes)")


TEMPLATE = r'''<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ducati V2 · iPhone 界面修改预览</title><link rel="icon" href="data:,">
<style>
@font-face{font-family:Barlow;src:__FONT_LABEL__} @font-face{font-family:BarlowNumbers;src:__FONT_NUMBER__}
:root{__TOKENS__;color-scheme:dark;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI","Microsoft YaHei UI",sans-serif;color:var(--primary);background:#111113}
*{box-sizing:border-box}[hidden]{display:none!important}body{margin:0}button,input{font:inherit}button{cursor:pointer;color:inherit;border:0;background:none;padding:0;text-align:left}button:focus-visible,input:focus-visible{outline:2px solid var(--primary);outline-offset:4px}svg.icon{width:22px;height:22px;fill:none;stroke:currentColor;stroke-width:1.5;stroke-linecap:round;stroke-linejoin:round;flex-shrink:0}.red{color:var(--accent)}.muted{color:var(--secondary)}
.board{max-width:1506px;padding:44px 44px 28px;margin:auto}.board-header{display:flex;justify-content:space-between;align-items:end;margin-bottom:32px;border-bottom:1px solid #333337;padding-bottom:24px}.board-title{font:56px/1 BarlowNumbers;margin:0 0 9px}.board-header p{font-size:13px;letter-spacing:1px;color:var(--secondary);margin:0}.edition{font:15px Barlow;letter-spacing:2px;display:flex;align-items:center;gap:10px}.edition:before{content:"";width:18px;height:3px;background:var(--accent)}
.gallery{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:32px;align-items:start}.column-label{display:flex;justify-content:space-between;font-size:14px;margin:0 2px 14px}.column-label span{font:13px Barlow;letter-spacing:1.6px;color:var(--secondary)}
.phone{height:932px;position:relative;background:var(--background);border:1px solid #3a3a40;border-radius:38px;overflow:hidden;display:flex;flex-direction:column;box-shadow:0 24px 50px #0004}.statusbar{height:44px;flex-shrink:0;display:flex;align-items:center;justify-content:space-between;padding:0 27px;font:600 13px system-ui}.island{width:100px;height:25px;position:absolute;left:50%;transform:translateX(-50%);top:9px;background:#171719;border-radius:20px}.status-symbols{display:flex;gap:5px;align-items:center}.battery{width:23px;height:11px;border:1px solid #aaa;border-radius:3px;position:relative;padding:1px}.battery:before{content:"";display:block;width:88%;height:100%;background:#fff;border-radius:1px}.battery:after{content:"";width:2px;height:5px;background:#aaa;position:absolute;right:-4px;top:2px}
.toolbar{height:44px;flex-shrink:0;display:flex;justify-content:space-between;align-items:center;padding:0 16px;font-size:16px}.toolbar>button{min-height:44px;min-width:44px;display:flex;align-items:center;justify-content:center}.toolbar .back{justify-content:flex-start;gap:4px;font-size:13px}.toolbar strong{font-size:16px;font-weight:600}.content{padding:18px 20px 28px;overflow-y:auto;scrollbar-width:none;flex:1}.content::-webkit-scrollbar{display:none}.stack{display:flex;flex-direction:column;gap:22px}.brand{display:flex;gap:10px;align-items:center}.brand:before{content:"";width:4px;height:27px;background:var(--accent);border-radius:1px}.brand strong{display:block;font:18px/1.1 Barlow;letter-spacing:2.2px}.brand small{display:block;font:10px Barlow;letter-spacing:2px;color:var(--secondary);margin-top:2px}.hero h2{font-size:34px;line-height:1.28;margin:0 0 10px;font-weight:700;letter-spacing:-1px}.hero p{font-size:14px;color:var(--secondary);line-height:1.55;margin:0}
.card{padding:18px;background:var(--surface);border:1px solid var(--border);border-radius:18px}.search{display:flex;align-items:center;gap:12px;min-height:60px}.search input{width:100%;min-width:0;color:#fff;background:none;border:0;outline:0;font-size:16px}.search input::placeholder{color:#85858d}.device{width:100%;display:flex;align-items:center;gap:14px;min-height:78px}.device>.icon{width:36px;height:32px}.device-text{flex:1;min-width:0}.device-text strong{font-size:14px;font-weight:600}.device-text small{display:block;font-size:11px;color:var(--secondary);line-height:1.5;margin-top:5px}.badge{display:inline-flex;align-items:center;gap:6px;background:#a6d80015;color:var(--ready);font-size:11px;padding:7px 10px;border-radius:100px;white-space:nowrap}.badge:before{content:"";width:6px;height:6px;background:currentColor;border-radius:50%}.badge.gray{background:#a3a3aa15;color:var(--secondary)}.badge.yellow{background:#ffc40015;color:var(--warning)}
.section-heading{display:flex;align-items:baseline;justify-content:space-between;margin-bottom:12px;padding-top:4px}.section-heading strong{font-size:14px;font-weight:600}.section-heading span{font:13px Barlow;letter-spacing:1.6px;color:var(--secondary)}.place{display:flex;align-items:center;gap:12px;min-height:76px;width:100%;padding:12px 0}.place-body{flex:1;min-width:0}.place-body strong{font-size:15px;font-weight:500}.place-body small{display:block;font-size:11px;color:var(--secondary);margin-top:5px;line-height:1.5}.place .chev{width:13px;color:var(--secondary)}.recent .card{padding-top:6px;padding-bottom:6px}.section-heading button{font-size:11px;color:var(--secondary);min-height:44px;margin-top:-12px;margin-bottom:-12px;min-width:44px;text-align:right}
.tools{display:grid;grid-template-columns:1fr 1fr;gap:12px}.tool{min-height:120px;display:flex;flex-direction:column;gap:14px}.tool .icon{width:23px;height:23px;color:var(--accent)}.tool strong{font-size:14px}.tool small{display:block;font-size:11px;color:var(--secondary);margin-top:5px;line-height:1.5}.utility{display:flex;gap:22px}.utility button{display:flex;align-items:center;gap:5px;min-height:44px;font-size:11px;color:var(--secondary)}.utility .icon{width:13px;height:13px}.credit{font:11px Barlow;letter-spacing:1.4px;color:var(--secondary);margin-top:-10px}.homebar{height:28px;flex-shrink:0;display:flex;justify-content:center;align-items:center}.homebar:before{content:"";width:120px;height:4px;border-radius:4px;background:white}.gateway-notice{font-size:12px;display:flex;gap:12px;align-items:start}.gateway-notice strong{display:block;font-size:14px}.gateway-notice small{display:block;line-height:1.6;color:var(--secondary);margin-top:5px}
.destination .origin{font-size:11px;color:var(--secondary);display:flex;gap:6px;align-items:center}.origin .icon{width:12px;height:12px}.destination h2{font-size:26px;line-height:1.35;margin:8px 0;font-weight:700}.destination p{font-size:11px;line-height:1.6;color:var(--secondary);margin:0}.map-card{height:250px;border:1px solid var(--border);border-radius:18px;overflow:hidden}.route-map{display:block;width:100%;height:100%;background:#101114}.road{stroke:#383a40;stroke-width:4;fill:none;stroke-linejoin:round;stroke-linecap:round}.selected-route{stroke:var(--accent);stroke-width:6;fill:none;stroke-linejoin:round;stroke-linecap:round}.route-content{gap:20px;padding-top:20px}.route-option{display:flex;align-items:center;gap:12px;width:100%;margin-bottom:10px}.route-option.selected{border-color:var(--accent);background:#e8001617}.route-option>div{flex:1}.route-option strong{display:block;font-size:11px;font-weight:600;margin-bottom:7px}.route-option .duration{font:34px/1.1 BarlowNumbers}.route-option .distance{font:20px Barlow;color:var(--secondary);margin-left:12px}.route-option .traffic{font-size:11px;color:var(--ready);margin-top:8px;display:flex;gap:6px;align-items:center}.traffic .icon{width:13px;height:13px}.selection{color:var(--accent);width:21px;height:21px;flex-shrink:0}.selection.off{color:var(--border)}.route-note{font-size:10px;color:var(--secondary);line-height:1.5}.download-route{min-height:44px;display:flex;align-items:center;gap:12px;font-size:13px}.download-route .chev{width:12px;color:var(--secondary);margin-left:auto}.actionbar{padding:14px 20px 10px;border-top:1px solid var(--border);background:#000;flex-shrink:0}.primary{width:100%;min-height:54px;border-radius:16px;background:var(--accent);font-weight:600;font-size:16px;display:flex;align-items:center;justify-content:center;gap:10px}.primary .icon{width:17px;height:17px}
.nav-hero{display:flex;flex-direction:column;gap:18px}.nav-hero .hero-top{display:flex;align-items:center;justify-content:space-between}.hero-top>.icon{width:30px;height:30px;color:var(--accent)}.nav-hero h2{font-size:34px;line-height:1.25;margin:0;font-weight:700}.nav-hero p{font-size:14px;line-height:1.6;margin:0;color:var(--secondary)}.active-destination .dest-name{display:flex;align-items:center;gap:9px;font-size:18px;font-weight:600;line-height:1.5}.active-destination .icon{width:20px;height:20px}.metrics{border:0;border-top:2px solid var(--accent);padding:20px;display:flex;gap:20px}.metric{flex:1;min-width:0}.metric strong{display:block;font:56px/1.05 BarlowNumbers;letter-spacing:-1px;overflow-wrap:anywhere}.metric small{display:block;font-size:11px;color:var(--secondary);margin-top:6px}.status-rows{margin-top:12px}.status-row{display:flex;align-items:center;gap:9px;font-size:13px;min-height:43px}.status-row .icon{width:17px;height:17px}.status-row span:last-child{margin-left:auto;color:var(--secondary);text-align:right}.status-row+.status-row{border-top:1px solid var(--border)}.status-row span.good{color:var(--ready)}.manage-map{font-size:13px;color:var(--accent);min-height:44px;margin-top:8px}
.stack>*{flex-shrink:0}.duration .unit,.metric strong .unit{font:14px Barlow;color:var(--secondary);margin-left:4px}.metric strong .unit{font-size:16px;display:inline}.route-option .traffic.muted{color:var(--secondary)}
.board-footer{display:flex;justify-content:space-between;gap:20px;margin-top:28px;color:var(--secondary);font-size:12px;line-height:1.8}.board-footer strong{color:#ddd;font-weight:500}.sheet{display:none;position:absolute;inset:70px 0 0;z-index:10;border-top:1px solid var(--border);border-radius:26px 26px 0 0;background:#000;padding:20px;overflow:auto}.sheet.open{display:block}.sheet header{display:flex;justify-content:space-between;align-items:center;margin-bottom:25px}.sheet header button{color:var(--accent);min-height:44px;min-width:44px;text-align:right}.sheet h3{font-size:23px}.sheet p{color:var(--secondary);font-size:13px;line-height:1.8}.sheet .device-art{display:flex;justify-content:center;padding:30px}.device-art svg{width:100px;height:100px;padding:20px;border:1px solid var(--accent);border-radius:24px}.sheet .primary{margin-top:22px}.sheet input{width:100%;background:var(--surface);color:white;padding:18px;border-radius:14px;border:1px solid var(--border);font-size:15px}.sheet .form-row{padding:17px 0;border-bottom:1px solid var(--border);font-size:14px}.preview-message{font-size:11px;color:var(--warning);line-height:1.7;margin-top:15px}.single .board{padding:0;max-width:none}.single .board-header,.single .board-footer,.single .column-label{display:none}.single .gallery{display:block}.single article{display:none}.single article.visible{display:block}.single .phone{width:100%;height:100dvh;border:0;border-radius:0;box-shadow:none}.single .content{padding-left:20px;padding-right:20px}.search-results{display:none}.search-results.visible{display:block}.setup-only{display:none}
@media(max-width:1100px){.gallery{grid-template-columns:1fr;max-width:430px;margin:auto}.board-header{align-items:start;gap:20px}.board-title{font-size:42px}.edition{font-size:12px;letter-spacing:1px}.board-footer{display:block}.phone{width:100%}.board{padding:25px 16px}.board-footer span{display:block}}@media(max-width:360px){.device{flex-wrap:wrap}.badge{font-size:10px}.metric strong{font-size:46px}.hero h2{font-size:32px}}@media(prefers-reduced-motion:no-preference){.primary,.route-option{transition:filter .15s}.primary:active{filter:brightness(.75)}}
</style></head><body>
<svg aria-hidden="true" style="display:none"><defs>
<symbol id="search" viewBox="0 0 24 24"><circle cx="10.5" cy="10.5" r="6.5"/><path d="m16 16 5 5"/></symbol>
<symbol id="device" viewBox="0 0 24 24"><rect x="4" y="3" width="16" height="16" rx="4"/><path d="M9 22h6M12 19v3M9 10l3-3 3 3M12 7v8"/></symbol>
<symbol id="gear" viewBox="0 0 24 24"><path d="m10 3-1 3-3 1-3 3 2 2-1 4 3 3 3-1 2 3 4-1 1-3 3-1 1-4-3-2-1-3-4 1z"/><circle cx="12" cy="12" r="3"/></symbol>
<symbol id="map" viewBox="0 0 24 24"><path d="m3 5 6-2 6 2 6-2v16l-6 2-6-2-6 2zM9 3v16M15 5v16"/></symbol>
<symbol id="play" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/><path d="m10 8 6 4-6 4z"/></symbol>
<symbol id="clock" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/></symbol>
<symbol id="chev" viewBox="0 0 24 24"><path d="m9 5 7 7-7 7"/></symbol>
<symbol id="back" viewBox="0 0 24 24"><path d="m15 5-7 7 7 7"/></symbol>
<symbol id="location" viewBox="0 0 24 24"><path d="m12 3 8 17-8-4-8 4z"/></symbol>
<symbol id="flag" viewBox="0 0 24 24"><path d="M5 21V3l7 2 7-2v11l-7 2-7-2"/><path d="m9 5 0 9m5-8v8M5 9l7 2 7-2"/></symbol>
<symbol id="car" viewBox="0 0 24 24"><path d="m5 11 2-6h10l2 6M4 11h16v7H4zM7 18v2m10-2v2M7 14h1m8 0h1"/></symbol>
<symbol id="network" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><ellipse cx="12" cy="12" rx="4" ry="9"/><path d="M3 12h18M5 7h14M5 17h14"/></symbol>
<symbol id="privacy" viewBox="0 0 24 24"><path d="m12 3 8 3v6c0 5-8 9-8 9s-8-4-8-9V6zM9 12l2 2 4-4"/></symbol>
<symbol id="download" viewBox="0 0 24 24"><path d="M12 3v12m-5-5 5 5 5-5M4 17v4h16v-4"/></symbol>
<symbol id="stop" viewBox="0 0 24 24"><rect x="5" y="5" width="14" height="14" rx="2" fill="currentColor" stroke="none"/></symbol>
<symbol id="check" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10" fill="currentColor"/><path d="m7 12 3 3 7-7" stroke="white" fill="none" stroke-width="2"/></symbol>
</defs></svg>
<main class="board"><header class="board-header"><div><h1 class="board-title">PANIGALE V2S</h1><p>手机伴侣 · 与车机一致的黑、白、红仪表语言</p></div><div class="edition">PHONE UI / 2026.10</div></header>
<div class="gallery">
<article data-screen="home"><div class="column-label">01 / 出发<span>DESTINATION</span></div><div class="phone">
<div class="statusbar"><span>9:41</span><i class="island"></i><div class="status-symbols">▮▮▮ <span>◔</span><i class="battery"></i></div></div>
<nav class="toolbar"><button aria-label="网关设置" data-sheet="gateway"><svg class="icon"><use href="#gear"/></svg></button><strong>出发</strong><button aria-label="我的车机" data-sheet="device"><svg class="icon"><use href="#device"/></svg></button></nav>
<div class="content stack"><div class="brand"><div><strong>PANIGALE V2S</strong><small>PHONE COMPANION</small></div></div>
<div class="hero"><h2>下一程，<br>即刻出发。</h2><p>选好路线，把前方交给车机。</p></div>
<label class="card search"><svg class="icon red"><use href="#search"/></svg><input placeholder="搜索地点或地址" aria-label="搜索目的地" id="search-input"></label>
<div class="search-results" id="results"><div class="section-heading"><strong>搜索结果</strong><span>DESTINATION</span></div><button class="card place" data-go="route"><svg class="icon red"><use href="#location"/></svg><div class="place-body"><strong>浪潮集团（总部）</strong><small>济南市 · 高新区 · 浪潮路</small></div><svg class="icon chev"><use href="#chev"/></svg></button><p class="route-note">效果预览 · 示例搜索结果</p></div>
<button class="card gateway-notice setup-only" data-sheet="gateway"><svg class="icon" style="color:var(--warning)"><use href="#network"/></svg><div><strong>设置导航网关</strong><small>填写网关地址，开启地点搜索与路线规划。</small></div></button>
<button class="card device home-default" data-sheet="device"><svg class="icon"><use href="#device"/></svg><div class="device-text"><strong>我的车机</strong><small>导航同步已就绪</small></div><span class="badge">已连接</span></button>
<section class="recent home-default"><div class="section-heading"><strong>最近搜索</strong><button id="clear-recent">清空</button></div><div class="card" id="recent"><button class="place" data-go="route"><svg class="icon muted"><use href="#clock"/></svg><div class="place-body"><strong>浪潮集团（总部）</strong><small>济南市 · 高新区 · 浪潮路</small></div><svg class="icon chev"><use href="#chev"/></svg></button></div></section>
<section class="home-default"><div class="section-heading"><strong>骑行工具</strong><span>TOOLS</span></div><div class="tools"><button class="card tool" data-sheet="maps"><svg class="icon"><use href="#map"/></svg><div><strong>离线地图</strong><small>提前保存沿途地图</small></div></button><button class="card tool" id="demo" data-go="navigation"><svg class="icon"><use href="#play"/></svg><div><strong>演示导航</strong><small>先体验一次导航</small></div></button></div></section>
<div class="utility home-default"><button data-sheet="gateway"><svg class="icon"><use href="#gear"/></svg>网关设置</button><button data-sheet="privacy"><svg class="icon"><use href="#privacy"/></svg>隐私与数据</button></div><div class="credit home-default">POWERED BY MOTO GPS · MALER X</div>
</div><div class="homebar"></div></div></article>
<article data-screen="route"><div class="column-label">02 / 选择路线<span>ROUTE SELECTION</span></div><div class="phone">
<div class="statusbar"><span>9:41</span><i class="island"></i><div class="status-symbols">▮▮▮ <span>◔</span><i class="battery"></i></div></div>
<nav class="toolbar"><button class="back" data-go="home"><svg class="icon"><use href="#back"/></svg>出发</button><strong>路线</strong><button data-go="home">更换</button></nav>
<div class="content stack route-content"><div class="brand"><div><strong>PANIGALE V2S</strong><small>ROUTE SELECTION</small></div></div>
<div class="destination"><div class="origin"><svg class="icon"><use href="#location"/></svg>从我的位置出发</div><h2>浪潮集团（总部）</h2><p>济南市 · 高新区 · 浪潮路</p></div>
<div class="map-card">__MAP__</div>
<section><div class="section-heading"><strong>选择路线</strong><span>SELECT ROUTE</span></div>
<button class="card route-option selected" data-route="0"><div><strong>推荐路线</strong><span class="duration">3<span class="unit">分钟</span></span><span class="distance">1.6 km</span><div class="traffic muted"><svg class="icon"><use href="#car"/></svg>暂无分段路况</div></div><svg class="icon selection"><use href="#check"/></svg></button>
<button class="card route-option" data-route="1"><div><strong>备选路线一</strong><span class="duration">5<span class="unit">分钟</span></span><span class="distance">2.1 km</span><div class="traffic"><svg class="icon"><use href="#car"/></svg>路况顺畅</div></div><svg class="icon selection off"><circle cx="12" cy="12" r="9"/></svg></button>
<p class="route-note">高德驾车路线 · 预计时间会随路况变化</p></section>
<button class="download-route" data-sheet="maps"><svg class="icon red"><use href="#download"/></svg>下载沿途离线地图<svg class="icon chev"><use href="#chev"/></svg></button>
</div><div class="actionbar"><button class="primary" id="start" data-go="navigation"><svg class="icon"><use href="#location"/></svg>开始导航</button></div><div class="homebar"></div></div></article>
<article data-screen="navigation"><div class="column-label">03 / 导航中<span>RIDE IN PROGRESS</span></div><div class="phone">
<div class="statusbar"><span>9:41</span><i class="island"></i><div class="status-symbols">▮▮▮ <span>◔</span><i class="battery"></i></div></div>
<nav class="toolbar"><button class="back" data-go="home"><svg class="icon"><use href="#back"/></svg>出发</button><strong id="nav-title">导航中</strong><button data-sheet="device" aria-label="我的车机"><svg class="icon"><use href="#device"/></svg></button></nav>
<div class="content stack route-content"><div class="brand"><div><strong>PANIGALE V2S</strong><small id="session-label">RIDE IN PROGRESS</small></div></div>
<div class="card nav-hero"><div class="hero-top"><svg class="icon"><use href="#location"/></svg><span class="badge" id="nav-badge">已获取</span></div><h2 id="nav-headline">正在导航</h2><p id="nav-description">车机已连接，手机可锁屏收好。</p></div>
<section class="active-destination"><div class="section-heading"><strong>目的地</strong><span>DESTINATION</span></div><div class="dest-name"><svg class="icon"><use href="#flag"/></svg><span id="nav-destination">浪潮集团（总部）</span></div></section>
<div class="card metrics"><div class="metric"><strong id="nav-time">3<span class="unit">分钟</span></strong><small>剩余时间</small></div><div class="metric"><strong id="nav-distance">1.6<span class="unit">km</span></strong><small>剩余路程</small></div></div>
<section><div class="section-heading"><strong>导航状态</strong><span>LIVE STATUS</span></div><button class="card device" data-sheet="device"><svg class="icon"><use href="#device"/></svg><div class="device-text"><strong>我的车机</strong><small>导航同步已就绪</small></div><span class="badge">已连接</span></button>
<div class="card status-rows"><div class="status-row"><svg class="icon"><use href="#location"/></svg><span>手机定位</span><span class="good" id="position-value">已获取</span></div><div class="status-row"><svg class="icon"><use href="#car"/></svg><span>路况</span><span>在线</span></div><div class="status-row"><svg class="icon"><use href="#map"/></svg><span>周边地图</span><span>已加载</span></div></div><button class="manage-map" data-sheet="maps">管理离线地图</button></section>
<p class="preview-message" id="demo-warning" hidden>演示中的位置与行驶过程为模拟数据。<br>道路数据 © OpenStreetMap contributors</p>
</div><div class="actionbar"><button class="primary" id="end" data-go="home"><svg class="icon"><use href="#stop"/></svg>结束导航</button></div><div class="homebar"></div></div></article>
</div><footer class="board-footer"><span><strong>界面效果预览 · 示例数据</strong>　根据已修改的 SwiftUI 布局重绘，非 iOS 模拟器截图。<br>原生地图使用 MapKit；此图使用仓库内的 OSM 演示路网。备选路线数值用于展示选择状态。</span><span>纯黑底色 / DUCATI RED #E80016 / Barlow Condensed<br>现有导航、蓝牙、地图下载流程 · 手机倾角回放尚未实现</span></footer></main>
<template id="font-license">__FONT_LICENSE__</template><script>
const params=new URLSearchParams(location.search),single=params.has('screen');
function go(screen){document.querySelectorAll('[data-screen]').forEach(a=>a.classList.toggle('visible',a.dataset.screen===screen));if(single){history.replaceState(null,'','?screen='+screen)}else{document.querySelector('[data-screen="'+screen+'"]').scrollIntoView({behavior:'smooth',block:'nearest'})}}
if(single){document.body.classList.add('single');go(params.get('screen'))}
document.querySelectorAll('[data-go]').forEach(b=>b.addEventListener('click',()=>{if(b.id==='demo')setDemo(true);if(b.id==='start')setDemo(false);if(b.dataset.go==='home'){inp.value='';inp.dispatchEvent(new Event('input'))}go(b.dataset.go)}));
const inp=document.getElementById('search-input');inp.addEventListener('input',()=>{const active=!!inp.value;document.getElementById('results').classList.toggle('visible',active);document.querySelectorAll('.home-default').forEach(e=>e.hidden=active)});
document.getElementById('clear-recent').onclick=()=>{document.getElementById('recent').innerHTML='<div class="place"><div class="place-body"><strong>下一站，想去哪里？</strong><small>搜索地点或地址，选好路线就出发。</small></div></div>';document.getElementById('clear-recent').hidden=true};
document.querySelectorAll('[data-route]').forEach(b=>b.onclick=()=>{document.querySelectorAll('[data-route]').forEach(r=>{const selected=r===b;r.classList.toggle('selected',selected);r.querySelector('.selection').innerHTML=selected?'<use href="#check"/>':'<circle cx="12" cy="12" r="9"/>';r.querySelector('.selection').classList.toggle('off',!selected)});document.getElementById('nav-time').innerHTML=(b.dataset.route==='0'?'3':'5')+'<span class="unit">分钟</span>';document.getElementById('nav-distance').innerHTML=(b.dataset.route==='0'?'1.6':'2.1')+'<span class="unit">km</span>'});
function setDemo(demo){document.getElementById('nav-title').textContent=demo?'演示导航':'导航中';document.getElementById('session-label').textContent=demo?'DEMO SESSION':'RIDE IN PROGRESS';document.getElementById('nav-headline').textContent=demo?'正在演示':'正在导航';document.getElementById('nav-badge').textContent=demo?'模拟数据':'已获取';document.getElementById('nav-badge').classList.toggle('yellow',demo);document.getElementById('nav-destination').textContent=demo?'MOTO GPS 演示路线':'浪潮集团（总部）';document.getElementById('nav-description').textContent=demo?'预览行驶过程与车机上的导航指引。':'车机已连接，手机可锁屏收好。';document.getElementById('position-value').textContent=demo?'演示位置':'已获取';document.getElementById('demo-warning').hidden=!demo}
function sheet(kind,phone){phone.querySelector('.sheet')?.remove();const s=document.createElement('section');s.className='sheet open';s.setAttribute('role','dialog');s.setAttribute('aria-modal','true');const names={device:'我的车机',maps:'地图',gateway:'网关设置',privacy:'隐私与数据'};s.innerHTML='<header><strong>'+names[kind]+'</strong><button aria-label="关闭">完成</button></header>';s.querySelector('button').onclick=()=>s.remove();const content={device:'<div class="brand"><div><strong>PANIGALE V2S</strong><small>CONNECTED DISPLAY</small></div></div><div class="device-art"><svg class="icon"><use href="#device"/></svg></div><h3>MOTO GPS</h3><span class="badge">已连接</span><p>车机已就绪，可以接收导航指引。</p><div class="card"><div class="status-row"><span>连接状态</span><span class="good">已连接</span></div><div class="status-row"><span>搜索位置</span><span>已获取</span></div></div><button class="primary" data-preview="连接操作需要 iPhone 与真实车机，此处仅展示 UI。">断开连接</button>',maps:'<div class="card"><strong>自动加载周边地图</strong><p>骑行时自动加载车机周边的道路和建筑。</p></div><div class="form-row">下载城市地图　›</div><div class="form-row">下载这条路线周边　›</div><h3>已保存</h3><p>还没有下载城市或路线地图</p><div class="form-row">济南基础地图 · 随 App 提供</div><div class="form-row">清理自动缓存</div><p>离线底图不包含实时路况，规划新路线仍需要网络。</p>',gateway:'<p>网关地址</p><input aria-label="网关地址" placeholder="https://你的网关地址/"><p>填写完整的 HTTPS 根地址，不要填高德 Key。</p><button class="primary" data-preview="效果预览不访问真实网关。">测试连接</button><p>保存后立即生效，无需重新安装。此预览不会保存或发送你的输入。</p>',privacy:'<h3>位置与路线</h3><p>地点搜索和路线规划使用配置的导航服务，所需位置经网关发送至高德。</p><h3>蓝牙与音乐</h3><p>导航、速度与地图通过蓝牙同步至车机，Apple Music 按授权同步媒体状态。</p><h3>存储与清理</h3><p>最近地点和下载地图保存在手机。原生 App 提供完整的隐私说明与第三方服务链接。</p>'};s.insertAdjacentHTML('beforeend',content[kind]+'<p class="preview-message">界面效果预览 · 示例数据；原生功能需在 iPhone 中验证。</p>');s.querySelectorAll('[data-preview]').forEach(b=>b.onclick=()=>{s.querySelector('.preview-message').textContent=b.dataset.preview});phone.append(s)}
document.querySelectorAll('[data-sheet]').forEach(b=>b.onclick=()=>sheet(b.dataset.sheet,b.closest('.phone')));
if(params.get('state')==='setup'){document.querySelector('.setup-only').style.display='flex';const dev=document.querySelector('[data-screen="home"] .device');dev.querySelector('small').textContent='连接后自动同步导航';dev.querySelector('.badge').textContent='未连接';dev.querySelector('.badge').classList.add('gray');document.getElementById('clear-recent').click()}
</script></body></html>'''


if __name__ == "__main__":
    main()
