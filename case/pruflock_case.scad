// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// Parametric 3D model of the case: shell, lid and clip.

// PrufLock badge — pocket case for the LilyGO TTGO T-Display v1.1
// with a 602158 LiPo pouch cell and coin vibration motor (TRU COMPONENTS TC-13324648).
//
// Three printed parts: shell (front + walls), lid (snaps in from the back), clip (screwed or
// glued to the lid). Render one part with  -D 'part="shell"'  etc. (see build.sh).
// With  -D with_clip=false  there is no clip; the lid carries a two-colour logo instead.
//
// Case frame: origin = board center. +X = USB end, +Y = RST edge, +Z = display side.
// The PCB's back face is Z = 0. All board positions come from LilyGO's STEP model
// (github.com/Xinyuan-LilyGO/TTGO-T-Display, 3d_file/T-Display.step), cross-checked against
// their own case STL. The pin headers must stay UNSOLDERED.

include <BOSL2/std.scad>
include <BOSL2/rounding.scad>
use <pruflock_vitamins.scad>         // board, battery, motor, screws for the exploded view (NopSCADlib)

part = "assembly";   // assembly | shell | lid | clip | print_shell | print_lid | print_clip | plate | section | illustration | explode
$fn = 48;
eps = 0.01;

// ── Board (TTGO T-Display v1.1) ──────────────────────────────────────────────
pcb_l = 51.48;  pcb_w = 25.19;  pcb_t = 1.01;
disp_top = 2.58;                     // display surface (Z)
disp_c = [-0.91, 0.08];  disp_sz = [32.0, 17.6];   // display module outline
usb_face = 27.24;                    // USB-C receptacle face (X), sticks out 1.5 mm
usb_w = 9.13;  usb_z = [1.01, 4.48]; // USB-C body
btn = [[22.45, -8.39], [22.30, 8.10]];   // tactile switches S1, S2 (top-facing)
btn_top = 3.05;                      // switch actuator top (Z)
rst_xz = [13.87, 2.04];              // side switch S3 on the +Y edge
back_max = 3.51;                     // tallest back-side part (U2), Z = -3.51

// ── Ordered parts ────────────────────────────────────────────────────────────
bat = [58, 21, 6];                   // 602158 LiPo pouch cell
coin_d = 8;  coin_t = 2.0;           // TC-13324648 coin motor, Ø8 × 2.0 mm ("0820")

// ── Case parameters (tweak after a test print) ───────────────────────────────
play     = 0.15;   // board ↔ case clearance in X and Y, per side
clr      = 0.30;   // battery ↔ lid clearance (Z)
parts_gap = 3.0;   // board back ↔ battery: room for the motor driver (BC337, resistors, wires).
                   // The battery is no longer clamped by U2; stick it to the lid with foam tape.
wall     = 1.6;    // side walls
front_t  = 1.2;    // front wall
lid_t    = 1.6;
lid_clr  = 0.10;   // lid plate ↔ shell opening, per side
r_corner = 4.8;    // outer vertical corner radius
r_cav    = 1.6;    // cavity corner radius: with the board pushed into a USB-end corner, its sharp
                   // corner sits at the arc's start when r_cav = cav_x[1] - usb_stop_x (1.70)
win      = [26.4, 16.6];             // display window at the inner face
win_c    = [-4.95, 0.08];            // window center (LilyGO's case uses the same)
disp_rest_h = 1.4;  disp_rest_w = 0.8;   // frame behind the window that the display rests on
tab_w    = 4.4;  tab_l = 8.8;  tab_slot = 0.6;  tab_t = 0.9;   // button flexures
tab_tip  = 2.6;                      // flexure free end past the switch center (toward USB)
nub_d    = 2.4;  nub_gap = 0.25;     // nub ↔ switch gap (pre-travel)
coin_clr = 0.5;                      // coin slot: width = coin_t + coin_clr
coin_fin = 0.8;  coin_fin_h = 6;     // fin that holds the coin against the far end wall
snap_bump = 0.5;                     // lid snap engagement
belt_t   = 4.5;                      // belt thickness
clip_gap = belt_t - 0.5;             // clip ↔ lid gap: 0.5 mm less than the belt, so the tongue clamps it
with_clip = true;                    // false: no clip and no screw holes; the logo is inlaid in the lid instead
logo_t    = 0.2;                     // logo inlay depth: one 0.2 mm layer in the second colour
logo_size = 4.2;                     // logo text size
logo_margin = 3;                     // logo distance from the lid's edges (bottom left, seen from outside)
logo_color = "#9a9a9a";  case_color = "#262626";   // colours in the 3MF and the previews

// ── Derived stack (Z) ────────────────────────────────────────────────────────
z_fi  = usb_z[1] + 0.32;             // front wall inner face   (4.80)
z_fo  = z_fi + front_t;              // front face              (6.00)
bat_top = -(back_max + 0.5 + parts_gap);  // battery top        (-7.01)
z_li  = bat_top - bat[2] - clr;      // lid inner face          (-13.31)
z_bo  = z_li - lid_t;                // back face               (-14.91)

// ── Derived plan (X/Y) ───────────────────────────────────────────────────────
stop_x = [-pcb_l/2 - 0.95, -pcb_l/2 - play];       // board end stops (far end)
usb_stop_x = pcb_l/2 + play;                       // board end stops (USB end), beside the USB opening
usb_stop_y = usb_w/2 + 1.1;
cav_x1 = usb_face + 0.35;
bat_x  = [cav_x1 - 0.3 - bat[0], cav_x1 - 0.3];
// The battery sets the length. The coin motor stands on edge, adhesive face on the far end
// wall, between the front wall and the battery, held by a fin on the board side.
cav_x  = [min(bat_x[0] - 0.3, stop_x[0] - coin_fin - coin_t - coin_clr), cav_x1];
slot_x = [cav_x[0], cav_x[0] + coin_t + coin_clr];
coin_c = [cav_x[0] + coin_t/2, 0, z_fi - coin_d/2];      // coin center, edge on the front wall
cav_y  = pcb_w/2 + play;
cav_l  = cav_x[1] - cav_x[0];
cav_cx = (cav_x[0] + cav_x[1]) / 2;
out_l  = cav_l + 2*wall;  out_w = 2*cav_y + 2*wall;

// lid features
tongues  = [[13.5, 23.5], [cav_x[0] + r_cav + 0.4, cav_x[0] + r_cav + 6.8]];   // keep clear of the corner radii
rail_x   = [tongues[1][1] + 1.1, 10.5];   // rails press the PCB edge strips (header holes) forward,
rail_y   = [11.3, 12.5];                  // starting just past the far-end tongue
// The +Y rail stops a half pitch short of the pins the motor is wired to: 3V (i = 0) at the
// far end, 27, G, 5V (i = 9–11) at the USB end, where the BC337 and its wires sit.
rail_x_wired = [tt_pin(1, 0).x + 1.27, tt_pin(1, 9).x - 1.27];
tongue_h = 4.0;  tongue_t = 1.2;
lid_fillet = 1.6;                    // concave fillet at the base of rails and tongues (battery side)
bump_z   = z_li + 3.2;
clip_x   = 18;                       // clip block center (X), at the USB end
clip_w   = 16;
screw_sp = 9;                        // distance between the two clip screws (Y)
clip_screws = [[clip_x, -screw_sp/2], [clip_x, screw_sp/2]];
screw_d  = 3;                        // M3 countersunk (DIN 965 / ISO 7046 head Ø5.5); M2.5 held too weakly
screw_hole = screw_d + 0.4;          // clearance in the lid
screw_csk  = 6.1;                    // countersink Ø at the lid's inner face (90°)
screw_pilot = 2.5;                   // pilot hole in the clip, the screw cuts its own thread

// front ribs (board stop on the PCB's front edge strips), avoiding S3, Q2, C11
ribs = [ [-1, -pcb_l/2 + 0.5, 10.5], [-1, 19.0, pcb_l/2 - 0.5],
         [ 1, -pcb_l/2 + 0.5, 10.8], [ 1, 17.0, 22.5] ];
rib_y0 = 11.4;

echo(str("Outer size: ", out_l, " x ", out_w, " x ", z_fo - z_bo, " mm (without clip)"));

// ─────────────────────────────────────────────────────────────────────────────

module rrect(size, r, h, center_xy=[0,0]) {
  translate([center_xy[0], center_xy[1], 0]) linear_extrude(h) rect(size, rounding=r);
}

module shell() {
  difference() {
    union() {
      difference() {
        // outer body: chamfered front edge (prints on the bed), rounded back rim
        translate([cav_cx, 0, z_bo])
          offset_sweep(rect([out_l, out_w], rounding=r_corner), height=z_fo - z_bo,
                       bottom=os_circle(r=0.8), top=os_chamfer(height=0.8));
        // cavity, open to the back
        translate([0, 0, z_bo - 1]) rrect([cav_l, 2*cav_y], r_cav, z_fi - z_bo + 1, [cav_cx, 0]);
      }
      // board front stops along both long edges
      for (r = ribs) let(s = r[0])
        translate([r[1], s > 0 ? rib_y0 : -cav_y - eps, pcb_t + 0.05])
          cube([r[2] - r[1], cav_y - rib_y0 + eps, z_fi - pcb_t - 0.05 + eps]);
      // board end stops at the far end, clear of the coin slot
      for (s = [-1, 1])
        translate([stop_x[0], s > 0 ? 10.6 : -cav_y - eps, -0.5])
          cube([stop_x[1] - stop_x[0], cav_y - 10.6 + eps, z_fi + 0.5 + eps]);
      // display rest: thin frame around the window opening, reaching down onto the display
      translate([win_c[0], win_c[1], z_fi - disp_rest_h])
        linear_extrude(disp_rest_h + eps) difference() {
          rect(win + 2*[disp_rest_w, disp_rest_w], rounding=0.8 + disp_rest_w);
          rect(win, rounding=0.8);
        }
      // board end stops at the USB end, either side of the USB opening
      for (s = [-1, 1])
        translate([usb_stop_x, s > 0 ? usb_stop_y : -cav_y - eps, -0.5])
          cube([cav_x[1] - usb_stop_x + eps, cav_y - usb_stop_y + eps, z_fi + 0.5 + eps]);
      // coin motor slot: fin hanging from the front wall, the far end wall is the other side
      translate([slot_x[1], -coin_d/2, z_fi - coin_fin_h]) cube([coin_fin, coin_d, coin_fin_h + eps]);
    }
    // display window, 45° bevel through the front wall
    hull() {
      translate([win_c[0], win_c[1], z_fi - eps]) linear_extrude(eps) rect(win, rounding=0.8);
      translate([win_c[0], win_c[1], z_fo]) linear_extrude(eps*2) rect(win + 2*[front_t, front_t], rounding=0.8 + front_t);
    }
    // button flexures: U-slot through the wall, tab thinned from inside
    for (b = btn) {
      tab0 = b[0] + tab_tip - tab_l;  // hinge end (toward the window)
      difference() {
        translate([tab0, b[1] - tab_w/2 - tab_slot, z_fi - eps])
          cube([tab_l + tab_slot, tab_w + 2*tab_slot, front_t + 2*eps]);
        translate([tab0 - eps, b[1] - tab_w/2, z_fi - 1]) cube([tab_l + eps, tab_w, front_t + 2]);
      }
      translate([tab0, b[1] - tab_w/2, z_fi - eps]) cube([tab_l, tab_w, front_t - tab_t + eps]);
      translate([b[0], b[1], z_fo - 0.3]) cylinder(d=3.0, h=1);       // finger dimple
    }
    // USB-C opening + recess for the plug's overmold
    translate([cav_x[1] - 0.2, 0, (usb_z[0] + usb_z[1])/2])
      xrot(90) yrot(90) linear_extrude(wall + 0.4) rect([usb_w + 0.8, usb_z[1] - usb_z[0] + 0.6], rounding=1.2);
    translate([cav_x[1] + wall - 0.8, 0, (usb_z[0] + usb_z[1])/2])
      xrot(90) yrot(90) linear_extrude(1) rect([12.8, 6.0], rounding=2.0);
    // RST pinhole (+Y wall)
    translate([rst_xz[0], cav_y - 0.2, rst_xz[1]]) ycyl(d=2.2, h=wall + 0.4, anchor=FRONT);
    // snap grooves for the lid tongues
    for (t = tongues, s = [-1, 1])
      translate([(t[0] + t[1])/2, s*cav_y, bump_z]) xrot(45) cube([t[1] - t[0] + 1.0, 1.0, 1.0], center=true);
    // pry notch at the far end of the back rim
    translate([cav_x[0] - wall - 1, -4, z_bo - 1]) cube([wall + 1.2, 8, 1.8]);
  }
  // button nubs on the tab undersides
  for (b = btn)
    translate([b[0], b[1], btn_top + nub_gap]) cylinder(d=nub_d, h=z_fi + front_t - tab_t - btn_top - nub_gap + eps);
}

// Concave fillet strip along X at the foot of a wall. `y_face` = the wall face it hugs; the fillet
// grows toward the board's center line (toward smaller |Y|), on top of the plate at z0.
module fillet_x(x0, x1, y_face, z0, r) {
  translate([x0, y_face, z0]) mirror([0, y_face < 0 ? 1 : 0, 0])
    rotate([90, 0, 90]) linear_extrude(x1 - x0)
      difference() {
        translate([-r, -eps]) square([r + eps, r + eps]);
        translate([-r, r]) circle(r=r, $fn=32);
      }
}

module lid() {
  difference() {
    union() {
      // plate, flush with the back rim
      translate([0, 0, z_bo]) rrect([cav_l - 2*lid_clr, 2*(cav_y - lid_clr)], r_cav - lid_clr, lid_t, [cav_cx, 0]);
      // rails: clamp the PCB edge strips against the front ribs, cage the battery
      for (s = [-1, 1]) let(rx = s > 0 ? rail_x_wired : rail_x) {
        translate([rx[0], s > 0 ? rail_y[0] : -rail_y[1], z_li - eps])
          cube([rx[1] - rx[0], rail_y[1] - rail_y[0], -0.05 - z_li + eps]);
        fillet_x(rx[0], rx[1], s*rail_y[0], z_li, lid_fillet);
      }
      // snap tongues with bumps
      for (t = tongues, s = [-1, 1]) {
        y_out = cav_y - lid_clr;
        translate([t[0], s > 0 ? y_out - tongue_t : -y_out, z_li - eps])
          cube([t[1] - t[0], tongue_t, tongue_h + eps]);
        fillet_x(t[0], t[1], s*(y_out - tongue_t), z_li, lid_fillet);
        translate([(t[0] + t[1])/2, s*y_out, bump_z])
          intersection() {
            xrot(45) cube([t[1] - t[0] - 1.0, snap_bump*1.414, snap_bump*1.414], center=true);
            translate([0, s*snap_bump/2, 0]) cube([t[1] - t[0], snap_bump, 2], center=true);
          }
      }
    }
    // clip screws: M3 countersunk from the inside
    if (with_clip) for (p = clip_screws) {
      translate([p[0], p[1], z_bo - eps]) cylinder(d=screw_hole, h=lid_t + 2*eps);
      let(h = (screw_csk - screw_hole)/2) translate([p[0], p[1], z_li - h]) cylinder(d1=screw_hole, d2=screw_csk, h=h + eps);
    }
    // without the clip: recess for the logo, filled by lid_logo()
    if (!with_clip) translate([0, 0, z_bo - eps]) linear_extrude(logo_t + eps) logo_2d();
  }
}

// The logo on the lid's outer face, in its bottom left corner as seen from outside (the USB end
// on the left). Seen from -Z, so it is mirrored to read correctly.
module logo_2d() translate([cav_x[1] - lid_clr - logo_margin, -(cav_y - lid_clr - logo_margin)]) mirror([1, 0])
  text("PrufLock", size=logo_size, font="Liberation Sans:style=Bold", halign="left", valign="baseline");

// The logo inlay, printed in the second colour as the lid's first layer.
module lid_logo() translate([0, 0, z_bo]) linear_extrude(logo_t) logo_2d();

// Whatever sits on the lid's outside: the clip, or the logo.
module lid_outside() {
  if (with_clip) clip();
  else color(logo_color) lid_logo();
}

module clip() {
  blk = [12, clip_w];
  blk_h = clip_gap + 1.8;
  tongue_len = 40;
  difference() {
    union() {
      // block, screwed to the lid
      translate([clip_x - blk[0]/2, -blk[1]/2, z_bo - blk_h]) cube([blk[0], blk[1], blk_h]);
      // spring tongue toward the far end, with a lead-in at the tip
      translate([clip_x + blk[0]/2 - tongue_len, -clip_w/2, z_bo - blk_h]) cube([tongue_len, clip_w, 1.8]);
      // lead-in: tip bends 20° away from the lid
      hull() {
        translate([clip_x + blk[0]/2 - tongue_len, -clip_w/2, z_bo - blk_h]) cube([eps, clip_w, 1.8]);
        translate([clip_x + blk[0]/2 - tongue_len, -clip_w/2, z_bo - blk_h + 1.8])
          yrot(-20) translate([-5, 0, -1.8]) cube([5, clip_w, 1.8]);
      }
    }
    for (p = clip_screws) translate([p[0], p[1], z_bo - blk_h - eps]) cylinder(d=screw_pilot, h=blk_h + 2*eps);
    // label on the outside of the tongue
    // text on a face seen from -Z must be mirrored to read correctly
    translate([clip_x - 16, 0, z_bo - blk_h - eps]) mirror([1, 0, 0]) linear_extrude(0.5)
      text("PrufLock", size=4.6, font="Liberation Sans:style=Bold", halign="center", valign="center");
  }
}

// Ghosts of the contents, for checking clearances.
module contents() {
  color("green", 0.8) translate([-pcb_l/2, -pcb_w/2, 0]) cube([pcb_l, pcb_w, pcb_t]);
  // display at its real (measured) height, not the STEP box (disp_top)
  color("#222", 0.9) translate([disp_c[0], disp_c[1], pcb_t]) linear_extrude(z_fi - disp_rest_h - pcb_t) rect(disp_sz);
  color("silver") translate([usb_face - 7.78, -usb_w/2, usb_z[0]]) cube([7.78, usb_w, usb_z[1] - usb_z[0]]);
  for (b = btn) color("white") translate([b[0], b[1], pcb_t]) linear_extrude(btn_top - pcb_t) rect([4, 2.8]);
  color("white") translate([rst_xz[0] - 2.3, 9.8, pcb_t]) cube([4.6, 2.3, 2.03]);
  color("#555") translate([-6, -10.5, -back_max]) cube([12, 6, back_max]);          // U2 (tallest back part)
  color("steelblue", 0.7) translate([bat_x[0], -bat[1]/2, bat_top - bat[2]]) cube(bat);
  color("gray") translate(coin_c) yrot(90) cylinder(d=coin_d, h=coin_t, center=true);
}

section_y = 0;
module halfcut() intersection() { children(); translate([-60, section_y - 40, -40]) cube([120, 40, 80]); }

module assembly() {
  shell();
  lid();
  lid_outside();
  contents();
}

// ── Illustration: the inspection screen on the display ───────────────────────
// Drawn in the firmware's own pixel coordinates (240 × 135, y down, see Display::showInspection in
// src/display.cpp) with the firmware's font, then scaled onto the display's active area.
use <../tools/fonts/LiberationSans-Bold.ttf>
scr_px = 0.1035;                     // mm per pixel: 1.14" ST7789, 240 × 135 px
scr_font = "Liberation Sans:style=Bold";
COL_ORANGE = "#FFA500";  COL_GREEN = "#00FF00";  COL_GREY = "#D3D3D3";

// TFT_eSPI font 7 (7-segment): 48 px high, 32 px advance; segments a–g, y down.
seg7 = [
  [[6,3],[8,1],[23,1],[25,3],[23,5.5],[8,5.5]],             // a top
  [[23,7.5],[26,5],[29,7.5],[29,19.5],[26,22],[23,19.5]],   // b upper right
  [[23,27.5],[26,25],[29,27.5],[29,39.5],[26,42],[23,39.5]],// c lower right
  [[6,44.5],[8,42],[23,42],[25,44.5],[23,47],[8,47]],       // d bottom
  [[2,27.5],[5,25],[8,27.5],[8,39.5],[5,42],[2,39.5]],      // e lower left
  [[2,7.5],[5,5],[8,7.5],[8,19.5],[5,22],[2,19.5]],         // f upper left
  [[4.5,23.5],[7,21],[24,21],[26.5,23.5],[24,26],[7,26]],   // g middle
];
seg7_digits = ["abcdef", "bc", "abdeg", "abcdg", "bcfg", "acdfg", "acdefg", "abc", "abcdefg", "abcdfg"];

module seg7_digit(d) {
  for (s = seg7_digits[d]) polygon(seg7[ord(s) - ord("a")]);
}

// Screen contents in pixel coordinates, y down. Each element is extruded inside its own color()
// (a colour on 2D shapes is lost when they are extruded together). Text size ≈ cap height × 1.4.
module screen_px() {
  module px(col) color(col) linear_extrude(0.1) children();
  module label(s, x, y, size, col, halign="center", valign="center")
    px(col) translate([x, y]) mirror([0, 1]) text(s, size=size, font=scr_font, halign=halign, valign=valign);
  // top row: network name, signal bars, battery
  label("HomeNet", 4, 3, 11.5, COL_GREY, halign="left", valign="top");
  label("87%", 236, 3, 11.5, COL_GREY, halign="right", valign="top");
  for (i = [0:3]) px(COL_GREEN) translate([180 + i * 6, 17 - (4 + 4 * i)]) square([4, 4 + 4 * i]);
  // label row and code
  label("INSPECTION · due 15:30", 120, 30, 11.5, COL_ORANGE);
  code = [4, 5, 3, 4, 7];
  for (i = [0:len(code) - 1]) px("white") translate([120 - len(code) * 16 + i * 32, 72 - 24]) seg7_digit(code[i]);
  // time left
  label("58 min left", 120, 116, 17.5, COL_GREEN);
}

module screen() {
  z = z_fi - disp_rest_h;            // top of the display, as in contents()
  translate([win_c[0], win_c[1], z]) {
    // black glass across the whole window, like the display's border
    color("black") translate([0, 0, 0.01]) linear_extrude(0.01) square(win + [2, 2], center=true);
    translate([0, 0, 0.03]) scale([scr_px, scr_px, 0.1]) translate([-120, 67.5]) mirror([0, 1, 0]) screen_px();
  }
}

module illustration() {
  color(case_color) { shell(); lid(); if (with_clip) clip(); }
  if (!with_clip) color(logo_color) lid_logo();
  screen();
}

// ── Exploded view: assembly order, bottom to top ─────────────────────────────
// Drawn in the case frame with the parts pulled apart toward the back (-Z), then flipped so the
// shell lies front face down with its cavity open, as on the bench.
explode_gap = 15;                    // distance between the levels
jst_xy = [18, 0];                    // battery socket on the board's back (tt_jst in pruflock_vitamins.scad)

// Pull `children` apart by `d` (toward the back), with a guide line from `at` (none if undef).
// `nested` lets pulls inside the children move too (relative to this part).
module pull(d, at, nested = false) translate(is_undef(at) ? [0, 0, 0] : at)
  exploded([0, 0, -d], show_line = !is_undef(at), nested = nested) translate(is_undef(at) ? [0, 0, 0] : -at) children();

module explode_view() let($explode = 1) xrot(180) {
  g = explode_gap;
  bat_c = [(bat_x[0] + bat_x[1])/2, 0, bat_top - bat[2]];
  jst_open = [jst_xy.x - 2.1, jst_xy.y, -1.6];       // mouth of the board's battery socket
  pcm  = [bat_x[0], 0, bat_c.z + bat[2]/2 - 4*g];    // where the battery leads leave the PCM
  plug = jst_open + [-3.4, 0, -3*g];                 // back face of the plug, pushed into the socket
  color("#3a3f45") shell();
  // coin motor: adhesive face against the far end wall
  pull(2*g, coin_c) translate(coin_c) yrot(-90) coin_motor(coin_d, coin_t);
  pull(3*g, [usb_face - 2, 0, (usb_z[0] + usb_z[1])/2]) ttgo_t_display();
  // battery, PCM at the far end; its cable runs straight into the board's socket
  pull(4*g) translate(bat_c) lipo_602158(bat, pcm_end = -1);
  // slack loop out past the far end and along the side
  battery_cable([pcm, pcm + [-3, -2, 0], pcm + [-6, -8, 4], [bat_x[0] + 2, -cav_y - 6, plug.z - 7],
                 [plug.x - 14, -cav_y - 4, plug.z - 6], [plug.x - 6, plug.y - 2, plug.z - 5],
                 plug + [-2, 0, -1], plug]);
  // lid with its screws (from the inside) and the clip (outside), lines through the screw holes
  pull(6*g, nested = true) {
    color("#4a5057") lid();
    if (with_clip) {
      for (p = clip_screws) pull(-0.8*g, [p.x, p.y, z_li]) translate([p.x, p.y, z_li]) clip_screw();
      for (p = clip_screws) pull(g, [p.x, p.y, z_bo]) if (p == clip_screws[0]) color("#5a6067") clip();
    } else color(logo_color) lid_logo();
  }
}

// ── Print orientations ───────────────────────────────────────────────────────
module print_shell() translate([0, 0, z_fo]) xrot(180) shell();                     // front face down
module print_lid()   translate([0, 0, -z_bo]) lid();                                 // outer face down
module print_logo()  translate([0, 0, -z_bo]) lid_logo();                            // first layer of the lid
module print_clip()  translate([0, 0, clip_w/2]) xrot(-90) translate([-clip_x, 0, -z_bo]) clip(); // on its side
plate_dy = out_w + 6;                // distance between the parts on the plate (Y)

if (part == "assembly") assembly();
else if (part == "explode") explode_view();
else if (part == "illustration") illustration();
else if (part == "shell") shell();
else if (part == "lid") lid();
else if (part == "clip") { if (with_clip) clip(); else echo("with_clip = false: no clip"); }
else if (part == "section") {   // cut at Y = section_y, each part keeps its color
  color("tan") halfcut() shell();
  color("peru") halfcut() lid();
  color("sienna") halfcut() lid_outside();
  halfcut() contents();
}
// Print orientations: flat faces on the bed, no supports.
else if (part == "print_shell") print_shell();
else if (part == "print_lid") print_lid();
else if (part == "print_clip") { if (with_clip) print_clip(); else echo("with_clip = false: no clip"); }
// All parts side by side for the 3MFs, one object each (--enable=lazy-union, see build.sh). The
// order (shell, lid, logo or clip) is what build.sh's 3mf_parts.py names them by.
else if (part == "plate") {
  color(case_color) print_shell();
  color(case_color) translate([0, -plate_dy, 0]) print_lid();
  if (with_clip) color(case_color) translate([0, plate_dy, 0]) print_clip();
  else color(logo_color) translate([0, -plate_dy, 0]) print_logo();
}
