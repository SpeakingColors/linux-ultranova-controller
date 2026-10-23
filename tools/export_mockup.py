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

TEXTJS = r"""(key) => {
  const root = document.querySelector('x-dc').lastElementChild; const rb = root.getBoundingClientRect();
  const z = parseFloat(root.style.zoom || '1') || 1;
  const rgba = c => { const m = c.match(/[\d.]+/g).map(Number); return [m[0], m[1], m[2], m.length > 3 ? m[3] : 1]; };
  const runs = []; const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT); let n;
  while ((n = walker.nextNode())) {
    const txt = n.nodeValue; if (!txt.trim()) continue; const p = n.parentElement;
    if (!p || ['SCRIPT', 'STYLE'].includes(p.tagName) || p.closest('svg') || p.closest('[data-knob]')) continue;
    const cs = getComputedStyle(p); if (cs.visibility === 'hidden' || cs.display === 'none') continue;
    let op = 1, ang = 0, e = p; while (e && e !== root.parentElement) { const c = getComputedStyle(e); op *= parseFloat(c.opacity);
      if (c.transform && c.transform !== 'none') { const m = c.transform.match(/matrix\(([^)]+)\)/); if (m) { const v = m[1].split(',').map(Number); ang += Math.atan2(v[1], v[0]) * 180 / Math.PI; } } e = e.parentElement; }
    const vertical = (cs.writingMode || '').startsWith('vertical'); if (vertical) ang += 90;
    const col = rgba(cs.color); col[3] *= op; if (col[3] <= 0.01) continue;
    const r = document.createRange(); const lines = []; let cur = null;
    for (let i = 0; i < txt.length; i++) {
      r.setStart(n, i); r.setEnd(n, i + 1); const rc = r.getClientRects()[0]; if (!rc || (rc.width === 0 && rc.height === 0)) continue;
      const key = vertical ? rc.left : rc.top; const tol = (parseFloat(cs.fontSize) * z) * 0.6;
      if (!cur || Math.abs(key - cur.key) > tol) { cur = { key, t: '', l: rc.left, tp: rc.top, r: rc.right, b: rc.bottom }; lines.push(cur); }
      cur.t += txt[i]; cur.l = Math.min(cur.l, rc.left); cur.tp = Math.min(cur.tp, rc.top); cur.r = Math.max(cur.r, rc.right); cur.b = Math.max(cur.b, rc.bottom);
    }
    const tt = cs.textTransform;
    lines.forEach(L => { let t = L.t.replace(/\s+/g, ' ').trim(); if (!t) return; if (tt === 'uppercase') t = t.toUpperCase(); else if (tt === 'lowercase') t = t.toLowerCase();
      runs.push({ t, x: +(L.l - rb.left).toFixed(2), y: +(L.tp - rb.top).toFixed(2), w: +(L.r - L.l).toFixed(2), h: +(L.b - L.tp).toFixed(2),
        size: +(parseFloat(cs.fontSize) * z).toFixed(2), weight: parseInt(cs.fontWeight) || 400, vt: /VT323/i.test(cs.fontFamily), col,
        ls: +((parseFloat(cs.letterSpacing) || 0) * z).toFixed(3), rot: Math.round(ang) }); });
  }
  const dials = [...document.querySelectorAll('[data-wavedial] svg[aria-label^="Waveform"]')].map(sv => { const b = sv.getBoundingClientRect(); const pa = sv.querySelector('path[stroke]');
    sv.style.visibility = 'hidden'; return { x: +(b.left - rb.left).toFixed(1), y: +(b.top - rb.top).toFixed(1), w: +b.width.toFixed(1), h: +b.height.toFixed(1), color: pa ? pa.getAttribute('stroke') : '#7EE3E8', label: sv.getAttribute('aria-label') }; });
  // the little waveform icon of the Oscillator 1..3 menu rows: hidden here, the plugin draws it from the real waveform
  const navIcons = []; document.querySelectorAll('nav a[href]').forEach(a => { const sp = a.querySelector('span'); const m = /^Oscillator (\d)$/.exec(sp ? sp.textContent.trim() : '');
    const sv = m && a.querySelector('svg'); if (!sv) return; const b = sv.getBoundingClientRect(); const pa = sv.querySelector('path[stroke]');
    navIcons.push({ n: +m[1], x: +(b.left - rb.left).toFixed(1), y: +(b.top - rb.top).toFixed(1), w: +b.width.toFixed(1), h: +b.height.toFixed(1), color: pa ? pa.getAttribute('stroke') : '#7EE3E8' }); sv.style.visibility = 'hidden'; });
  // the six little level bars of the Mixer menu row (every page): the plugin draws them from the mixer levels
  const navBars = []; document.querySelectorAll('nav a[href]').forEach(a => { const sp = a.querySelector('span'); if (!sp || sp.textContent.trim() !== 'Mixer') return;
    a.querySelectorAll('span[style*="width: 4px"]').forEach(b => { const r = b.getBoundingClientRect(); navBars.push({ x: +(r.left - rb.left).toFixed(1), y: +(r.top - rb.top).toFixed(1), w: +r.width.toFixed(1), h: +r.height.toFixed(1) }); b.style.visibility = 'hidden'; }); });
  // Oscillators page only: the mixer faders, the six envelope bars and the envelope graph become live controls
  const faders = [], bars = []; let env = null;
  if (key === 'osc' || key === 'filter') {
    const R = e => { const r = e.getBoundingClientRect(); return { x: +(r.left - rb.left).toFixed(1), y: +(r.top - rb.top).toFixed(1), w: +r.width.toFixed(1), h: +r.height.toFixed(1) }; };
    root.querySelectorAll('div[style*="width: 8px"][style*="height: 54px"]').forEach(f => { faders.push(R(f)); f.style.visibility = 'hidden'; });
    root.querySelectorAll('div[style*="height: 3px"][style*="background: #2C3B52"]').forEach(f => { bars.push(R(f)); f.style.visibility = 'hidden'; });
    const sv = root.querySelector('svg[aria-label="Envelope shape"]'); if (sv) { env = R(sv); sv.style.visibility = 'hidden'; }
  }
  // Filter page: the two round filter response windows are drawn by the plugin from type, frequency and resonance
  const filterDials = []; if (key === 'filter') root.querySelectorAll('svg[aria-label^="Filter "]').forEach(sv => { const r = sv.getBoundingClientRect(); const ring = sv.querySelector('circle[stroke]'); const pa = sv.querySelector('path[stroke]');
    filterDials.push({ x: +(r.left - rb.left).toFixed(1), y: +(r.top - rb.top).toFixed(1), w: +r.width.toFixed(1), h: +r.height.toFixed(1), ring: ring ? ring.getAttribute('stroke') : '#56B9E0', color: pa ? pa.getAttribute('stroke') : '#7EE3E8' }); sv.style.visibility = 'hidden'; });
  return { runs, dials, navIcons, navBars, faders, bars, env, filterDials };
}"""
HIDE_TEXT = "*{color:transparent !important;text-shadow:none !important;-webkit-text-fill-color:transparent !important} svg text{display:none !important}"

with sync_playwright() as p:
    b = p.chromium.launch(); pg = b.new_page(viewport={"width": 1500, "height": 900}, device_scale_factor=2)
    for key, name in PAGES.items():
        pg.goto("file://%s/docs/mockups/%s.dc.html" % (root, name)); pg.add_style_tag(content=FONTS)
        pg.wait_for_timeout(1500); pg.evaluate("document.fonts.ready"); pg.wait_for_timeout(500)
        info = pg.evaluate(JS, [760, 1348])
        tx = pg.evaluate(TEXTJS, key); info["runs"] = tx["runs"]; info["dials"] = tx["dials"]; info["navIcons"] = tx["navIcons"]; info["navBars"] = tx["navBars"]; info["faders"] = tx["faders"]; info["bars"] = tx["bars"]; info["env"] = tx["env"]; info["filterDials"] = tx["filterDials"]
        pg.add_style_tag(content=HIDE_TEXT); pg.wait_for_timeout(200)
        el = pg.query_selector("x-dc > :last-child")
        el.screenshot(path=os.path.join(root, "assets/ui/%s.png" % key))
        json.dump(info, open(os.path.join(root, "assets/ui/%s.json" % key), "w"), indent=1)
        print(key, info["w"], info["h"], len(info["knobs"]), "knobs", len(info["runs"]), "text runs", len(info["dials"]), "dials")
    b.close()
