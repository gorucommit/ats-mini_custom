#ifndef WATERFALL_DECODER_SCRIPT_H
#define WATERFALL_DECODER_SCRIPT_H

#define WATERFALL_DECODER_SCRIPT \
"#!/usr/bin/env python3\n" \
"# Decode ATS-Mini waterfall .raw to PNG. Usage: python waterfall_decoder.py [input.raw] [output.png]\n" \
"import argparse, struct, sys\n" \
"try: import numpy as np\n" \
"except ImportError: np = None\n" \
"try: from PIL import Image, ImageDraw, ImageFont\n" \
"except ImportError: Image = ImageDraw = ImageFont = None\n" \
"MAGIC = 0x31465757\n" \
"def load_raw(path):\n" \
"  with open(path, \"rb\") as f: data = f.read()\n" \
"  if len(data) < 24: raise ValueError(\"File too short\")\n" \
"  magic,bins,rows = struct.unpack(\"<IHH\", data[0:8])\n" \
"  if magic != MAGIC: raise ValueError(\"Bad magic\")\n" \
"  min_f,max_f,step_f,interval = struct.unpack(\"<IIII\", data[8:24])\n" \
"  payload = data[24:]; need = rows * bins\n" \
"  if len(payload) < need: raise ValueError(\"Payload short\")\n" \
"  return {\"bins\":bins,\"rows\":rows,\"min_freq_khz\":min_f,\"max_freq_khz\":max_f,\"step_khz\":step_f,\"interval_ms\":interval,\"pixels\":payload[:need]}\n" \
"def colormap(v):\n" \
"  if v <= 0: return (0,0,128)\n" \
"  if v >= 1: return (128,0,0)\n" \
"  v *= 4\n" \
"  if v <= 1: return (0, 0, 128 + int(127*v))\n" \
"  if v <= 2: return (0, int(255*(v-1)), 255)\n" \
"  if v <= 3: return (int(255*(v-2)), 255, int(255*(3-v)))\n" \
"  return (255, int(255*(4-v)), 0)\n" \
"def decode(info):\n" \
"  bins,rows,raw = info[\"bins\"],info[\"rows\"],info[\"pixels\"]\n" \
"  if np and Image:\n" \
"    arr = np.frombuffer(raw, dtype=np.uint8).reshape((rows, bins))\n" \
"    lo, hi = arr.min(), arr.max(); span = (hi - lo) or 1\n" \
"    norm = (arr.astype(np.float32) - lo) / span\n" \
"    r = np.clip(1.5 - 4*np.abs(norm - 0.75), 0, 1)\n" \
"    g = np.clip(1.5 - 4*np.abs(norm - 0.5), 0, 1)\n" \
"    b = np.clip(1.5 - 4*np.abs(norm - 0.25), 0, 1)\n" \
"    return Image.fromarray((np.stack([r,g,b], axis=-1)*255).astype(np.uint8))\n" \
"  if not Image: raise RuntimeError(\"PIL required\")\n" \
"  lo, hi = min(raw), max(raw); span = (hi - lo) or 1\n" \
"  img = Image.new(\"RGB\", (bins, rows)); pix = img.load()\n" \
"  for y in range(rows):\n" \
"    for x in range(bins): pix[x,y] = colormap((raw[y*bins+x]-lo)/span)\n" \
"  return img\n" \
"def add_axis(img, info):\n" \
"  if not ImageDraw or not Image: return img\n" \
"  bins,min_f,max_f,step_f = info[\"bins\"],info[\"min_freq_khz\"],info[\"max_freq_khz\"],info[\"step_khz\"]\n" \
"  w,h=img.size; ax=22; out=Image.new(\"RGB\",(w,h+ax),(30,30,30)); out.paste(img,(0,0))\n" \
"  d=ImageDraw.Draw(out)\n" \
"  try: f=ImageFont.truetype(\"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf\",10)\n" \
"  except: f=ImageFont.load_default()\n" \
"  n=min(8,max(3,(max_f-min_f)//max(step_f,1)+1)) if max_f>min_f else 1\n" \
"  for i in range(n):\n" \
"    fk=min_f+(max_f-min_f)*i//(n-1) if n>1 else min_f\n" \
"    x=int((fk-min_f)/max(step_f,1)/max(bins-1,1)*(w-1)) if bins>1 else 0\n" \
"    x=max(0,min(x,w-1))\n" \
"    d.line([(x,h),(x,h+ax)],fill=(160,160,160))\n" \
"    lb=f\"{fk/1000:.1f} MHz\" if fk>=10000 else f\"{fk} kHz\"\n" \
"    d.text((x-20,h+2),lb,fill=(200,200,200),font=f)\n" \
"  return out\n" \
"def main():\n" \
"  ap = argparse.ArgumentParser()\n" \
"  ap.add_argument(\"input\", nargs=\"?\", default=\"waterfall.raw\")\n" \
"  ap.add_argument(\"output\", nargs=\"?\", default=None)\n" \
"  args = ap.parse_args()\n" \
"  out = args.output or (args.input.rsplit(\".\",1)[0]+\".png\" if \".\" in args.input else args.input+\".png\")\n" \
"  info = load_raw(args.input)\n" \
"  add_axis(decode(info), info).save(out); print(\"Saved\", out)\n" \
"if __name__ == \"__main__\": main()\n"

#endif
