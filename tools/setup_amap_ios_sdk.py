#!/usr/bin/env python3
"""Download pinned official, no-IDFA SDK packages; never commit vendor binaries or keys."""
from pathlib import Path
import hashlib
import shutil
import plistlib
import urllib.request
import zipfile
root = Path(__file__).resolve().parents[1]
ios = root / 'third_party/moto-gps-waveshare/platforms/ios'
cache = root / 'build/amap-sdk'
cache.mkdir(parents=True, exist_ok=True)
vendor = ios / 'Vendor/AMap'
vendor.mkdir(parents=True, exist_ok=True)
packages = [
 ('foundation','AMap_iOS_Foundation_NO_IDFA_Lib_V1.9.4.zip','e812db2c83e2542e8bcd1d6904e03d49fa1a2e20b33772e1c3f1ede14d5dd552'),
 ('search','AMap_iOS_Search_Lib_V9.8.1.zip','d11e0b418319c74abf5228611f678b58130eac791e267d6d6e3e7ec9f41b2007'),
 ('navi','AMap_iOS_Navi_Lib_V11.3.100.zip','b6704b432c0b6f9618c8e60d5ac82123b1715ea601bf8e1e0432057836713930'),
]
for name, archive, digest in packages:
    path = cache / (name + '.zip')
    if not path.exists(): urllib.request.urlretrieve('https://a.amap.com/lbs/static/zip/' + archive, path)
    if hashlib.sha256(path.read_bytes()).hexdigest() != digest: raise SystemExit('SDK checksum mismatch: ' + name)
    with zipfile.ZipFile(path) as z:
        if any('..' in Path(n).parts or Path(n).is_absolute() for n in z.namelist()): raise SystemExit('Unsafe SDK archive')
        z.extractall(vendor)
# Static frameworks are not embedded. Bundle each vendor privacy manifest explicitly.
privacy = [
 ('AMapFoundationKit-NO-IDFA','45d16718a40cc00d6845a6b8e2d98ec6793bf16df57cca75eac838dec6d15f6c'),
 ('AMapSearchKit','abbd5a0fa0ca13c2f2b9e247b549a03a55b66e5e8cade345f797c63ab99c24c3'),
 ('AMapNaviKit','16ed40f7fbf982ebc1302ecf858650575a659ba0c6ec18197a3472343266edd2'),
]
for name, digest in privacy:
    path = cache / (name + '.xcprivacy.zip')
    if not path.exists(): urllib.request.urlretrieve('https://a.amap.com/lbs/static/zip/' + path.name, path)
    if hashlib.sha256(path.read_bytes()).hexdigest() != digest: raise SystemExit('Privacy checksum mismatch: ' + name)
    bundle = vendor / (name + 'Privacy.bundle')
    bundle.mkdir(exist_ok=True)
    with zipfile.ZipFile(path) as z: (bundle / 'PrivacyInfo.xcprivacy').write_bytes(z.read(name + '.xcprivacy'))
    (bundle / 'Info.plist').write_bytes(plistlib.dumps({
        'CFBundleIdentifier':'com.autonavi.' + name + '.privacy', 'CFBundlePackageType':'BNDL',
        'CFBundleVersion':'1', 'CFBundleName':name + ' Privacy'}))
local = ios / 'Local.xcconfig'
if not local.exists(): shutil.copyfile(ios / 'Local.xcconfig.example', local)
print('Verified AMap SDKs installed in', vendor)
