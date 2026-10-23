#!/usr/bin/env python3
"""Exports the mockup pages (docs/mockups/*.dc.html) for the real plugin window.
For every page it writes assets/ui/<page>.png (the artwork at 2x, with the live controls removed) and
assets/ui/<page>.json (where the live controls sit). The plugin draws knobs and values on top, so the
real window looks like the mockup. Needs: pip install playwright, and the fonts in assets/fonts.
Run: python3 tools/export_mockup.py"""
import json, os, sys
from playwright.sync_api import sync_playwright
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAGES = {"animate": "UltraNova", "osc": "UNova_Osc", "filter": "UNova_Filter", "mod": "UNova_Mod", "fx": "UNova_FX", "global": "UNova_Global", "lib": "UNova_Lib"}
FONTS = """
@font-face{font-family:'Exo 2';font-weight:300;src:url('file://%(r)s/assets/fonts/exo-2-latin-300-normal.ttf')}
@font-face{font-family:'Exo 2';font-weight:400;src:url('file://%(r)s/assets/fonts/exo-2-latin-400-normal.ttf')}
@font-face{font-family:'Exo 2';font-weight:600;src:url('file://%(r)s/assets/fonts/exo-2-latin-600-normal.ttf')}
@font-face{font-family:'VT323';font-weight:400;src:url('file://%(r)s/assets/fonts/vt323-latin-400-normal.ttf')}
""" % {"r": root}
JS = r"""([H, W]) => {
  const rootEl = document.querySelector('x-dc').lastElementChild;
  // every page gets the same window size: stretch the page to H and shrink a too wide page to W with zoom
  const nat = rootEl.getBoundingClientRect().width; const z = nat > W + 1 ? W / nat : 1;
  const HH = H / z; if (z < 1) rootEl.style.zoom = String(z);
  rootEl.style.height = HH + 'px'; [...rootEl.children].forEach(c => { if ((c.style.height || '').endsWith('px')) c.style.height = HH + 'px'; });
  rootEl.querySelectorAll('div').forEach(d => { if (['660px', '669px', '697px', '696px'].includes(d.style.height)) d.style.height = HH + 'px'; });
  const rb = rootEl.getBoundingClientRect();
  const rel = e => { const b = e.getBoundingClientRect(); return { x: +(b.left - rb.left).toFixed(1), y: +(b.top - rb.top).toFixed(1), w: +b.width.toFixed(1), h: +b.height.toFixed(1) }; };
  const out = { w: rb.width, h: rb.height, knobs: [], nav: [] };
  document.querySelectorAll('nav a[href]').forEach(a => { out.nav.push({ page: a.getAttribute('href').replace('.dc.html', ''), label: a.querySelector('span') ? a.querySelector('span').textContent.trim() : '', rect: rel(a) }); });
  out.buttons = [...document.querySelectorAll('nav button')].map(b => ({ text: b.textContent.trim(), label: b.getAttribute('aria-label') || '', rect: rel(b) }));
  document.querySelectorAll('[data-knob]').forEach((k, i) => {
    const wrap = k; const r = rel(wrap); let ang = null;
    wrap.querySelectorAll('div').forEach(d => { const m = /rotate\((-?[\d.]+)deg\)/.exec(d.style.transform || ''); if (m) ang = parseFloat(m[1]); });
    const rec = { id: k.getAttribute('data-knob'), rect: r, angle: ang, label: '', valueText: '' };
    // label: the nearest text above or below inside the same column
    const col = wrap.parentElement && wrap.parentElement.parentElement;
    const texts = col ? [...col.querySelectorAll('span,div')].filter(e => e.children.length === 0 && e.textContent.trim() && !wrap.contains(e)).map(e => ({ t: e.textContent.trim(), r: rel(e) })) : [];
    rec.near = texts.slice(0, 4);
    out.knobs.push(rec); wrap.style.visibility = 'hidden';
  });
  return out;
}"""
with sync_playwright() as p:
    b = p.chromium.launch(); pg = b.new_page(viewport={"width": 1500, "height": 900}, device_scale_factor=2)
    for key, name in PAGES.items():
        pg.goto("file://%s/docs/mockups/%s.dc.html" % (root, name)); pg.add_style_tag(content=FONTS)
        pg.wait_for_timeout(1500); pg.evaluate("document.fonts.ready"); pg.wait_for_timeout(500)
        info = pg.evaluate(JS, [760, 1348])
        el = pg.query_selector("x-dc > :last-child")
        el.screenshot(path=os.path.join(root, "assets/ui/%s.png" % key))
        json.dump(info, open(os.path.join(root, "assets/ui/%s.json" % key), "w"), indent=1)
        print(key, info["w"], info["h"], len(info["knobs"]), "knobs")
    b.close()
