# SPDX-License-Identifier: MIT
# PrufLock (https://github.com/Lockedsub/pruflock)
# Author: LockedSub, Zürich, Switzerland
# Names the objects of a 3MF exported by OpenSCAD, and joins objects into one with several parts.

# Usage: python3 3mf_parts.py FILE.3mf NAME...
# Each NAME is for the next object, in the order OpenSCAD wrote them ("OpenSCAD Model 1", 2, …).
# "Lid+Logo" takes two objects and makes them one object "Lid" with the parts "Lid" and "Logo",
# a standard 3MF component object. For Bambu Studio (and likely OrcaSlicer), which name such parts after
# the object, Metadata/model_settings.config names the parts and gives part n filament n; other
# slicers ignore that file. Also drops the CreationDate and makes the UUIDs stable, so the file
# only changes when the model does.

import re, sys, uuid, zipfile

path, specs = sys.argv[1], sys.argv[2:]
MODEL = "3D/3dmodel.model"
with zipfile.ZipFile(path) as z:
    files = {name: z.read(name) for name in z.namelist()}
xml = files[MODEL].decode()

xml = re.sub(r'\s*<metadata name="CreationDate"[^>]*>[^<]*</metadata>', "", xml)

objects = re.findall(r'<object id="(\d+)" name="OpenSCAD Model \d+"', xml)
names = [n for spec in specs for n in spec.split("+")]
if len(objects) != len(names):
    sys.exit(f"{path}: {len(objects)} objects, but {len(names)} names given")
ids = dict(zip(names, objects))
for name, oid in ids.items():
    xml = re.sub(rf'(<object id="{oid}" name=")OpenSCAD Model \d+"', rf'\g<1>{name}"', xml)

settings = []
next_id = max(int(i) for i in re.findall(r' id="(\d+)"', xml)) + 1
for spec in specs:
    parts = spec.split("+")
    if len(parts) == 1:
        continue
    gid, next_id = next_id, next_id + 1
    comps = "".join(f'\n\t\t\t\t<component objectid="{ids[p]}" p:UUID=""/>' for p in parts)
    group = (f'\t<object id="{gid}" name="{parts[0]}" type="model" p:UUID="">'
             f'\n\t\t\t<components>{comps}\n\t\t\t</components>\n\t\t</object>\n\t')
    settings.append(f'  <object id="{gid}">\n    <metadata key="name" value="{parts[0]}"/>\n'
                    + "".join(f'    <part id="{ids[p]}" subtype="normal_part">\n'
                              f'      <metadata key="name" value="{p}"/>\n'
                              f'      <metadata key="extruder" value="{n}"/>\n    </part>\n'
                              for n, p in enumerate(parts, 1))
                    + "  </object>\n")
    xml = xml.replace("</resources>", group + "</resources>", 1)
    # the parts leave the build; the group takes the place of the first one
    first = True
    for p in parts:
        item = re.search(rf'\s*<item objectid="{ids[p]}"[^>]*/>', xml)
        new = f'\n\t\t<item objectid="{gid}" p:UUID=""/>' if first else ""
        xml = xml[:item.start()] + new + xml[item.end():]
        first = False

# OpenSCAD's UUIDs are random; number them instead
counter = iter(range(1, 1000000))
xml = re.sub(r'p:UUID="[^"]*"', lambda m: f'p:UUID="{uuid.uuid5(uuid.NAMESPACE_URL, f"pruflock:{next(counter)}")}"', xml)
files[MODEL] = xml.encode()

if settings:
    files["Metadata/model_settings.config"] = (
        '<?xml version="1.0" encoding="UTF-8"?>\n<config>\n' + "".join(settings) + "</config>\n").encode()
    types = files["[Content_Types].xml"].decode()
    if 'Extension="config"' not in types:
        types = types.replace("</Types>", '\t<Default Extension="config" ContentType="text/xml"/>\n</Types>')
        files["[Content_Types].xml"] = types.encode()
with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
    for name, data in files.items():
        z.writestr(zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0)), data, zipfile.ZIP_DEFLATED)   # fixed date
