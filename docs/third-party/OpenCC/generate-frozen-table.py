# SPDX-License-Identifier: GPL-3.0-only
# Relaxer verification helper, not upstream OpenCC code.
# Reproduces legacy bytes for proof; use notice-marked-opencc-st.inc for publication.
import argparse, pathlib
p=argparse.ArgumentParser();p.add_argument("dictionary",type=pathlib.Path);p.add_argument("output",type=pathlib.Path);a=p.parse_args()
pairs=[]
for line in a.dictionary.read_text(encoding="utf-8-sig").splitlines():
 if line and not line.startswith("#"):
  source,targets=line.split("\t");target=targets.split()[0]
  if source!=target:pairs.append((source,target))
body="static const char16_t *const kTradMap[] = {\n"+"".join('    u"'+s+'", u"'+t+'",\n' for s,t in pairs)+"};\n"
a.output.write_bytes(body.encode("utf-8"))
