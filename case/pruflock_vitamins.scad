// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// Bought-in parts (vitamins) for the exploded view, built from NopSCADlib.

// NopSCADlib has no TTGO T-Display, pouch LiPo or coin vibration motor, so they are put together
// here from its PCB components and screws. Pulled into pruflock_case.scad with `use`,
// which keeps NopSCADlib's names apart from BOSL2's.
//
// Same frame as the case: origin = board center, +X = USB end, +Y = RST edge, PCB back face
// at Z = 0. The board outline, USB-C, switches and U2 come from LilyGO's STEP model (as in
// pruflock_case.scad); the other back-side parts are only illustrative.

include <NopSCADlib/core.scad>
include <NopSCADlib/vitamins/pcb.scad>
include <NopSCADlib/vitamins/smds.scad>
include <NopSCADlib/vitamins/screws.scad>

// ── TTGO T-Display v1.1 ──────────────────────────────────────────────────────
tt_l = 51.48;  tt_w = 25.19;  tt_t = 1.01;
tt_disp_h = 2.39;                    // display module height above the PCB (measured, not the STEP box)

// pcb() wants positions from the board's corner; c() converts from the case frame
function c(p) = [p.x + tt_l/2, p.y + tt_w/2];
function comp(p, rot, type, a=undef, b=undef, d=undef, e=undef, f=undef) =
  let(q = c(p)) [q.x, q.y, rot, type, a, b, d, e, f];

// Header pins, from LilyGO's pinmap (image/pinmap.jpg in their T-Display repo): two rows of 12,
// numbered i = 0 (far end) … 11 (USB end). Row +1 is the RST edge (+Y), row -1 the other one.
// The row positions along X are measured from their photo of the board's back.
tt_hdr_x = -4.7;                     // center of the rows (X)
function tt_pin(row, i) = [tt_hdr_x + (i - 5.5) * 2.54, row * 11.43];
function tt_pin_names(row) = row > 0
  ? ["3V", "36", "37", "38", "39", "32", "33", "25", "26", "27", "G", "5V"]
  : ["G", "G", "21", "22", "17", "2", "15", "13", "12", "G", "G", "3V"];

tt_colour = "#1f4d2b";               // dark green: the real board is almost black, but green prints better

function ttgo_pcb(colour = tt_colour) = pcb("TTGO_T_Display", "LilyGO TTGO T-Display v1.1", [tt_l, tt_w, tt_t],
  corner_r = 1.0, hole_d = 1.0, land_d = 1.7, colour = colour,
  holes = [for (i = [0:11], row = [-1, 1]) c(tt_pin(row, i))],   // 2 × 12 header holes
  components = [
    // front: display, buttons, USB-C, reset
    comp([-0.91, 0.08], 0, "chip", 32.0, 17.6, tt_disp_h, grey(22)),
    comp([-4.95, 0.08], 0, "chip", 24.9, 14.0, tt_disp_h + 0.02, "black"),       // active area
    comp([22.45, -8.39], 0, "chip", 4.0, 2.8, 1.5, silver),
    comp([22.45, -8.39], 0, "chip", 1.6, 1.0, 2.04, grey(15)),
    comp([22.30,  8.10], 0, "chip", 4.0, 2.8, 1.5, silver),
    comp([22.30,  8.10], 0, "chip", 1.6, 1.0, 2.04, grey(15)),
    comp([27.24 - 7.35/2, 0], 0, "usb_C"),
    comp([13.87, 10.95], 0, "chip", 4.6, 2.3, 2.03, "white"),
    // back: U2 (the tallest part), ESP32, flash, USB-UART, battery socket, a few passives
    comp([0, -7.5], 0, "-chip", 12, 6, 3.51, grey(30)),
    comp([-12.5, 3.5], 0, "-chip", 5, 5, 0.9, grey(20)),
    comp([-3.5, 5.0], 90, "-chip", 5, 4, 1.5, grey(20)),
    comp([10, -3.5], 0, "-chip", 4, 4, 0.9, grey(20)),
    for (i = [0:3]) comp([5 + i * 1.6, 2.5], 90, "-smd_res", RES0603, "103"),
    for (i = [0:2]) comp([-19 + i * 1.6, -4], 90, "-smd_cap", CAP0603, 0.8),
    comp([-12, -7], 0, "-text", 9, 2.2, "T-Display", "Liberation Sans:style=Bold"),
  ]);

tt_jst = [18, 0];                    // battery socket on the back, opening toward -X (away from USB)
function tt_jst_pos() = tt_jst;      // for files that `use` this one (variables don't carry over)

module ttgo_t_display(colour = tt_colour) {
  pcb(ttgo_pcb(colour));
  translate([tt_jst.x, tt_jst.y, 0]) mirror([0, 0, 1]) jst_socket();
}

// ── JST 1.25 mm 2-pin SMD socket, horizontal: on top of Z = 0, opening toward -X ─
jst_body = [4.2, 5.2, 3.2];
jst_plug_size = [3.4, 4.4, 2.4];
module jst_socket() {
  color("white") difference() {
    translate([-jst_body.x/2, -jst_body.y/2, 0]) cube(jst_body);
    translate([-jst_body.x/2 - eps, -jst_plug_size.y/2 - 0.1, 0.4]) cube([jst_plug_size.x + 0.1, jst_plug_size.y + 0.2, jst_plug_size.z + 0.1]);
  }
  for (y = [-0.625, 0.625]) color(silver) translate([-jst_body.x/2 + 0.6, y - 0.2, 1.4]) cube([jst_plug_size.x - 0.4, 0.4, 0.4]);
  for (y = [-1, 1]) color(silver) translate([-0.8, y * (jst_body.y/2 + 0.4) - 0.4, 0]) cube([1.6, 0.8, 0.15]);   // solder tabs
}

// The plug, centered, wires leaving at -X.
module jst_plug() color("white") cube(jst_plug_size, center = true);

// Two-wire cable (red = +, black = -) along a smooth curve through `pts`, ending in a JST plug
// whose back face is at the last point, plugging in toward +X.
module battery_cable(pts) {
  path = smooth_path(pts);
  for (s = [-1, 1]) color(s > 0 ? "red" : "black")
    lead([for (p = path) p + [0, s * 0.625, 0]], d = 0.9);
  translate(pts[len(pts) - 1] + [jst_plug_size.x/2, 0, 0]) jst_plug();
}

// ── 602158 LiPo pouch cell (6.0 × 21 × 58 mm), 3.7 V ───────────────────────────
// Centered in X/Y, bottom face at Z = 0, protection circuit (PCM) at the -X end (+X with pcm_end = 1).
// The leads leave the PCM; draw them with battery_cable().
module lipo_602158(size = [58, 21, 6], pcm_end = -1) {
  pcm = 4;                           // folded protection board
  body = [size.x - pcm, size.y, size.z];
  bx = -pcm_end * pcm/2;             // body center (X)
  color("#c9ced3") translate([bx, 0, 0]) rounded_rectangle(body, 1.5, center = false);
  // label on the lid side (Z = 0), mirrored so it reads from outside
  color("steelblue") translate([bx, 0, -0.05]) linear_extrude(0.05) rounded_square([body.x - 6, body.y - 3], 1);
  color("white") translate([bx, 0, -0.07]) linear_extrude(0.02) mirror([1, 0]) {
    translate([0, 2.5]) resize([22, 3.2], auto = true) text("602158", font = "Liberation Sans:style=Bold", halign = "center", valign = "center");
    translate([0, -2.5]) resize([17, 2.4], auto = true) text("LiPo  3.7 V", font = "Liberation Sans", halign = "center", valign = "center");
  }
  color("#d98c1f") translate([pcm_end * (size.x - pcm)/2, 0, size.z/2])
    cube([pcm + 1, size.y - 2, 4], center = true);                     // Kapton-wrapped PCM
}

// Catmull-Rom spline through all of `pts`, `n` steps per span: a cable that bends smoothly.
function smooth_path(pts, n = 12) =
  let(m = len(pts), p = function(i) pts[max(0, min(m - 1, i))])
  concat([for (i = [0 : m - 2], k = [0 : n - 1])
            let(t = k / n, p0 = p(i - 1), p1 = p(i), p2 = p(i + 1), p3 = p(i + 2))
              0.5 * (2*p1 + (p2 - p0)*t + (2*p0 - 5*p1 + 4*p2 - p3)*t*t + (3*p1 - p0 - 3*p2 + p3)*t*t*t)],
         [pts[m - 1]]);

// Wire through a list of points, 1 mm thick.
module lead(pts, d = 1.0)
  for (i = [0 : len(pts) - 2])
    hull() { translate(pts[i]) sphere(d = d, $fn = 12); translate(pts[i + 1]) sphere(d = d, $fn = 12); }

// ── TRU COMPONENTS TC-13324648 coin vibration motor, Ø8 × 2 mm ("0820") ─────
// Disc centered on the origin, axis along Z, adhesive (black) face at +Z, solder tab and leads at +Y.
module coin_motor(d = 8, t = 2.0, leads = 12) {
  color(silver) translate([0, 0, -t/2]) cylinder(d = d, h = t - 0.1);
  color(grey(12)) translate([0, 0, t/2 - 0.1]) cylinder(d = d - 0.3, h = 0.1);    // adhesive pad
  color("#c8a24a") translate([-1.2, d/2 - 0.6, -t/2]) cube([2.4, 1.6, 0.4]);      // solder tab
  for (s = [-1, 1]) color(s > 0 ? "red" : "royalblue")
    lead([[s * 0.5, d/2 + 0.6, -t/2 + 0.6], [s * 0.5, d/2 + 2.5, -t/2 + 0.6], [s * 0.8, d/2 + 2.5 + leads, -t/2 - 2]], d = 0.7);
}

// ── Loose parts for the wiring diagram: lying flat on Z = 0, drawn between two points ─────
// Axial part between a and b: leads, then a body of length `l` in the middle.
module axial(a, b, l, d, colour) {
  v = b - a;  len = norm(v);  ang = atan2(v.y, v.x);
  translate(a) rotate(ang) {
    color(silver) translate([0, 0, d/2]) rotate([0, 90, 0]) cylinder(d = 0.5, h = len, $fn = 12);
    color(colour) translate([(len - l)/2, 0, d/2]) rotate([0, 90, 0]) cylinder(d = d, h = l, $fn = 24);
    children();                      // decorations, in the part's frame (X along the part, body from (len - l)/2)
  }
}

// Resistor with colour bands (4-band code, e.g. ["brown", "black", "red", "gold"] = 1 kΩ).
module resistor(a, b, bands, l = 3.6, d = 1.8)
  axial(a, b, l, d, "#d8c08f") let(len = norm(b - a))
    for (i = [0 : len(bands) - 1])
      color(bands[i]) translate([(len - l)/2 + 0.5 + i * (i < 3 ? 0.6 : 0.9), 0, d/2]) rotate([0, 90, 0]) cylinder(d = d + 0.05, h = 0.35, $fn = 24);

// 1N4148: glass body, black band at the cathode end `k`.
module diode(k, a, l = 3.6, d = 1.8)
  axial(k, a, l, d, "#c0392b") let(len = norm(a - k))
    color(grey(10)) translate([(len - l)/2 + 0.3, 0, d/2]) rotate([0, 90, 0]) cylinder(d = d + 0.05, h = 0.6, $fn = 24);

// TO-92 transistor lying flat side up, legs toward -Y: center of the flat face at the origin.
// Leg order seen from the flat side, left to right, is `pins` (BC337: C B E).
module to92(name = "BC337", pins = ["C", "B", "E"], leg = 1.5) {
  w = 4.6;  h = 4.6;
  color(grey(12)) linear_extrude(2.2)
    polygon([[-w/2, -h/2], [w/2, -h/2], [w/2, h/2 - 1], [w/2 - 1, h/2], [-w/2 + 1, h/2], [-w/2, h/2 - 1]]);
  color("white") translate([0, 0.8, 2.21]) linear_extrude(0.02)
    text(name, size = 0.95, font = "Liberation Sans:style=Bold", halign = "center", valign = "center");
  for (k = [0:2]) color("white") translate([(k - 1) * 1.27, -1.3, 2.21]) linear_extrude(0.02)
    text(pins[k], size = 0.85, font = "Liberation Sans:style=Bold", halign = "center", valign = "center");
  for (k = [0:2]) color(silver) translate([(k - 1) * 1.27, -h/2 - leg/2, 0.4]) cube([0.45, leg, 0.4], center = true);
}

// Solder joint.
module joint(p) color("#b8bcc0") translate(p) sphere(d = 1.3, $fn = 16);

// ── M3 × 6 countersunk screw for the clip, head flush at Z = 0, pointing to -Z ─
module clip_screw() screw(M3_cs_cap_screw, 6);

// NopSCADlib's explode(): moves the children by `d × $explode`, with a guide line.
// `nested` lets explode() calls inside the children move them too.
module exploded(d, show_line = true, nested = false) explode(d, nested, show_line = show_line) children();
