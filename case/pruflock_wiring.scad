// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// Wiring diagram for the optional vibration motor: board seen from the back, as it is soldered.

// Matches the schematic in the main README: motor and 1N4148 between 3V and the collector,
// GPIO27 → 1 kΩ → base, 10 kΩ base → emitter, emitter → GND. Rendered by build.sh to
// preview/wiring.png; needs NopSCADlib (see pruflock_vitamins.scad).

use <pruflock_vitamins.scad>
$fn = 32;

ink = "#333";                        // label colour on the background
font = "Liberation Sans";
w = 0.8;                             // wire thickness

// Board back up: the case frame flipped about X, so +Y (the RST edge with 3V, 27, G) is at the bottom.
function flip(p) = [p.x, -p.y, len(p) > 2 ? -p[2] : 0];
function pin(i) = flip(tt_pin(1, i)) + [0, 0, 0.3];

module label(s, p, size = 1.6, col = ink, bold = false, halign = "center", valign = "center")
  color(col) translate([p.x, p.y, 6]) linear_extrude(0.02)
    text(s, size = size, font = str(font, bold ? ":style=Bold" : ""), halign = halign, valign = valign);

module wire(pts, col) color(col) lead(smooth_path(pts), d = w);

// ── board and its pin labels ──
xrot180() ttgo_t_display();
module xrot180() rotate([180, 0, 0]) children();
names = tt_pin_names(1);
used = ["3V", "27", "G"];
for (i = [0:11]) let(p = pin(i), on = i == 0 || i == 9 || i == 10)
  label(names[i], p + [0, 2.3], size = on ? 1.3 : 1.0, col = on ? "white" : "#8a8a8a", bold = on);
for (i = [0:11]) let(p = flip(tt_pin(-1, i)))
  label(tt_pin_names(-1)[i], p + [0, -2.3], size = 1.0, col = "#8a8a8a");

// ── battery: plugged into the socket, cable off the top ──
jst = flip(tt_jst_pos());
plug = [jst.x - 2.1 - 1.7, jst.y, 1.6];
color("white") translate(plug) cube([3.4, 4.4, 2.4], center = true);
off = flip(tt_pin(-1, 8));          // leave the board over pin 12, which isn't used
for (s = [-1, 1]) wire([plug + [-1.7, s * 0.625, 0], plug + [-5, s * 0.625, 0.8], [off.x + 2 - s * 0.625, 5, 4.5],
                        [off.x - s * 0.625, 13, 4.5], [off.x - s * 0.625, 18, 4.5]], s > 0 ? "red" : "black");
label("to the battery", [off.x, 20.5], size = 1.6);

// ── parts below the board ──
T  = [13, -22, 0];                   // transistor (flat face center)
Lc = [10.4, -29.5, 0.4];  Lb = [13, -29.5, 0.4];  Le = [15.6, -29.5, 0.4];   // leg ends
translate(T) to92();
for (l = [[-1.27, Lc], [0, Lb], [1.27, Le]])
  color("silver") lead([T + [l[0], -3, 0.4], T + [l[0], -4, 0.4], l[1]], d = 0.45);

R1 = [[0, -37, 0], [8, -37, 0]];     // 1 kΩ: GPIO27 → base
R2 = [[12, -43, 0], [20, -43, 0]];   // 10 kΩ: base → emitter
resistor(R1[0], R1[1], ["brown", "black", "red", "gold"]);
resistor(R2[0], R2[1], ["brown", "black", "orange", "gold"]);
label("1 kΩ", (R1[0] + R1[1])/2 + [0, 2.4]);
label("10 kΩ", (R2[0] + R2[1])/2 + [0, -2.6]);

M  = [-27, -33, 0];                  // coin motor
Np = [-19, -25, 0.6];  Nm = [-12, -31, 0.6];   // diode ends: cathode on the + lead, anode on the - lead
translate(M + [0, 0, 1]) coin_motor(8, 2, leads = 0);
diode(Np, Nm);
label("motor", M + [0, -6.5]);
label("1N4148", (Np + Nm)/2 + [3.6, 1.6], halign = "left");
label("band to 3V", (Np + Nm)/2 + [3.6, -0.4], size = 1.2, halign = "left");

// ── wires ──
red = "#d0312d";  blk = "#222";  org = "#e67e22";  blu = "royalblue";
wire([M + [0.4, 6, 0.8], M + [1, 8.5, 0.8], Np], red);                            // motor +
wire([Np, Np + [0.5, 5, 0.4], pin(0) + [0, -3, 0.3], pin(0)], red);              // → 3V
wire([M + [-0.4, 6, 0.8], M + [3, 4, 0.8], Nm], blu);                             // motor -
wire([Nm, Nm + [6, -2, 0.4], Lc + [-4, -2, 0.2], Lc], blu);                       // → collector
wire([pin(9), pin(9) + [0, -4, 0.3], R1[0] + [-1.5, 6, 0.6], R1[0] + [-1, 0, 0.6], R1[0] + [0, 0, 0.6]], org);   // GPIO27 → 1 kΩ
wire([R1[1] + [0, 0, 0.6], R1[1] + [2.5, 0.5, 0.6], Lb + [0, -3, 0.2], Lb], "#888");     // 1 kΩ → base
wire([R2[0] + [0, 0, 0.6], R2[0] + [0, 4, 0.6], Lb + [0, -4, 0.3], Lb], "#888");     // 10 kΩ → base
wire([R2[1] + [0, 0, 0.6], R2[1] + [0, 5, 0.6], Le + [1, -3, 0.2], Le], blk);         // 10 kΩ → emitter
wire([Le, Le + [3, 1, 0.4], [19, -17, 1], pin(10) + [2, -3, 0.8], pin(10)], blk);  // emitter → GND
for (p = [pin(0), pin(9), pin(10), Np, Nm, Lc, Lb, Le, R1[0] + [0, 0, 0.6], R1[1] + [0, 0, 0.6], R2[0] + [0, 0, 0.6], R2[1] + [0, 0, 0.6]])
  joint(p);

// ── notes ──
label("Back of the board: solder to the pins labelled in white.", [-4, -50], size = 1.5);
label("BC337 flat side up: legs C B E.  2N2222 and S8050 are E B C, check yours.", [-4, -53], size = 1.5);
label("Battery: red wire = +. The socket is keyed, so the plug only fits one way.", [-4, -56], size = 1.5);
